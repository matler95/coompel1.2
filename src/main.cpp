/**
 * @file main.cpp
 * @brief ESP32-C3 Interactive Device - Motion + Touch Test
 * @version 0.4.1
 * 
 * Phase 4.1: Motion (shake/tilt) + Touch sensor ready
 */

#include <Arduino.h>
#include "config.h"
#include "DisplayManager.h"
#include "InputManager.h"
#include "MotionSensor.h"
#include "TouchSensor.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
DisplayManager display;
InputManager input;
MotionSensor motion;
TouchSensor touch(TOUCH_SENSOR_PIN);

// ============================================================================
// EVENT TRACKING
// ============================================================================
String lastTouchEvent = "None";
String lastMotionEvent = "None";
uint16_t tapCount = 0;
uint16_t doubleTapCount = 0;
uint16_t shakeCount = 0;
float accelX = 0, accelY = 0, accelZ = 0;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void updateDisplay();
void onMotionEvent(MotionEvent event);
void onTouchEvent(TouchEvent event);
void onButtonEvent(ButtonEvent event);

// ============================================================================
// TOUCH CALLBACK
// ============================================================================

void onTouchEvent(TouchEvent event) {
    switch (event) {
        case TouchEvent::TAP:
            lastTouchEvent = "TAP!";
            tapCount++;
            Serial.println("[TOUCH] Tap!");
            break;
            
        case TouchEvent::DOUBLE_TAP:
            lastTouchEvent = "DOUBLE TAP!!";
            doubleTapCount++;
            Serial.println("[TOUCH] Double tap!");
            break;
            
        case TouchEvent::LONG_TOUCH:
            lastTouchEvent = "LONG TOUCH";
            Serial.println("[TOUCH] Long touch!");
            break;
            
        default:
            break;
    }
}

// ============================================================================
// MOTION CALLBACK
// ============================================================================

void onMotionEvent(MotionEvent event) {
    switch (event) {
        case MotionEvent::SHAKE:
            lastMotionEvent = "SHAKE!!!";
            shakeCount++;
            Serial.println("[MOTION] Shake!");
            break;
            
        default:
            break;
    }
}

// ============================================================================
// BUTTON CALLBACK (Simulate touch when TTP223 not connected)
// ============================================================================

void onButtonEvent(ButtonEvent event) {
    #if !TOUCH_ENABLED
    // Use button to simulate touch events when TTP223 not available
    switch (event) {
        case ButtonEvent::CLICK:
            lastTouchEvent = "TAP (BTN)";
            tapCount++;
            Serial.println("[BUTTON] Simulating tap");
            break;
            
        case ButtonEvent::DOUBLE_CLICK:
            lastTouchEvent = "DBL (BTN)";
            doubleTapCount++;
            Serial.println("[BUTTON] Simulating double tap");
            break;
            
        case ButtonEvent::LONG_PRESS:
            lastTouchEvent = "LONG (BTN)";
            Serial.println("[BUTTON] Simulating long touch");
            break;
            
        default:
            break;
    }
    #else
    // With TTP223, button resets counters
    if (event == ButtonEvent::CLICK) {
        tapCount = 0;
        doubleTapCount = 0;
        shakeCount = 0;
        lastTouchEvent = "Reset";
        lastMotionEvent = "Reset";
    }
    #endif
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("ESP32-C3 Interactive Device v0.4.1");
    Serial.println("Phase 4: Motion + Touch Test");
    Serial.println("========================================\n");
    
    // Initialize display
    if (!display.init(I2C_SDA_PIN, I2C_SCL_PIN)) {
        Serial.println("[ERROR] Display failed!");
        while (1) delay(1000);
    }
    
    // Initialize input
    if (!input.init(BTN_SELECT_PIN, BTN_BACK_PIN)) {
        Serial.println("[ERROR] Input failed!");
        while (1) delay(1000);
    }
    input.setButtonCallback(ButtonID::SELECT, onButtonEvent);
    
    // Initialize motion sensor
    if (!motion.init(&Wire)) {
        Serial.println("[ERROR] Motion sensor failed!");
        display.clear();
        display.showTextCentered("MPU6050", 10, 2);
        display.showTextCentered("NOT FOUND!", 30, 1);
        display.update();
        while (1) delay(1000);
    }
    motion.setCallback(onMotionEvent);
    motion.setShakeThreshold(20.0f);
    
    // Initialize touch sensor
    #if TOUCH_ENABLED
    touch.begin();
    touch.setCallback(onTouchEvent);
    Serial.println("[TOUCH] TTP223 enabled");
    #else
    touch.setEnabled(false);
    Serial.println("[TOUCH] TTP223 disabled (using button simulation)");
    #endif
    
    Serial.println("[INIT] System ready!");
    Serial.println("Try: Shake device, " + String(TOUCH_ENABLED ? "Touch sensor" : "Click button"));
    Serial.println("========================================\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Update all systems
    input.update();
    motion.update();
    
    #if TOUCH_ENABLED
    touch.update();
    #endif
    
    // Get current acceleration data
    motion.getAcceleration(accelX, accelY, accelZ);
    
    // Update display every 100ms
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate >= 100) {
        lastDisplayUpdate = millis();
        updateDisplay();
    }
}

// ============================================================================
// DISPLAY UPDATE
// ============================================================================

void updateDisplay() {
    display.clear();
    
    // Title
    display.drawText("MOTION+TOUCH", 64, 0, 1, TextAlign::CENTER);
    
    // Touch events
    display.drawText("Touch:", 0, 12, 1);
    display.drawText(lastTouchEvent.c_str(), 40, 12, 1);
    
    // Motion events
    display.drawText("Motion:", 0, 22, 1);
    display.drawText(lastMotionEvent.c_str(), 45, 22, 1);
    
    // Statistics
    char stats[32];
    snprintf(stats, sizeof(stats), "T:%d D:%d S:%d", tapCount, doubleTapCount, shakeCount);
    display.drawText(stats, 0, 34, 1);
    
    // Orientation
    Orientation orient = motion.getOrientation();
    const char* orientText = "?";
    switch (orient) {
        case Orientation::FLAT: orientText = "FLAT"; break;
        case Orientation::UPSIDE_DOWN: orientText = "UPSIDE"; break;
        case Orientation::PORTRAIT: orientText = "PORTRT"; break;
        case Orientation::LANDSCAPE_LEFT: orientText = "L-LEFT"; break;
        case Orientation::LANDSCAPE_RIGHT: orientText = "L-RGHT"; break;
        default: orientText = "?"; break;
    }
    
    display.drawText("Orient:", 0, 46, 1);
    display.drawText(orientText, 50, 46, 1);
    
    // Status indicator
    #if TOUCH_ENABLED
    display.drawText("TTP223:ON", 0, 56, 1);
    #else
    display.drawText("BTN Mode", 0, 56, 1);
    #endif
    
    display.update();
}