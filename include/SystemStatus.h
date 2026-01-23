#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <Arduino.h>
#include "WiFiManager.h"
#include "WeatherService.h"

struct SystemStatus {
    WiFiState wifiState = WiFiState::IDLE;
    bool wifiConnected = false;
    WeatherState weatherState = WeatherState::IDLE;
    WeatherError lastWeatherError = WeatherError::NONE;
    uint32_t lastWeatherUpdateTs = 0;
};

#endif
