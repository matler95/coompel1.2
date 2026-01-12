/**
 * @file TouchSensor.cpp
 * @brief Implementation of TouchSensor class
 */

#include "TouchSensor.h"

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

TouchSensor::TouchSensor(uint8_t pin)
    : _pin(pin),
      _enabled(true),
      _currentState(false),
      _lastState(false),
      _debouncedState(false),
      _lastEvent(TouchEvent::NONE),
      _lastDebounceTime(0),
      _touchedTime(0),
      _releasedTime(0),
      _lastTapTime(0),
      _debounceDelay(20),        // TTP223 is fast, less debounce needed
      _longTouchThreshold(800),   // 800ms for long touch
      _doubleTapWindow(400),      // 400ms window for double tap
      _touchedEdge(false),
      _releasedEdge(false),
      _longTouchTriggered(false),
      _tapCount(0),
      _callback(nullptr)
{
}

void TouchSensor::begin(bool enablePulldown) {
    // TTP223 output is HIGH when touched, LOW when not
    // No pull resistor needed (sensor has output driver)
    pinMode(_pin, INPUT);
    
    // Read initial state
    _currentState = readRawState();
    _lastState = _currentState;
    _debouncedState = _currentState;
    
    Serial.printf("[TOUCH] Initialized on GPIO%d\n", _pin);
}

// ============================================================================
// MAIN UPDATE METHOD
// ============================================================================

void TouchSensor::update() {
    if (!_enabled) return;
    
    bool rawState = readRawState();
    unsigned long currentTime = millis();
    
    // Reset edge flags
    _touchedEdge = false;
    _releasedEdge = false;
    _lastEvent = TouchEvent::NONE;
    
    // Debounce logic
    if (rawState != _lastState) {
        _lastDebounceTime = currentTime;
    }
    
    if ((currentTime - _lastDebounceTime) > _debounceDelay) {
        // State has stabilized
        if (rawState != _debouncedState) {
            _debouncedState = rawState;
            
            if (_debouncedState) {
                // Touch detected
                _touchedEdge = true;
                _touchedTime = currentTime;
                _longTouchTriggered = false;
                
                _lastEvent = TouchEvent::TOUCH;
                triggerCallback(TouchEvent::TOUCH);
                
            } else {
                // Release detected
                _releasedEdge = true;
                _releasedTime = currentTime;
                
                _lastEvent = TouchEvent::RELEASE;
                triggerCallback(TouchEvent::RELEASE);
                
                // Detect tap type
                if (!_longTouchTriggered) {
                    detectEvents();
                }
            }
        }
    }
    
    // Check for long touch (while touched)
    if (_debouncedState && !_longTouchTriggered) {
        if ((currentTime - _touchedTime) >= _longTouchThreshold) {
            _longTouchTriggered = true;
            _lastEvent = TouchEvent::LONG_TOUCH;
            triggerCallback(TouchEvent::LONG_TOUCH);
        }
    }
    
    _lastState = rawState;
}

// ============================================================================
// STATE QUERIES
// ============================================================================

bool TouchSensor::isTouched() const {
    return _debouncedState;
}

bool TouchSensor::wasTouched() {
    bool result = _touchedEdge;
    _touchedEdge = false;
    return result;
}

bool TouchSensor::wasReleased() {
    bool result = _releasedEdge;
    _releasedEdge = false;
    return result;
}

TouchEvent TouchSensor::getEvent() {
    return _lastEvent;
}

unsigned long TouchSensor::getTouchedDuration() const {
    if (_debouncedState) {
        return millis() - _touchedTime;
    }
    return 0;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void TouchSensor::setCallback(TouchCallback callback) {
    _callback = callback;
}

void TouchSensor::setTiming(uint16_t debounceMs, uint16_t longTouchMs, uint16_t doubleTapMs) {
    _debounceDelay = debounceMs;
    _longTouchThreshold = longTouchMs;
    _doubleTapWindow = doubleTapMs;
}

void TouchSensor::setEnabled(bool enabled) {
    _enabled = enabled;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool TouchSensor::readRawState() {
    // TTP223 output: HIGH = touched, LOW = not touched
    return digitalRead(_pin) == HIGH;
}

void TouchSensor::detectEvents() {
    unsigned long currentTime = millis();
    unsigned long timeSinceLastTap = currentTime - _lastTapTime;
    
    if (timeSinceLastTap < _doubleTapWindow && _tapCount == 1) {
        // Double tap detected
        _tapCount = 0;
        _lastEvent = TouchEvent::DOUBLE_TAP;
        triggerCallback(TouchEvent::DOUBLE_TAP);
    } else {
        // Single tap detected
        _tapCount = 1;
        _lastTapTime = currentTime;
        _lastEvent = TouchEvent::TAP;
        triggerCallback(TouchEvent::TAP);
    }
}

void TouchSensor::triggerCallback(TouchEvent event) {
    if (_callback != nullptr) {
        _callback(event);
    }
}