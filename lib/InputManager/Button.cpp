/**
 * @file Button.cpp
 * @brief Implementation of Button class
 */

#include "Button.h"

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

Button::Button(uint8_t pin, bool activeLow, bool enablePullup)
    : _pin(pin),
      _activeLow(activeLow),
      _pullupEnabled(enablePullup),
      _currentState(false),
      _lastState(false),
      _debouncedState(false),
      _lastEvent(ButtonEvent::NONE),
      _lastDebounceTime(0),
      _pressedTime(0),
      _releasedTime(0),
      _lastClickTime(0),
      _debounceDelay(50),
      _longPressThreshold(1000),
      _doubleClickWindow(300),
      _pressedEdge(false),
      _releasedEdge(false),
      _longPressTriggered(false),
      _clickCount(0),
      _callback(nullptr)
{
}

void Button::begin() {
    if (_pullupEnabled) {
        pinMode(_pin, INPUT_PULLUP);
    } else {
        pinMode(_pin, INPUT);
    }
    
    // Read initial state
    _currentState = readRawState();
    _lastState = _currentState;
    _debouncedState = _currentState;
}

// ============================================================================
// MAIN UPDATE METHOD
// ============================================================================

void Button::update() {
    bool rawState = readRawState();
    unsigned long currentTime = millis();
    
    // Reset edge flags
    _pressedEdge = false;
    _releasedEdge = false;
    _lastEvent = ButtonEvent::NONE;
    
    // Debounce logic
    if (rawState != _lastState) {
        _lastDebounceTime = currentTime;
    }
    
    if ((currentTime - _lastDebounceTime) > _debounceDelay) {
        // State has stabilized
        if (rawState != _debouncedState) {
            _debouncedState = rawState;
            
            if (_debouncedState) {
                // Button pressed
                _pressedEdge = true;
                _pressedTime = currentTime;
                _longPressTriggered = false;
                
                _lastEvent = ButtonEvent::PRESSED;
                triggerCallback(ButtonEvent::PRESSED);
                
            } else {
                // Button released
                _releasedEdge = true;
                _releasedTime = currentTime;
                
                _lastEvent = ButtonEvent::RELEASED;
                triggerCallback(ButtonEvent::RELEASED);
                
                // Detect click type
                if (!_longPressTriggered) {
                    detectEvents();
                }
            }
        }
    }
    
    // Check for long press (while button is held)
    if (_debouncedState && !_longPressTriggered) {
        if ((currentTime - _pressedTime) >= _longPressThreshold) {
            _longPressTriggered = true;
            _lastEvent = ButtonEvent::LONG_PRESS;
            triggerCallback(ButtonEvent::LONG_PRESS);
        }
    }
    
    // Long press hold (continuous while held)
    if (_debouncedState && _longPressTriggered) {
        _lastEvent = ButtonEvent::LONG_PRESS_HOLD;
        // Don't trigger callback continuously to avoid spam
    }
    
    _lastState = rawState;
}

// ============================================================================
// STATE QUERIES
// ============================================================================

bool Button::isPressed() const {
    return _debouncedState;
}

bool Button::wasPressed() {
    bool result = _pressedEdge;
    _pressedEdge = false;  // Clear flag after reading
    return result;
}

bool Button::wasReleased() {
    bool result = _releasedEdge;
    _releasedEdge = false;
    return result;
}

ButtonEvent Button::getEvent() {
    return _lastEvent;
}

unsigned long Button::getPressedDuration() const {
    if (_debouncedState) {
        return millis() - _pressedTime;
    }
    return 0;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void Button::setCallback(ButtonCallback callback) {
    _callback = callback;
}

void Button::setTiming(uint16_t debounceMs, uint16_t longPressMs, uint16_t doubleClickMs) {
    _debounceDelay = debounceMs;
    _longPressThreshold = longPressMs;
    _doubleClickWindow = doubleClickMs;
}

void Button::reset() {
    _currentState = false;
    _lastState = false;
    _debouncedState = false;
    _lastEvent = ButtonEvent::NONE;
    _pressedEdge = false;
    _releasedEdge = false;
    _longPressTriggered = false;
    _clickCount = 0;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool Button::readRawState() {
    bool state = digitalRead(_pin);
    return _activeLow ? !state : state;
}

void Button::detectEvents() {
    unsigned long currentTime = millis();
    unsigned long timeSinceLastClick = currentTime - _lastClickTime;
    
    if (timeSinceLastClick < _doubleClickWindow && _clickCount == 1) {
        // Double click detected
        _clickCount = 0;
        _lastEvent = ButtonEvent::DOUBLE_CLICK;
        triggerCallback(ButtonEvent::DOUBLE_CLICK);
    } else {
        // Single click detected (will become double if another comes soon)
        _clickCount = 1;
        _lastClickTime = currentTime;
        _lastEvent = ButtonEvent::CLICK;
        triggerCallback(ButtonEvent::CLICK);
    }
}

void Button::triggerCallback(ButtonEvent event) {
    if (_callback != nullptr) {
        _callback(event);
    }
}