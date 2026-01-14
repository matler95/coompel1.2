/**
 * @file Button.h
 * @brief Professional button handling with debouncing and event detection
 * @version 1.0.0
 * 
 * Features:
 * - Software debouncing (prevents false triggers)
 * - Click type detection (single, double, long press)
 * - Event callbacks (function pointers)
 * - Non-blocking operation
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

// ============================================================================
// BUTTON EVENT TYPES
// ============================================================================
enum class ButtonEvent {
    NONE,
    PRESSED,        // Button just pressed down
    RELEASED,       // Button just released
    CLICK,          // Single click completed
    DOUBLE_CLICK,   // Double click detected
    LONG_PRESS,     // Long press detected
    LONG_PRESS_HOLD // Continuous long press (repeating)
};

// ============================================================================
// CALLBACK FUNCTION TYPE
// ============================================================================
// Modern C++ pattern: function pointer for callbacks
typedef void (*ButtonCallback)(ButtonEvent event);

// ============================================================================
// BUTTON CLASS
// ============================================================================
class Button {
public:
    /**
     * @brief Constructor
     * @param pin GPIO pin number
     * @param activeLow true if button connects to GND (default), false if to VCC
     * @param enablePullup true to enable internal pull-up resistor
     */
    Button(uint8_t pin, bool activeLow = true, bool enablePullup = true);
    
    /**
     * @brief Initialize button hardware
     */
    void begin();
    
    /**
     * @brief Update button state (call in loop)
     * Must be called frequently for accurate timing
     */
    void update();
    
    /**
     * @brief Check if button is currently pressed
     * @return true if pressed
     */
    bool isPressed() const;
    
    /**
     * @brief Check if button was just pressed (edge detection)
     * @return true on press edge
     */
    bool wasPressed();
    
    /**
     * @brief Check if button was just released
     * @return true on release edge
     */
    bool wasReleased();
    
    /**
     * @brief Get last detected event
     * @return ButtonEvent type
     */
    ButtonEvent getEvent();
    
    /**
     * @brief Register callback for button events
     * @param callback Function to call when event occurs
     */
    void setCallback(ButtonCallback callback);
    
    /**
     * @brief Configure timing parameters
     * @param debounceMs Debounce delay in milliseconds
     * @param longPressMs Long press threshold in milliseconds
     * @param doubleClickMs Max time between clicks for double-click
     */
    void setTiming(uint16_t debounceMs, uint16_t longPressMs, uint16_t doubleClickMs);
    
    /**
     * @brief Get how long button has been held
     * @return Duration in milliseconds
     */
    unsigned long getPressedDuration() const;
    
    /**
     * @brief Reset button state
     */
    void reset();

private:
    // Hardware configuration
    uint8_t _pin;
    bool _activeLow;
    bool _pullupEnabled;
    
    // State tracking
    bool _currentState;           // Raw current state
    bool _lastState;              // Previous debounced state
    bool _debouncedState;         // Debounced state
    ButtonEvent _lastEvent;       // Last event detected
    
    // Timing
    unsigned long _lastDebounceTime;
    unsigned long _pressedTime;
    unsigned long _releasedTime;
    unsigned long _lastClickTime;
    
    // Configuration
    uint16_t _debounceDelay;      // Debounce time (ms)
    uint16_t _longPressThreshold; // Long press time (ms)
    uint16_t _doubleClickWindow;  // Double click max time (ms)
    
    // Flags
    volatile bool _pressedEdge;
    volatile bool _releasedEdge;
    bool _longPressTriggered;
    uint8_t _clickCount;
    
    // Callback
    ButtonCallback _callback;
    
    // Private methods
    bool readRawState();
    void detectEvents();
    void triggerCallback(ButtonEvent event);
};

#endif // BUTTON_H