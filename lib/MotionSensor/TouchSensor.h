/**
 * @file TouchSensor.h
 * @brief TTP223 capacitive touch sensor interface
 * @version 1.0.0
 * 
 * Features:
 * - Single and double tap detection
 * - Long touch detection
 * - Works with TTP223 or any digital touch sensor
 * - Falls back to button input if touch not available
 */

#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include <Arduino.h>

// ============================================================================
// TOUCH EVENT TYPES
// ============================================================================
enum class TouchEvent {
    NONE,
    TOUCH,          // Touched
    RELEASE,        // Released
    TAP,            // Single tap
    DOUBLE_TAP,     // Double tap
    LONG_TOUCH      // Long touch
};

// ============================================================================
// CALLBACK TYPE
// ============================================================================
typedef void (*TouchCallback)(TouchEvent event);

// ============================================================================
// TOUCH SENSOR CLASS
// ============================================================================
class TouchSensor {
public:
    /**
     * @brief Constructor
     * @param pin GPIO pin connected to TTP223 output
     */
    TouchSensor(uint8_t pin);
    
    /**
     * @brief Initialize touch sensor
     * @param enablePulldown Enable internal pull-down (if supported)
     */
    void begin(bool enablePulldown = false);
    
    /**
     * @brief Update touch state (call in loop)
     */
    void update();
    
    /**
     * @brief Check if currently touched
     * @return true if touched
     */
    bool isTouched() const;
    
    /**
     * @brief Check if was just touched (edge)
     * @return true on touch edge
     */
    bool wasTouched();
    
    /**
     * @brief Check if was just released
     * @return true on release edge
     */
    bool wasReleased();
    
    /**
     * @brief Get last detected event
     * @return TouchEvent type
     */
    TouchEvent getEvent();
    
    /**
     * @brief Register callback for touch events
     * @param callback Function to call
     */
    void setCallback(TouchCallback callback);
    
    /**
     * @brief Configure timing parameters
     * @param debounceMs Debounce delay
     * @param longTouchMs Long touch threshold
     * @param doubleTapMs Double tap window
     */
    void setTiming(uint16_t debounceMs, uint16_t longTouchMs, uint16_t doubleTapMs);
    
    /**
     * @brief Get touch duration
     * @return Duration in milliseconds
     */
    unsigned long getTouchedDuration() const;
    
    /**
     * @brief Enable/disable touch sensor
     * @param enabled true to enable
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Check if sensor is enabled
     * @return true if enabled
     */
    bool isEnabled() const { return _enabled; }

private:
    uint8_t _pin;
    bool _enabled;
    
    // State tracking
    bool _currentState;
    bool _lastState;
    bool _debouncedState;
    TouchEvent _lastEvent;
    
    // Timing
    unsigned long _lastDebounceTime;
    unsigned long _touchedTime;
    unsigned long _releasedTime;
    unsigned long _lastTapTime;
    
    // Configuration
    uint16_t _debounceDelay;
    uint16_t _longTouchThreshold;
    uint16_t _doubleTapWindow;
    
    // Flags
    bool _touchedEdge;
    bool _releasedEdge;
    bool _longTouchTriggered;
    uint8_t _tapCount;
    
    // Callback
    TouchCallback _callback;
    
    // Private methods
    bool readRawState();
    void detectEvents();
    void triggerCallback(TouchEvent event);
};

#endif // TOUCH_SENSOR_H