#include "WeatherClient.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <map>
#include <vector>

WeatherClient::WeatherClient() {
    // Constructor - nothing to initialize
}

bool WeatherClient::fetchForecast(float latitude, float longitude, WeatherForecast& forecast) {
    // Reset forecast
    forecast.valid = false;
    forecast.dayCount = 0;
    for (int i = 0; i < 4; i++) {
        forecast.days[i].valid = false;
        forecast.days[i].tempMin = 999.0f;
        forecast.days[i].tempMax = -999.0f;
        forecast.days[i].humidity = 0.0f;
        memset(forecast.days[i].date, 0, sizeof(forecast.days[i].date));
        memset(forecast.days[i].symbolCode, 0, sizeof(forecast.days[i].symbolCode));
    }

    Serial.println("[Weather] Fetching forecast...");
    Serial.printf("[Weather] Location: %.4f, %.4f\n", latitude, longitude);
    Serial.printf("[Weather] Free heap before: %u bytes\n", ESP.getFreeHeap());

    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Weather] WiFi not connected");
        return false;
    }

    // Construct API URL with coordinates
    char url[256];
    snprintf(url, sizeof(url), "%s?lat=%.4f&lon=%.4f", API_URL, latitude, longitude);

    WiFiClientSecure client;
    client.setInsecure();  // Skip TLS verification

    HTTPClient http;
    http.setTimeout(TIMEOUT_MS);

    if (!http.begin(client, url)) {
        Serial.println("[Weather] Failed to initialize HTTP client");
        return false;
    }

    // Set required User-Agent header
    http.addHeader("User-Agent", USER_AGENT);

    Serial.printf("[Weather] GET %s\n", url);

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[Weather] HTTP error: %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    Serial.printf("[Weather] Response length: %d bytes\n", payload.length());
    Serial.printf("[Weather] Free heap after HTTP: %u bytes\n", ESP.getFreeHeap());

    // Parse the JSON response
    bool success = parseResponse(payload, forecast);

    if (success) {
        Serial.printf("[Weather] Success! Parsed %d days\n", forecast.dayCount);
        for (int i = 0; i < forecast.dayCount; i++) {
            Serial.printf("[Weather] Day %d: %s | %.1f-%.1f°C | %.0f%% | %s\n",
                          i,
                          forecast.days[i].date,
                          forecast.days[i].tempMin,
                          forecast.days[i].tempMax,
                          forecast.days[i].humidity,
                          forecast.days[i].symbolCode);
        }
    } else {
        Serial.println("[Weather] Failed to parse response");
    }

    return success;
}

bool WeatherClient::parseResponse(const String& json, WeatherForecast& forecast) {
    // MET Norway API returns large JSON (~40KB)
    // Use filter to only extract needed fields for memory efficiency
    StaticJsonDocument<256> filter;
    filter["properties"]["timeseries"][0]["time"] = true;
    filter["properties"]["timeseries"][0]["data"]["instant"]["details"]["air_temperature"] = true;
    filter["properties"]["timeseries"][0]["data"]["instant"]["details"]["relative_humidity"] = true;
    filter["properties"]["timeseries"][0]["data"]["next_1_hours"]["summary"]["symbol_code"] = true;

    // Allocate sufficient memory for filtered response
    // We need ~100 timeseries entries with minimal fields
    const size_t capacity = 50000;  // 50KB should be enough for filtered data
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));

    if (error) {
        Serial.printf("[Weather] JSON parse error: %s\n", error.c_str());
        Serial.printf("[Weather] Capacity: %u, used: %u\n", doc.capacity(), doc.memoryUsage());
        return false;
    }

    // Navigate to timeseries array
    JsonArray timeseries = doc["properties"]["timeseries"];
    if (!timeseries) {
        Serial.println("[Weather] Missing timeseries data");
        return false;
    }

    Serial.printf("[Weather] Timeseries entries: %d\n", timeseries.size());

    // Use maps to aggregate data by date
    struct DayData {
        float tempMin = 999.0f;
        float tempMax = -999.0f;
        std::vector<float> humidityValues;
        std::map<String, int> symbolCounts;
        int sampleCount = 0;
    };

    std::map<String, DayData> dailyData;
    String dates[4];
    int dateCount = 0;

    // Process timeseries entries
    for (JsonObject entry : timeseries) {
        const char* timeStr = entry["time"];
        if (!timeStr) continue;

        // Extract date (YYYY-MM-DD) from ISO8601 timestamp
        String timestamp(timeStr);
        String date = timestamp.substring(0, 10);

        // Only process first 4 unique dates
        bool dateExists = false;
        for (int i = 0; i < dateCount; i++) {
            if (dates[i] == date) {
                dateExists = true;
                break;
            }
        }

        if (!dateExists) {
            if (dateCount >= 4) {
                break;  // Already have 4 days
            }
            dates[dateCount++] = date;
        }

        // Get instant data
        JsonObject instant = entry["data"]["instant"]["details"];
        if (instant) {
            float temp = instant["air_temperature"];
            float humidity = instant["relative_humidity"];

            DayData& day = dailyData[date];
            day.sampleCount++;

            if (temp < day.tempMin) day.tempMin = temp;
            if (temp > day.tempMax) day.tempMax = temp;
            day.humidityValues.push_back(humidity);
        }

        // Get next_1_hours symbol (most relevant for daily summary)
        JsonObject next1h = entry["data"]["next_1_hours"];
        if (next1h) {
            const char* symbol = next1h["summary"]["symbol_code"];
            if (symbol) {
                String symbolStr(symbol);
                dailyData[date].symbolCounts[symbolStr]++;
            }
        }
    }

    // Convert aggregated data to forecast structure
    forecast.dayCount = 0;
    for (int i = 0; i < dateCount && i < 4; i++) {
        String& date = dates[i];
        DayData& dayData = dailyData[date];

        if (dayData.sampleCount == 0) continue;

        DailyForecast& day = forecast.days[forecast.dayCount];

        // Copy date
        strncpy(day.date, date.c_str(), sizeof(day.date) - 1);

        // Temperature
        day.tempMin = dayData.tempMin;
        day.tempMax = dayData.tempMax;

        // Average humidity
        float humiditySum = 0.0f;
        for (float h : dayData.humidityValues) {
            humiditySum += h;
        }
        day.humidity = humiditySum / dayData.humidityValues.size();

        // Most common symbol
        String mostCommonSymbol;
        int maxCount = 0;
        for (const auto& pair : dayData.symbolCounts) {
            if (pair.second > maxCount) {
                maxCount = pair.second;
                mostCommonSymbol = pair.first;
            }
        }
        strncpy(day.symbolCode, mostCommonSymbol.c_str(), sizeof(day.symbolCode) - 1);

        day.valid = true;
        forecast.dayCount++;
    }

    forecast.valid = (forecast.dayCount > 0);
    return forecast.valid;
}

void WeatherClient::aggregateDailyData(WeatherForecast& forecast) {
    // This method is reserved for future use if additional aggregation is needed
    // Currently, aggregation is done directly in parseResponse
}
