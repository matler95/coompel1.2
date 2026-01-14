#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <Arduino.h>

// ============================================================================
// Weather Icons (8x8 pixels) for OLED display
// Based on MET Norway / yr.no symbol codes
// ============================================================================

// Clear sky - Sun icon
//   ░░█░░█░░
//   ░░░█░░░░
//   █░█████░
//   ░░█████░
//   █░█████░
//   ░░░█░░░░
//   ░░█░░█░░
//   ░░░░░░░░
const uint8_t PROGMEM icon_clearsky_day[] = {
    0b00100100,
    0b00010000,
    0b10111110,
    0b00111110,
    0b10111110,
    0b00010000,
    0b00100100,
    0b00000000
};

// Clear sky night - Moon icon
//   ░░░░░░░░
//   ░░██░░░░
//   ░███░░░░
//   █████░░░
//   █████░░░
//   ░███░░░░
//   ░░██░░░░
//   ░░░░░░░░
const uint8_t PROGMEM icon_clearsky_night[] = {
    0b00000000,
    0b00110000,
    0b01110000,
    0b11111000,
    0b11111000,
    0b01110000,
    0b00110000,
    0b00000000
};

// Partly cloudy day - Sun with cloud
//   ░█░░░░░░
//   █░░░░░░░
//   ░░░██░░░
//   ░░██████
//   ░████████
//   ░████████
//   ░░██████░
//   ░░░░░░░░
const uint8_t PROGMEM icon_partlycloudy_day[] = {
    0b01000000,
    0b10000000,
    0b00011000,
    0b00111111,
    0b01111111,
    0b01111111,
    0b00111110,
    0b00000000
};

// Partly cloudy night - Moon with cloud
//   ░░█░░░░░
//   ░█░░░░░░
//   ░░░██░░░
//   ░░██████
//   ░████████
//   ░████████
//   ░░██████░
//   ░░░░░░░░
const uint8_t PROGMEM icon_partlycloudy_night[] = {
    0b00100000,
    0b01000000,
    0b00011000,
    0b00111111,
    0b01111111,
    0b01111111,
    0b00111110,
    0b00000000
};

// Cloudy - Full cloud
//   ░░░░░░░░
//   ░░░██░░░
//   ░░██████
//   ░████████
//   ████████
//   ████████
//   ░██████░
//   ░░░░░░░░
const uint8_t PROGMEM icon_cloudy[] = {
    0b00000000,
    0b00011000,
    0b00111100,
    0b01111110,
    0b11111111,
    0b11111111,
    0b01111110,
    0b00000000
};

// Fair day (slightly cloudy) - Small sun, small cloud
//   ░░█░░░░░
//   █░█░░░░░
//   ░███░░░░
//   ░░░░░░░░
//   ░░░░██░░
//   ░░░██████
//   ░░░██████
//   ░░░░░░░░
const uint8_t PROGMEM icon_fair_day[] = {
    0b00100000,
    0b10100000,
    0b01110000,
    0b00000000,
    0b00001100,
    0b00011111,
    0b00011111,
    0b00000000
};

// Fair night - Moon with small cloud
//   ░░██░░░░
//   ░███░░░░
//   ░░██░░░░
//   ░░░░░░░░
//   ░░░░██░░
//   ░░░██████
//   ░░░██████
//   ░░░░░░░░
const uint8_t PROGMEM icon_fair_night[] = {
    0b00110000,
    0b01110000,
    0b00110000,
    0b00000000,
    0b00001100,
    0b00011111,
    0b00011111,
    0b00000000
};

// Rain - Cloud with rain drops
//   ░░░██░░░
//   ░░██████
//   ████████
//   ████████
//   ░░░░░░░░
//   ░█░█░█░░
//   █░█░█░░░
//   ░░░░░░░░
const uint8_t PROGMEM icon_rain[] = {
    0b00011000,
    0b00111100,
    0b11111111,
    0b11111111,
    0b00000000,
    0b01010100,
    0b10101000,
    0b00000000
};

// Light rain - Cloud with light drops
//   ░░░██░░░
//   ░░██████
//   ████████
//   ████████
//   ░░░░░░░░
//   ░░█░░█░░
//   ░░░░░░░░
//   ░░░░░░░░
const uint8_t PROGMEM icon_lightrain[] = {
    0b00011000,
    0b00111100,
    0b11111111,
    0b11111111,
    0b00000000,
    0b00100100,
    0b00000000,
    0b00000000
};

// Heavy rain - Cloud with heavy drops
//   ░░░██░░░
//   ░░██████
//   ████████
//   ████████
//   █░█░█░█░
//   ░█░█░█░█
//   █░█░█░█░
//   ░░░░░░░░
const uint8_t PROGMEM icon_heavyrain[] = {
    0b00011000,
    0b00111100,
    0b11111111,
    0b11111111,
    0b10101010,
    0b01010101,
    0b10101010,
    0b00000000
};

// Snow - Cloud with snowflakes
//   ░░░██░░░
//   ░░██████
//   ████████
//   ████████
//   ░░░░░░░░
//   ░█░░░█░░
//   ░░░█░░░░
//   ░█░░░█░░
const uint8_t PROGMEM icon_snow[] = {
    0b00011000,
    0b00111100,
    0b11111111,
    0b11111111,
    0b00000000,
    0b01000100,
    0b00010000,
    0b01000100
};

// Light snow
//   ░░░██░░░
//   ░░██████
//   ████████
//   ████████
//   ░░░░░░░░
//   ░░█░░░░░
//   ░░░░░█░░
//   ░░░░░░░░
const uint8_t PROGMEM icon_lightsnow[] = {
    0b00011000,
    0b00111100,
    0b11111111,
    0b11111111,
    0b00000000,
    0b00100000,
    0b00000100,
    0b00000000
};

// Heavy snow
//   ░░░██░░░
//   ░░██████
//   ████████
//   ████████
//   █░█░█░█░
//   ░█░█░█░█
//   █░█░█░█░
//   ░░░░░░░░
const uint8_t PROGMEM icon_heavysnow[] = {
    0b00011000,
    0b00111100,
    0b11111111,
    0b11111111,
    0b10101010,
    0b01010101,
    0b10101010,
    0b00000000
};

// Sleet - Rain and snow mixed
//   ░░░██░░░
//   ░░██████
//   ████████
//   ████████
//   ░░░░░░░░
//   ░█░░█░░░
//   ░░█░░░█░
//   ░░░░░░░░
const uint8_t PROGMEM icon_sleet[] = {
    0b00011000,
    0b00111100,
    0b11111111,
    0b11111111,
    0b00000000,
    0b01001000,
    0b00100010,
    0b00000000
};

// Fog - Horizontal lines
//   ░░░░░░░░
//   ████████
//   ░░░░░░░░
//   ░██████░
//   ░░░░░░░░
//   ████████
//   ░░░░░░░░
//   ░██████░
const uint8_t PROGMEM icon_fog[] = {
    0b00000000,
    0b11111111,
    0b00000000,
    0b01111110,
    0b00000000,
    0b11111111,
    0b00000000,
    0b01111110
};

// Thunder - Cloud with lightning bolt
//   ░░░██░░░
//   ░░██████
//   ████████
//   ░░░░█░░░
//   ░░░██░░░
//   ░░░█░░░░
//   ░░██░░░░
//   ░░░░░░░░
const uint8_t PROGMEM icon_thunder[] = {
    0b00011000,
    0b00111100,
    0b11111111,
    0b00001000,
    0b00011000,
    0b00010000,
    0b00110000,
    0b00000000
};

// Unknown weather - Question mark
//   ░░████░░
//   ░█░░░░█░
//   ░░░░░█░░
//   ░░░░█░░░
//   ░░░█░░░░
//   ░░░░░░░░
//   ░░░█░░░░
//   ░░░░░░░░
const uint8_t PROGMEM icon_unknown[] = {
    0b00111100,
    0b01000010,
    0b00000100,
    0b00001000,
    0b00010000,
    0b00000000,
    0b00010000,
    0b00000000
};

// ============================================================================
// Icon lookup function
// Returns pointer to appropriate icon based on MET Norway symbol code
// ============================================================================

inline const uint8_t* getWeatherIcon(const char* symbolCode) {
    if (symbolCode == nullptr || symbolCode[0] == '\0') {
        return icon_unknown;
    }

    // Check for specific patterns in symbol code
    // MET Norway codes: clearsky_day, clearsky_night, cloudy, partlycloudy_day, etc.

    // Thunder variants (check first as they may contain rain/snow)
    if (strstr(symbolCode, "thunder")) {
        return icon_thunder;
    }

    // Fog
    if (strstr(symbolCode, "fog")) {
        return icon_fog;
    }

    // Heavy rain variants
    if (strstr(symbolCode, "heavyrain")) {
        return icon_heavyrain;
    }

    // Light rain variants
    if (strstr(symbolCode, "lightrain")) {
        return icon_lightrain;
    }

    // Rain variants (check after heavy/light)
    if (strstr(symbolCode, "rain")) {
        return icon_rain;
    }

    // Heavy snow variants
    if (strstr(symbolCode, "heavysnow")) {
        return icon_heavysnow;
    }

    // Light snow variants
    if (strstr(symbolCode, "lightsnow")) {
        return icon_lightsnow;
    }

    // Snow variants (check after heavy/light)
    if (strstr(symbolCode, "snow")) {
        return icon_snow;
    }

    // Sleet variants
    if (strstr(symbolCode, "sleet")) {
        return icon_sleet;
    }

    // Clear sky
    if (strstr(symbolCode, "clearsky")) {
        if (strstr(symbolCode, "night") || strstr(symbolCode, "polartwilight")) {
            return icon_clearsky_night;
        }
        return icon_clearsky_day;
    }

    // Fair (slightly cloudy)
    if (strstr(symbolCode, "fair")) {
        if (strstr(symbolCode, "night") || strstr(symbolCode, "polartwilight")) {
            return icon_fair_night;
        }
        return icon_fair_day;
    }

    // Partly cloudy
    if (strstr(symbolCode, "partlycloudy")) {
        if (strstr(symbolCode, "night") || strstr(symbolCode, "polartwilight")) {
            return icon_partlycloudy_night;
        }
        return icon_partlycloudy_day;
    }

    // Cloudy (check last as other conditions contain cloud references)
    if (strstr(symbolCode, "cloudy")) {
        return icon_cloudy;
    }

    // Default to unknown
    return icon_unknown;
}

#endif // WEATHER_ICONS_H
