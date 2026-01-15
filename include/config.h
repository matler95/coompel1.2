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
// Rotary Encoder (KY-040) - replaces potentiometer + select button
#define ENCODER_CLK_PIN         2     // Encoder CLK (output A) - GPIO2/D2
#define ENCODER_DT_PIN          3     // Encoder DT (output B) - GPIO3/D3
#define ENCODER_SW_PIN          4     // Encoder switch (button) - GPIO4
// NOTE: Many KY-040 variants output 2 state changes per physical "click" (detent) with common decoding,
// while others output 4. If you see "2 detents move one line", set this to 2.
#define ENCODER_STEPS_PER_DETENT 2    // Steps per detent (1=very sensitive, 2=common, 4=some variants)

// ============================================================================
// MOTION SENSOR (MPU6050)
// ============================================================================
#define MPU6050_I2C_ADDRESS 0x68  // Default address (0x69 if AD0=HIGH)

// ============================================================================
// TOUCH SENSOR (TTP223)
// ============================================================================
#define TOUCH_SENSOR_PIN    10    // GPIO for TTP223 output (adjust for your board)
#define TOUCH_ENABLED       false // Set to true when TTP223 is connected


// Gesture Detection Thresholds
#define ACCEL_THRESHOLD_SHAKE   8.0f   // G-force for shake

// ============================================================================
// ANALOG SENSORS
// ============================================================================
#define SOUND_SENSOR_PIN    0     // HW-484 analog output - GPIO0/ADC1-0
#define DHT11_PIN           5     // Digital pin for DHT11 - GPIO5/D5

// ============================================================================
// AUDIO FEEDBACK
// ============================================================================
#define BUZZER_PIN          8     // GPIO8 for passive buzzer (PWM capable)


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