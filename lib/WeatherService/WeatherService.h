#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include <Arduino.h>
#include <Preferences.h>
#include "GeoLocationClient.h"
#include "WeatherClient.h"

// Weather service states
enum class WeatherState {
    IDLE,               // Not started
    FETCHING_LOCATION,  // Getting geolocation
    FETCHING_WEATHER,   // Getting forecast
    CACHED,             // Have valid cached data
    STALE,              // Cache expired, awaiting update
    ERROR               // Last fetch failed
};

// Events for callback notifications
enum class WeatherEvent {
    LOCATION_UPDATED,
    WEATHER_UPDATED,
    LOCATION_FAILED,
    WEATHER_FAILED,
    CACHE_LOADED,
    CACHE_STALE
};

using WeatherEventCallback = void (*)(WeatherEvent event);

/**
 * WeatherService - Coordinator for geolocation and weather forecast
 *
 * Features:
 * - Combines GeoLocationClient and WeatherClient
 * - Smart caching with NVS persistence
 * - Scheduled updates (4 hours for weather, 7 days for location)
 * - Non-blocking update() for main loop integration
 * - Graceful degradation on network failure
 */
class WeatherService {
public:
    WeatherService();

    // Initialize service, load cached data from NVS
    bool init();

    // Non-blocking update - call from main loop
    void update();

    // Force immediate update regardless of cache validity
    bool forceUpdate();

    // Getters
    const WeatherForecast& getForecast() const { return forecast_; }
    const GeoLocation& getLocation() const { return location_; }
    WeatherState getState() const { return state_; }
    bool hasValidData() const { return forecast_.valid && location_.valid; }
    uint32_t getLastUpdateTime() const { return weatherFetchTime_; }
    uint32_t getNextUpdateTime() const { return nextUpdateTime_; }

    // Configuration
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }
    void setUpdateInterval(uint32_t seconds);
    uint32_t getUpdateInterval() const { return updateIntervalSecs_; }

    // Cache management
    void clearCache();

    // Event callback
    void setEventCallback(WeatherEventCallback callback) { eventCallback_ = callback; }

private:
    // State management
    void setState(WeatherState newState);
    void triggerEvent(WeatherEvent event);

    // Fetch operations
    bool fetchLocation();
    bool fetchWeather();

    // Cache operations with FIXED keys (no String concatenation)
    void saveLocationToNVS();
    void saveWeatherToNVS();
    void loadCacheFromNVS();
    bool isLocationCacheValid() const;
    bool isWeatherCacheValid() const;

    // NVS namespace
    static constexpr const char* NVS_NAMESPACE = "weather";

    // NVS keys - ALL FIXED const char* (no String concatenation!)
    static constexpr const char* KEY_ENABLED = "enabled";
    static constexpr const char* KEY_INTERVAL = "interval";
    static constexpr const char* KEY_LAT = "lat";
    static constexpr const char* KEY_LON = "lon";
    static constexpr const char* KEY_CITY = "city";
    static constexpr const char* KEY_COUNTRY = "country";
    static constexpr const char* KEY_LOC_TIME = "loc_time";
    static constexpr const char* KEY_FC_COUNT = "fc_count";
    static constexpr const char* KEY_FC_TIME = "fc_time";

    // Forecast day 0 keys
    static constexpr const char* KEY_FC0_DATE = "fc0_date";
    static constexpr const char* KEY_FC0_TMIN = "fc0_tmin";
    static constexpr const char* KEY_FC0_TMAX = "fc0_tmax";
    static constexpr const char* KEY_FC0_HUM = "fc0_hum";
    static constexpr const char* KEY_FC0_SYM = "fc0_sym";

    // Forecast day 1 keys
    static constexpr const char* KEY_FC1_DATE = "fc1_date";
    static constexpr const char* KEY_FC1_TMIN = "fc1_tmin";
    static constexpr const char* KEY_FC1_TMAX = "fc1_tmax";
    static constexpr const char* KEY_FC1_HUM = "fc1_hum";
    static constexpr const char* KEY_FC1_SYM = "fc1_sym";

    // Forecast day 2 keys
    static constexpr const char* KEY_FC2_DATE = "fc2_date";
    static constexpr const char* KEY_FC2_TMIN = "fc2_tmin";
    static constexpr const char* KEY_FC2_TMAX = "fc2_tmax";
    static constexpr const char* KEY_FC2_HUM = "fc2_hum";
    static constexpr const char* KEY_FC2_SYM = "fc2_sym";

    // Forecast day 3 keys
    static constexpr const char* KEY_FC3_DATE = "fc3_date";
    static constexpr const char* KEY_FC3_TMIN = "fc3_tmin";
    static constexpr const char* KEY_FC3_TMAX = "fc3_tmax";
    static constexpr const char* KEY_FC3_HUM = "fc3_hum";
    static constexpr const char* KEY_FC3_SYM = "fc3_sym";

    // Timing constants
    static constexpr uint32_t LOCATION_CACHE_SECS = 7UL * 24UL * 60UL * 60UL;  // 7 days
    static constexpr uint32_t WEATHER_CACHE_SECS = 4UL * 60UL * 60UL;          // 4 hours
    static constexpr uint32_t MIN_UPDATE_INTERVAL_SECS = 5UL * 60UL;           // 5 minutes
    static constexpr uint32_t RETRY_DELAY_SECS = 30UL;                         // 30 seconds
    static constexpr uint32_t DEFAULT_UPDATE_INTERVAL = 4UL * 60UL * 60UL;     // 4 hours

    // Retry tracking
    static constexpr uint8_t MAX_RETRIES = 3;

    // Clients
    GeoLocationClient geoClient_;
    WeatherClient weatherClient_;

    // Cached data
    GeoLocation location_;
    WeatherForecast forecast_;

    // State
    WeatherState state_;
    bool enabled_;
    uint32_t updateIntervalSecs_;

    // Timestamps (seconds since boot via millis()/1000)
    uint32_t locationFetchTime_;
    uint32_t weatherFetchTime_;
    uint32_t lastAttemptTime_;
    uint32_t nextUpdateTime_;

    // Retry tracking
    uint8_t retryCount_;

    // Callback
    WeatherEventCallback eventCallback_;
};

#endif // WEATHER_SERVICE_H
