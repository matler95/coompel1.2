#include "WeatherService.h"
#include <WiFi.h>

WeatherService::WeatherService()
    : state_(WeatherState::IDLE),
      enabled_(false),  // Default disabled - user must opt-in via menu
      updateIntervalSecs_(DEFAULT_UPDATE_INTERVAL),
      locationFetchTime_(0),
      weatherFetchTime_(0),
      lastAttemptTime_(0),
      nextUpdateTime_(0),
      wifiConnectedTimeMs_(0),
      retryCount_(0),
      wasConnected_(false),
      lastError_(WeatherError::NONE),
      eventCallback_(nullptr),
      fetchInProgress_(false),
      fetchNeedsLocation_(false),
      fetchTaskHandle_(nullptr),
      dataMutex_(nullptr) {
    location_.valid = false;
    forecast_.valid = false;
}

const char* WeatherService::getErrorString() const {
    switch (lastError_) {
        case WeatherError::NONE:                  return "No error";
        case WeatherError::WIFI_NOT_CONNECTED:    return "WiFi disconnected";
        case WeatherError::HTTP_TIMEOUT:          return "Connection timeout";
        case WeatherError::HTTP_CONNECTION_FAILED: return "Connection failed";
        case WeatherError::HTTP_ERROR_400:        return "Bad request";
        case WeatherError::HTTP_ERROR_403:        return "Access forbidden";
        case WeatherError::HTTP_ERROR_404:        return "Not found";
        case WeatherError::HTTP_ERROR_429:        return "Rate limited";
        case WeatherError::HTTP_ERROR_500:        return "Server error";
        case WeatherError::HTTP_ERROR_OTHER:      return "HTTP error";
        case WeatherError::JSON_PARSE_FAILED:     return "Parse failed";
        case WeatherError::INVALID_RESPONSE:      return "Invalid response";
        case WeatherError::LOCATION_FAILED:       return "Location failed";
        case WeatherError::WEATHER_FAILED:        return "Weather failed";
        default:                                  return "Unknown error";
    }
}

bool WeatherService::init() {
    Serial.println("[WeatherService] Initializing...");

    // Create mutex for thread-safe data access
    dataMutex_ = xSemaphoreCreateMutex();
    if (dataMutex_ == nullptr) {
        Serial.println("[WeatherService] Failed to create mutex");
        return false;
    }

    // Load cached data and settings from NVS
    loadCacheFromNVS();

    if (location_.valid && forecast_.valid) {
        setState(WeatherState::CACHED);
        Serial.printf("[WeatherService] Loaded cache: %s, %s\n",
                     location_.city, location_.country);
        triggerEvent(WeatherEvent::CACHE_LOADED);
    } else if (location_.valid || forecast_.valid) {
        setState(WeatherState::STALE);
        Serial.println("[WeatherService] Partial cache loaded");
    } else {
        Serial.println("[WeatherService] No cached data available");
    }

    // Schedule first update check
    nextUpdateTime_ = millis() / 1000 + 5;  // Check in 5 seconds

    return true;
}

void WeatherService::update() {
    // Skip if disabled
    if (!enabled_) {
        return;
    }

    // Skip if fetch already in progress
    if (fetchInProgress_) {
        return;
    }

    uint32_t nowMs = millis();
    uint32_t now = nowMs / 1000;
    bool isConnected = (WiFi.status() == WL_CONNECTED);

    // Track WiFi connection state changes
    if (isConnected && !wasConnected_) {
        // WiFi just connected - record time and wait for DNS to stabilize
        wifiConnectedTimeMs_ = nowMs;
        wasConnected_ = true;
        Serial.printf("[WeatherService] WiFi connected, waiting %lu ms for DNS...\n", WIFI_STABILIZE_MS);
        return;
    } else if (!isConnected) {
        wasConnected_ = false;
        wifiConnectedTimeMs_ = 0;
        return;
    }

    // Wait for WiFi/DNS to stabilize after connection (using millisecond precision)
    if (wifiConnectedTimeMs_ > 0 && (nowMs - wifiConnectedTimeMs_) < WIFI_STABILIZE_MS) {
        return;
    }

    // Not time for update yet
    if (now < nextUpdateTime_) {
        return;
    }

    // Check what needs updating
    bool needsLocation = !isLocationCacheValid();
    bool needsWeather = !isWeatherCacheValid();

    if (!needsLocation && !needsWeather) {
        // Cache still valid, check again in 1 minute
        nextUpdateTime_ = now + 60;
        return;
    }

    // Respect minimum interval between attempts
    if (now - lastAttemptTime_ < MIN_UPDATE_INTERVAL_SECS && lastAttemptTime_ != 0) {
        nextUpdateTime_ = lastAttemptTime_ + MIN_UPDATE_INTERVAL_SECS;
        return;
    }

    // Start background fetch
    lastAttemptTime_ = now;
    startBackgroundFetch(needsLocation);
}

bool WeatherService::forceUpdate() {
    Serial.println("[WeatherService] Force update initiated");

    if (fetchInProgress_) {
        Serial.println("[WeatherService] Fetch already in progress");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WeatherService] WiFi not connected");
        return false;
    }

    // Start background fetch with location refresh
    startBackgroundFetch(true);
    return true;
}

// Static wrapper for FreeRTOS task
void WeatherService::fetchTaskWrapper(void* param) {
    WeatherService* self = static_cast<WeatherService*>(param);
    self->fetchTask();
    vTaskDelete(nullptr);  // Delete task when done
}

// Start background fetch task
void WeatherService::startBackgroundFetch(bool includeLocation) {
    if (fetchInProgress_) {
        return;
    }

    fetchInProgress_ = true;
    fetchNeedsLocation_ = includeLocation;

    if (includeLocation) {
        setState(WeatherState::FETCHING_LOCATION);
    } else {
        setState(WeatherState::FETCHING_WEATHER);
    }

    Serial.println("[WeatherService] Starting background fetch task...");

    BaseType_t result = xTaskCreate(
        fetchTaskWrapper,
        "WeatherFetch",
        FETCH_TASK_STACK_SIZE,
        this,
        FETCH_TASK_PRIORITY,
        &fetchTaskHandle_
    );

    if (result != pdPASS) {
        Serial.println("[WeatherService] Failed to create fetch task");
        fetchInProgress_ = false;
        setState(WeatherState::ERROR);
        lastError_ = WeatherError::HTTP_CONNECTION_FAILED;
    }
}

// Background fetch task - runs in separate FreeRTOS task
void WeatherService::fetchTask() {
    Serial.println("[WeatherService] Fetch task started");

    size_t heapBefore = ESP.getFreeHeap();
    Serial.printf("[WeatherService] Free heap before: %u bytes\n", heapBefore);

    bool success = true;
    uint32_t now = millis() / 1000;

    // Fetch location if needed
    if (fetchNeedsLocation_) {
        if (!fetchLocation()) {
            retryCount_++;
            if (retryCount_ >= MAX_RETRIES) {
                setState(WeatherState::ERROR);
                nextUpdateTime_ = now + updateIntervalSecs_;
                retryCount_ = 0;
            } else {
                nextUpdateTime_ = now + RETRY_DELAY_SECS;
            }
            success = false;
        } else {
            retryCount_ = 0;
        }
    }

    // Fetch weather if location succeeded or wasn't needed
    if (success) {
        setState(WeatherState::FETCHING_WEATHER);

        if (!fetchWeather()) {
            retryCount_++;
            if (retryCount_ >= MAX_RETRIES) {
                setState(WeatherState::ERROR);
                nextUpdateTime_ = now + updateIntervalSecs_;
                retryCount_ = 0;
            } else {
                nextUpdateTime_ = now + RETRY_DELAY_SECS;
            }
            success = false;
        } else {
            retryCount_ = 0;
        }
    }

    // Update state based on result
    if (success) {
        setState(WeatherState::CACHED);
        nextUpdateTime_ = now + updateIntervalSecs_;
    }

    size_t heapAfter = ESP.getFreeHeap();
    Serial.printf("[WeatherService] Free heap after: %u bytes\n", heapAfter);
    Serial.printf("[WeatherService] Heap used: %d bytes\n", (int)(heapBefore - heapAfter));

    fetchTaskHandle_ = nullptr;
    fetchInProgress_ = false;

    Serial.println("[WeatherService] Fetch task completed");
}

bool WeatherService::fetchLocation() {
    Serial.println("[WeatherService] Fetching geolocation...");

    if (WiFi.status() != WL_CONNECTED) {
        lastError_ = WeatherError::WIFI_NOT_CONNECTED;
        Serial.println("[WeatherService] WiFi not connected");
        triggerEvent(WeatherEvent::LOCATION_FAILED);
        return false;
    }

    if (!geoClient_.fetchLocation(location_)) {
        lastError_ = WeatherError::LOCATION_FAILED;
        Serial.println("[WeatherService] Geolocation fetch failed");
        triggerEvent(WeatherEvent::LOCATION_FAILED);
        return false;
    }

    lastError_ = WeatherError::NONE;

    Serial.printf("[WeatherService] Location: %.4f, %.4f (%s, %s)\n",
                 location_.latitude, location_.longitude,
                 location_.city, location_.country);

    locationFetchTime_ = millis() / 1000;
    saveLocationToNVS();
    triggerEvent(WeatherEvent::LOCATION_UPDATED);

    return true;
}

bool WeatherService::fetchWeather() {
    if (!location_.valid) {
        lastError_ = WeatherError::LOCATION_FAILED;
        Serial.println("[WeatherService] No valid location for weather fetch");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        lastError_ = WeatherError::WIFI_NOT_CONNECTED;
        Serial.println("[WeatherService] WiFi not connected");
        triggerEvent(WeatherEvent::WEATHER_FAILED);
        return false;
    }

    Serial.println("[WeatherService] Fetching weather forecast...");

    if (!weatherClient_.fetchForecast(location_.latitude, location_.longitude, forecast_)) {
        lastError_ = WeatherError::WEATHER_FAILED;
        Serial.println("[WeatherService] Weather fetch failed");
        triggerEvent(WeatherEvent::WEATHER_FAILED);
        return false;
    }

    lastError_ = WeatherError::NONE;
    Serial.printf("[WeatherService] Weather fetched: %d days\n", forecast_.dayCount);
    for (int i = 0; i < forecast_.dayCount; i++) {
        Serial.printf("[WeatherService]   %s: %.1f-%.1f°C, %.0f%%, %s\n",
                     forecast_.days[i].date,
                     forecast_.days[i].tempMin,
                     forecast_.days[i].tempMax,
                     forecast_.days[i].humidity,
                     forecast_.days[i].symbolCode);
    }

    weatherFetchTime_ = millis() / 1000;
    saveWeatherToNVS();
    triggerEvent(WeatherEvent::WEATHER_UPDATED);

    return true;
}

void WeatherService::setState(WeatherState newState) {
    if (state_ != newState) {
        state_ = newState;
    }
}

void WeatherService::triggerEvent(WeatherEvent event) {
    if (eventCallback_ != nullptr) {
        eventCallback_(event);
    }
}

void WeatherService::setEnabled(bool enabled) {
    enabled_ = enabled;

    // Persist to NVS
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putBool(KEY_ENABLED, enabled_);
        prefs.end();
    }
}

void WeatherService::setUpdateInterval(uint32_t seconds) {
    if (seconds < MIN_UPDATE_INTERVAL_SECS) {
        seconds = MIN_UPDATE_INTERVAL_SECS;
    }
    updateIntervalSecs_ = seconds;

    // Persist to NVS
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putULong(KEY_INTERVAL, updateIntervalSecs_);
        prefs.end();
    }
}

bool WeatherService::isLocationCacheValid() const {
    if (!location_.valid || locationFetchTime_ == 0) {
        return false;
    }

    uint32_t now = millis() / 1000;
    uint32_t age = now - locationFetchTime_;

    return age < LOCATION_CACHE_SECS;
}

bool WeatherService::isWeatherCacheValid() const {
    if (!forecast_.valid || weatherFetchTime_ == 0) {
        return false;
    }

    uint32_t now = millis() / 1000;
    uint32_t age = now - weatherFetchTime_;

    return age < WEATHER_CACHE_SECS;
}

void WeatherService::saveLocationToNVS() {
    Serial.println("[WeatherService] Saving location to NVS...");

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[WeatherService] Failed to open NVS for writing");
        return;
    }

    prefs.putFloat(KEY_LAT, location_.latitude);
    prefs.putFloat(KEY_LON, location_.longitude);
    prefs.putString(KEY_CITY, location_.city);
    prefs.putString(KEY_COUNTRY, location_.country);
    prefs.putULong(KEY_LOC_TIME, locationFetchTime_);

    prefs.end();
    Serial.println("[WeatherService] Location saved");
}

void WeatherService::saveWeatherToNVS() {
    Serial.println("[WeatherService] Saving weather to NVS...");

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[WeatherService] Failed to open NVS for writing");
        return;
    }

    prefs.putUChar(KEY_FC_COUNT, forecast_.dayCount);
    prefs.putULong(KEY_FC_TIME, weatherFetchTime_);

    // Save each day using fixed keys
    if (forecast_.dayCount > 0) {
        prefs.putString(KEY_FC0_DATE, forecast_.days[0].date);
        prefs.putFloat(KEY_FC0_TMIN, forecast_.days[0].tempMin);
        prefs.putFloat(KEY_FC0_TMAX, forecast_.days[0].tempMax);
        prefs.putFloat(KEY_FC0_HUM, forecast_.days[0].humidity);
        prefs.putString(KEY_FC0_SYM, forecast_.days[0].symbolCode);
    }

    if (forecast_.dayCount > 1) {
        prefs.putString(KEY_FC1_DATE, forecast_.days[1].date);
        prefs.putFloat(KEY_FC1_TMIN, forecast_.days[1].tempMin);
        prefs.putFloat(KEY_FC1_TMAX, forecast_.days[1].tempMax);
        prefs.putFloat(KEY_FC1_HUM, forecast_.days[1].humidity);
        prefs.putString(KEY_FC1_SYM, forecast_.days[1].symbolCode);
    }

    if (forecast_.dayCount > 2) {
        prefs.putString(KEY_FC2_DATE, forecast_.days[2].date);
        prefs.putFloat(KEY_FC2_TMIN, forecast_.days[2].tempMin);
        prefs.putFloat(KEY_FC2_TMAX, forecast_.days[2].tempMax);
        prefs.putFloat(KEY_FC2_HUM, forecast_.days[2].humidity);
        prefs.putString(KEY_FC2_SYM, forecast_.days[2].symbolCode);
    }

    if (forecast_.dayCount > 3) {
        prefs.putString(KEY_FC3_DATE, forecast_.days[3].date);
        prefs.putFloat(KEY_FC3_TMIN, forecast_.days[3].tempMin);
        prefs.putFloat(KEY_FC3_TMAX, forecast_.days[3].tempMax);
        prefs.putFloat(KEY_FC3_HUM, forecast_.days[3].humidity);
        prefs.putString(KEY_FC3_SYM, forecast_.days[3].symbolCode);
    }

    prefs.end();
    Serial.println("[WeatherService] Weather saved");
}

void WeatherService::loadCacheFromNVS() {
    Serial.println("[WeatherService] Loading cache from NVS...");

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("[WeatherService] Failed to open NVS for reading");
        return;
    }

    // Load settings
    enabled_ = prefs.getBool(KEY_ENABLED, false);  // Default off - opt-in required
    updateIntervalSecs_ = prefs.getULong(KEY_INTERVAL, DEFAULT_UPDATE_INTERVAL);

    // Load location
    if (prefs.isKey(KEY_LAT) && prefs.isKey(KEY_LON)) {
        location_.latitude = prefs.getFloat(KEY_LAT, 0.0f);
        location_.longitude = prefs.getFloat(KEY_LON, 0.0f);

        // Use buffer-based getString to avoid String allocations
        char buffer[32];

        size_t len = prefs.getString(KEY_CITY, buffer, sizeof(buffer));
        if (len > 0) {
            strncpy(location_.city, buffer, sizeof(location_.city) - 1);
            location_.city[sizeof(location_.city) - 1] = '\0';
        }

        len = prefs.getString(KEY_COUNTRY, buffer, sizeof(buffer));
        if (len > 0) {
            strncpy(location_.country, buffer, sizeof(location_.country) - 1);
            location_.country[sizeof(location_.country) - 1] = '\0';
        }

        locationFetchTime_ = prefs.getULong(KEY_LOC_TIME, 0);
        location_.valid = true;

        Serial.printf("[WeatherService] Loaded location: %.4f, %.4f (%s)\n",
                     location_.latitude, location_.longitude, location_.city);
    }

    // Load forecast
    uint8_t dayCount = prefs.getUChar(KEY_FC_COUNT, 0);
    if (dayCount > 0 && dayCount <= 4) {
        forecast_.dayCount = dayCount;
        weatherFetchTime_ = prefs.getULong(KEY_FC_TIME, 0);

        char dateBuffer[16];
        char symBuffer[32];

        // Load day 0
        if (dayCount > 0) {
            prefs.getString(KEY_FC0_DATE, dateBuffer, sizeof(dateBuffer));
            strncpy(forecast_.days[0].date, dateBuffer, sizeof(forecast_.days[0].date) - 1);
            forecast_.days[0].date[sizeof(forecast_.days[0].date) - 1] = '\0';

            forecast_.days[0].tempMin = prefs.getFloat(KEY_FC0_TMIN, 0.0f);
            forecast_.days[0].tempMax = prefs.getFloat(KEY_FC0_TMAX, 0.0f);
            forecast_.days[0].humidity = prefs.getFloat(KEY_FC0_HUM, 0.0f);

            prefs.getString(KEY_FC0_SYM, symBuffer, sizeof(symBuffer));
            strncpy(forecast_.days[0].symbolCode, symBuffer, sizeof(forecast_.days[0].symbolCode) - 1);
            forecast_.days[0].symbolCode[sizeof(forecast_.days[0].symbolCode) - 1] = '\0';

            forecast_.days[0].valid = true;
        }

        // Load day 1
        if (dayCount > 1) {
            prefs.getString(KEY_FC1_DATE, dateBuffer, sizeof(dateBuffer));
            strncpy(forecast_.days[1].date, dateBuffer, sizeof(forecast_.days[1].date) - 1);
            forecast_.days[1].date[sizeof(forecast_.days[1].date) - 1] = '\0';

            forecast_.days[1].tempMin = prefs.getFloat(KEY_FC1_TMIN, 0.0f);
            forecast_.days[1].tempMax = prefs.getFloat(KEY_FC1_TMAX, 0.0f);
            forecast_.days[1].humidity = prefs.getFloat(KEY_FC1_HUM, 0.0f);

            prefs.getString(KEY_FC1_SYM, symBuffer, sizeof(symBuffer));
            strncpy(forecast_.days[1].symbolCode, symBuffer, sizeof(forecast_.days[1].symbolCode) - 1);
            forecast_.days[1].symbolCode[sizeof(forecast_.days[1].symbolCode) - 1] = '\0';

            forecast_.days[1].valid = true;
        }

        // Load day 2
        if (dayCount > 2) {
            prefs.getString(KEY_FC2_DATE, dateBuffer, sizeof(dateBuffer));
            strncpy(forecast_.days[2].date, dateBuffer, sizeof(forecast_.days[2].date) - 1);
            forecast_.days[2].date[sizeof(forecast_.days[2].date) - 1] = '\0';

            forecast_.days[2].tempMin = prefs.getFloat(KEY_FC2_TMIN, 0.0f);
            forecast_.days[2].tempMax = prefs.getFloat(KEY_FC2_TMAX, 0.0f);
            forecast_.days[2].humidity = prefs.getFloat(KEY_FC2_HUM, 0.0f);

            prefs.getString(KEY_FC2_SYM, symBuffer, sizeof(symBuffer));
            strncpy(forecast_.days[2].symbolCode, symBuffer, sizeof(forecast_.days[2].symbolCode) - 1);
            forecast_.days[2].symbolCode[sizeof(forecast_.days[2].symbolCode) - 1] = '\0';

            forecast_.days[2].valid = true;
        }

        // Load day 3
        if (dayCount > 3) {
            prefs.getString(KEY_FC3_DATE, dateBuffer, sizeof(dateBuffer));
            strncpy(forecast_.days[3].date, dateBuffer, sizeof(forecast_.days[3].date) - 1);
            forecast_.days[3].date[sizeof(forecast_.days[3].date) - 1] = '\0';

            forecast_.days[3].tempMin = prefs.getFloat(KEY_FC3_TMIN, 0.0f);
            forecast_.days[3].tempMax = prefs.getFloat(KEY_FC3_TMAX, 0.0f);
            forecast_.days[3].humidity = prefs.getFloat(KEY_FC3_HUM, 0.0f);

            prefs.getString(KEY_FC3_SYM, symBuffer, sizeof(symBuffer));
            strncpy(forecast_.days[3].symbolCode, symBuffer, sizeof(forecast_.days[3].symbolCode) - 1);
            forecast_.days[3].symbolCode[sizeof(forecast_.days[3].symbolCode) - 1] = '\0';

            forecast_.days[3].valid = true;
        }

        forecast_.valid = true;
        Serial.printf("[WeatherService] Loaded forecast: %d days\n", forecast_.dayCount);
    }

    prefs.end();
}

void WeatherService::clearCache() {
    Serial.println("[WeatherService] Clearing cache...");

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[WeatherService] Failed to open NVS for clearing");
        return;
    }

    prefs.clear();
    prefs.end();

    // Reset in-memory data
    location_.valid = false;
    forecast_.valid = false;
    locationFetchTime_ = 0;
    weatherFetchTime_ = 0;
    retryCount_ = 0;
    setState(WeatherState::IDLE);

    Serial.println("[WeatherService] Cache cleared");
}
