#include "GeoLocationClient.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#define TIMEOUT_MS 10000  // HTTP timeout

// Geo providers (primary + fallback)
const char* GEO_PROVIDERS[] = {
    "https://ipwho.is/",
    "https://ipapi.co/json/"  // fallback
};
const size_t GEO_PROVIDER_COUNT = sizeof(GEO_PROVIDERS) / sizeof(GEO_PROVIDERS[0]);

GeoLocationClient::GeoLocationClient() : lastHttpCode_(0) {
    // Constructor
}

bool GeoLocationClient::fetchLocation(GeoLocation& location) {
    // Reset location
    location.valid = false;
    location.latitude = 0.0f;
    location.longitude = 0.0f;
    memset(location.city, 0, sizeof(location.city));
    memset(location.country, 0, sizeof(location.country));

    Serial.println("[GeoLocation] Fetching location via IP...");
    Serial.printf("[GeoLocation] Free heap before: %u bytes\n", ESP.getFreeHeap());

    // --- Ensure WiFi is connected ---
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[GeoLocation] WiFi not connected");
        return false;
    }

    // Optional: Force reliable DNS (Google DNS)
    WiFi.config(
        WiFi.localIP(),
        WiFi.gatewayIP(),
        WiFi.subnetMask(),
        IPAddress(8, 8, 8, 8),
        IPAddress(8, 8, 4, 4)
    );

    WiFiClientSecure client;
    client.setInsecure();  // Skip TLS verification for simplicity

    HTTPClient http;
    http.setTimeout(TIMEOUT_MS);

    bool success = false;

    // Try all providers until one succeeds
    for (size_t i = 0; i < GEO_PROVIDER_COUNT; ++i) {
        const char* url = GEO_PROVIDERS[i];
        Serial.printf("[GeoLocation] Trying provider: %s\n", url);

        if (!http.begin(client, url)) {
            Serial.println("[GeoLocation] Failed to initialize HTTP client");
            continue;
        }

        int httpCode = http.GET();
        lastHttpCode_ = httpCode;
        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("[GeoLocation] HTTP error: %d\n", httpCode);
            if (httpCode == 429) {
                Serial.println("[GeoLocation] Rate limited!");
            }
            http.end();
            continue;
        }

        String payload = http.getString();
        http.end();

        Serial.printf("[GeoLocation] Response length: %d bytes\n", payload.length());
        Serial.printf("[GeoLocation] Free heap after HTTP: %u bytes\n", ESP.getFreeHeap());

        if (parseResponse(payload, location)) {
            success = true;
            break;  // Stop on first success
        }
    }

    if (success) {
        Serial.printf("[GeoLocation] Success: %.4f, %.4f (%s, %s)\n",
                      location.latitude,
                      location.longitude,
                      location.city,
                      location.country);
    } else {
        Serial.println("[GeoLocation] All providers failed");
    }

    return success;
}

bool GeoLocationClient::parseResponse(const String& json, GeoLocation& location) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.printf("[GeoLocation] JSON parse error: %s\n", error.c_str());
        return false;
    }

    // ipwho.is uses "success"; ipapi.co does not
    if (doc.containsKey("success") && !doc["success"].as<bool>()) {
        Serial.println("[GeoLocation] API returned success=false");
        return false;
    }

    // Extract latitude
    if (doc.containsKey("latitude")) {
        location.latitude = doc["latitude"];
    } else if (doc.containsKey("lat")) {
        location.latitude = doc["lat"];
    } else {
        Serial.println("[GeoLocation] Missing latitude");
        return false;
    }

    // Extract longitude
    if (doc.containsKey("longitude")) {
        location.longitude = doc["longitude"];
    } else if (doc.containsKey("lon")) {
        location.longitude = doc["lon"];
    } else {
        Serial.println("[GeoLocation] Missing longitude");
        return false;
    }

    // Validate coordinates
    if (location.latitude < -90.0f || location.latitude > 90.0f ||
        location.longitude < -180.0f || location.longitude > 180.0f) {
        Serial.println("[GeoLocation] Invalid coordinates");
        return false;
    }

    // Extract city
    if (doc.containsKey("city")) {
        const char* cityStr = doc["city"];
        if (cityStr) {
            strncpy(location.city, cityStr, sizeof(location.city) - 1);
            location.city[sizeof(location.city) - 1] = '\0';
        }
    }

    // Extract country code
    if (doc.containsKey("country_code")) {
        const char* countryStr = doc["country_code"];
        if (countryStr) {
            strncpy(location.country, countryStr, sizeof(location.country) - 1);
            location.country[sizeof(location.country) - 1] = '\0';
        }
    } else if (doc.containsKey("country")) {  // fallback ipapi.co
        const char* countryStr = doc["country"];
        if (countryStr) {
            strncpy(location.country, countryStr, sizeof(location.country) - 1);
            location.country[sizeof(location.country) - 1] = '\0';
        }
    }

    location.valid = true;
    return true;
}
