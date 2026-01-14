#ifndef WIFI_ICONS_H
#define WIFI_ICONS_H

#include <Arduino.h>

// WiFi Connected Icon (8x8 pixels) - 3 signal bars
const unsigned char PROGMEM wifi_connected[] = {
    0b00000000,
    0b00111100,
    0b01000010,
    0b10011001,
    0b00100100,
    0b00011000,
    0b00000000,
    0b00011000
};

// WiFi Disconnected Icon (8x8 pixels) - X symbol
const unsigned char PROGMEM wifi_disconnected[] = {
    0b00000000,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00011000,
    0b00100100,
    0b01000010,
    0b00000000
};

// WiFi AP Mode Icon (8x8 pixels) - Broadcast/antenna symbol
const unsigned char PROGMEM wifi_ap[] = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b01011010,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00111100
};

// WiFi Connecting Icon (8x8 pixels) - Partial bars
const unsigned char PROGMEM wifi_connecting[] = {
    0b00000000,
    0b00111100,
    0b01000010,
    0b10000001,
    0b00000000,
    0b00011000,
    0b00000000,
    0b00011000
};

#endif // WIFI_ICONS_H
