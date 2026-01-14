#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include <Arduino.h>

// Daily weather forecast data
struct DailyForecast {
    char date[11];           // YYYY-MM-DD format
    float tempMin = 0.0f;    // Minimum temperature (°C)
    float tempMax = 0.0f;    // Maximum temperature (°C)
    float humidity = 0.0f;   // Average humidity (%)
    char symbolCode[32];     // Most common weather symbol code
    bool valid = false;
};

// Weather forecast collection (today + 3 days)
struct WeatherForecast {
    DailyForecast days[4];   // 4-day forecast
    uint8_t dayCount = 0;
    bool valid = false;
};

// Client for fetching weather forecast from MET Norway API
class WeatherClient {
public:
    WeatherClient();

    // Fetch 4-day weather forecast for given coordinates
    // Returns true on success, false on failure
    // Forecast data is stored in the provided WeatherForecast struct
    bool fetchForecast(float latitude, float longitude, WeatherForecast& forecast);

private:
    // Parse JSON response from MET Norway API
    bool parseResponse(const String& json, WeatherForecast& forecast);

    // Aggregate hourly data into daily summaries
    void aggregateDailyData(WeatherForecast& forecast);

    // API endpoint - MET Norway LocationForecast
    static constexpr const char* API_URL = "https://api.met.no/weatherapi/locationforecast/2.0/compact";

    // User-Agent header (required by MET Norway API)
    static constexpr const char* USER_AGENT = "coompel-weather/0.1.0 github.com/yourproject";

    // HTTP timeout in milliseconds
    static constexpr int TIMEOUT_MS = 15000;
};

#endif // WEATHER_CLIENT_H
