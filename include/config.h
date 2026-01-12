/**
 * @file config.h
 * @brief Hardware configuration and pin definitions
 * 
 * This file contains all hardware-specific configurations
 * for the ESP32-C3 Interactive Device project.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// PROJECT INFORMATION
// ============================================================================
#define PROJECT_NAME "ESP32-C3 Interactive Device"
#define HARDWARE_VERSION "1.0"
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.1.0"  // Fallback if not set in build flags
#endif

// ============================================================================
// DISPLAY CONFIGURATION (SH1106 OLED)
// ============================================================================
#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      64
#define DISPLAY_I2C_ADDRESS 0x3C  // Common address (try 0x3D if not working)

// I2C Pins for Display & Sensors
#define I2C_SDA_PIN         6     // GPIO8 (adjust for your board)
#define I2C_SCL_PIN         7     // GPIO9 (adjust for your board)
#define I2C_FREQUENCY       400000 // 400kHz (fast mode)

// ============================================================================
// INPUT CONFIGURATION
// ============================================================================
// Buttons
#define BTN_SELECT_PIN      2     // Main select button
#define BTN_BACK_PIN        3     // Back/cancel button
#define BTN_DEBOUNCE_MS     50    // Debounce delay
#define BTN_LONGPRESS_MS    1000  // Long press threshold

// ============================================================================
// MOTION SENSOR (MPU6050)
// ============================================================================
#define MPU6050_I2C_ADDRESS 0x68  // Default address (0x69 if AD0=HIGH)
#define MOTION_INT_PIN      10    // Interrupt pin for motion events

// Gesture Detection Thresholds
#define ACCEL_THRESHOLD_TAP     2.5f   // G-force for tap detection
#define ACCEL_THRESHOLD_SHAKE   8.0f   // G-force for shake
#define GYRO_THRESHOLD_TILT     100.0f // deg/s for orientation change

// ============================================================================
// ANALOG SENSORS
// ============================================================================
#define SOUND_SENSOR_PIN    0     // HW-484 analog output
#define POTENTIOMETER_PIN   1     // Brightness/volume control
#define DHT11_PIN           4     // Digital pin for DHT11

// ============================================================================
// AUDIO FEEDBACK
// ============================================================================
#define BUZZER_PIN          5     // Piezo buzzer (PWM capable)

// ============================================================================
// STATUS INDICATORS
// ============================================================================
#define LED_STATUS_PIN      8     // Status LED
#define LED_ERROR_PIN       9     // Error indicator LED

// ============================================================================
// POWER MANAGEMENT
// ============================================================================
#define BATTERY_ADC_PIN     ADC1_CHANNEL_0  // Battery voltage monitor
#define SLEEP_TIMEOUT_MS    300000          // 5 minutes idle -> sleep
#define DEEP_SLEEP_TIMEOUT_MS 900000        // 15 min idle -> deep sleep

// ============================================================================
// DEBUG SETTINGS
// ============================================================================
#define SERIAL_BAUD_RATE    115200
#define DEBUG_ENABLED       true

// Debug macros
#if DEBUG_ENABLED
  #define DEBUG_PRINT(x)    Serial.print(x)
  #define DEBUG_PRINTLN(x)  Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

#endif // CONFIG_H