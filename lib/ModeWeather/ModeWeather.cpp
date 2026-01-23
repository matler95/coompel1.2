#include "ModeWeather.h"
#include "DisplayManager.h"
#include "WeatherService.h"
#include "WiFiManager.h"
#include "SystemStatus.h"
#include "config.h"

extern DisplayManager display;
extern WiFiManager wifi;
extern WeatherService weatherService;
extern SystemStatus systemStatus;
// extern AppMode currentMode;  // Not used, AppMode defined in main.cpp
extern uint8_t weatherViewPage;

extern const uint8_t* getWeatherIcon(const char* symbolCode);

void updateWeatherViewMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 100) return;
    lastUpdate = millis();

    display.clear();

    if (!weatherService.hasValidData()) {
        display.showTextCentered("Weather", 0, 1);

        WeatherState state = weatherService.getState();

        if (state == WeatherState::FETCHING_LOCATION) {
            display.drawText("Getting", 0, 20, 1);
            display.drawText("location...", 0, 32, 1);
        } else if (state == WeatherState::FETCHING_WEATHER) {
            display.drawText("Getting", 0, 20, 1);
            display.drawText("forecast...", 0, 32, 1);
        } else if (state == WeatherState::ERROR) {
            display.drawText("Error:", 0, 20, 1);
            display.drawText(weatherService.getErrorString(), 0, 32, 1);

            char retryStr[24];
            snprintf(retryStr, sizeof(retryStr), "Retry %d/%d",
                    weatherService.getRetryCount(), 3);
            display.drawText(retryStr, 0, 44, 1);
        } else if (state == WeatherState::CACHED || state == WeatherState::STALE) {
            display.drawText("No fresh data", 0, 20, 1);
            if (systemStatus.lastWeatherUpdateTs > 0) {
                char info[24];
                snprintf(info, sizeof(info), "Last ok: %lus", (unsigned long)systemStatus.lastWeatherUpdateTs);
                display.drawText(info, 0, 32, 1);
            }
        } else if (!wifi.isConnected()) {
            display.drawText("No WiFi", 0, 20, 1);
            display.drawText("connection", 0, 32, 1);
        } else {
            display.drawText("No data", 0, 20, 1);
            display.drawText("available", 0, 32, 1);
        }

        display.update();
        return;
    }

    const WeatherForecast& forecast = weatherService.getForecast();
    const GeoLocation& location = weatherService.getLocation();

    if (weatherViewPage == 0) {
        display.drawText(location.city, 0, 0, 1);

        for (int i = 0; i < forecast.dayCount && i < 4; i++) {
            int y = 14 + (i * 12);

            const uint8_t* icon = getWeatherIcon(forecast.days[i].symbolCode);
            display.drawBitmap(icon, 0, y, 8, 8, 1);

            char dayNum[4];
            strncpy(dayNum, &forecast.days[i].date[8], 2);
            dayNum[2] = '\0';
            display.drawText(dayNum, 12, y, 1);

            char tempStr[16];
            snprintf(tempStr, sizeof(tempStr), "%.0f/%.0f",
                    forecast.days[i].tempMin, forecast.days[i].tempMax);
            display.drawText(tempStr, 30, y, 1);

            char humStr[8];
            snprintf(humStr, sizeof(humStr), "%.0f%%", forecast.days[i].humidity);
            display.drawText(humStr, 80, y, 1);
        }

        display.drawText("Rotate: details", 0, 56, 1);

    } else {
        uint8_t dayIdx = weatherViewPage - 1;
        if (dayIdx >= forecast.dayCount) {
            weatherViewPage = 0;
            return;
        }

        const DailyForecast& day = forecast.days[dayIdx];

        display.drawText(day.date, 0, 0, 1);

        const uint8_t* icon = getWeatherIcon(day.symbolCode);
        display.drawBitmap(icon, 60, 0, 8, 8, 1);

        char tempLine[32];
        snprintf(tempLine, sizeof(tempLine), "Temp: %.1f - %.1f C",
                day.tempMin, day.tempMax);
        display.drawText(tempLine, 0, 16, 1);

        char humLine[20];
        snprintf(humLine, sizeof(humLine), "Humidity: %.0f%%", day.humidity);
        display.drawText(humLine, 0, 28, 1);

        display.drawText("Cond:", 0, 40, 1);
        char symbolShort[16];
        strncpy(symbolShort, day.symbolCode, 15);
        symbolShort[15] = '\0';
        display.drawText(symbolShort, 36, 40, 1);

        char navHint[24];
        snprintf(navHint, sizeof(navHint), "Day %d/%d  Rotate:nav",
                dayIdx + 1, forecast.dayCount);
        display.drawText(navHint, 0, 56, 1);
    }

    display.update();
}

void updateWeatherAboutMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 500) return;
    lastUpdate = millis();

    display.clear();

    display.showTextCentered("Weather Data", 0, 1);
    display.drawText("Provided by", 0, 16, 1);
    display.drawText("MET Norway", 0, 28, 1);
    display.drawText("yr.no", 0, 40, 1);

    display.update();
}

void updateWeatherPrivacyMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 500) return;
    lastUpdate = millis();

    display.clear();

    display.showTextCentered("Privacy Info", 0, 1);
    display.drawText("Weather uses", 0, 14, 1);
    display.drawText("IP geolocation", 0, 24, 1);
    display.drawText("for city-level", 0, 34, 1);
    display.drawText("location only.", 0, 44, 1);

    display.update();
}
