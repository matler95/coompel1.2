/**
 * @file RotaryEncoder.cpp
 * @brief Implementation of RotaryEncoder class
 */

#include "RotaryEncoder.h"

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

RotaryEncoder::RotaryEncoder(uint8_t clkPin, uint8_t dtPin, uint8_t swPin, uint8_t stepsPerDetent)
    : _clkPin(clkPin),
      _dtPin(dtPin),
      _swPin(swPin),
      _stepsPerDetent(stepsPerDetent),
      _position(0),
      _lastReportedPosition(0),
      _lastEncoded(0),
      _direction(EncoderDirection::NONE),
      _lastEvent(EncoderEvent::NONE),
      _stepAccumulator(0),
      _accelerationEnabled(false),
      _accelerationThreshold(50),
      _lastRotationTime(0),
      _accelerationFactor(1),
      _button(nullptr),
      _callback(nullptr)
{
}

void RotaryEncoder::begin() {
    // Configure encoder pins with pull-ups
    pinMode(_clkPin, INPUT_PULLUP);
    pinMode(_dtPin, INPUT_PULLUP);

    // Initialize button
    _button = new Button(_swPin, true, true);
    _button->begin();

    // Read initial state
    _lastEncoded = getEncodedState();

    Serial.printf("[ENCODER] Initialized on CLK=%d, DT=%d, SW=%d\n", _clkPin, _dtPin, _swPin);
}

// ============================================================================
// UPDATE METHOD
// ============================================================================

void RotaryEncoder::update() {
    readEncoder();
    updateButton();
}

// ============================================================================
// ENCODER READING
// ============================================================================

uint8_t RotaryEncoder::getEncodedState() {
    // Read both pins and encode as 2-bit value
    uint8_t clk = digitalRead(_clkPin);
    uint8_t dt = digitalRead(_dtPin);
    return (clk << 1) | dt;
}

void RotaryEncoder::readEncoder() {
    // State machine lookup table for reliable quadrature decoding
    // Each entry represents the direction for a given state transition
    // Format: [previous_state][current_state] = direction
    static const int8_t STATE_TABLE[4][4] = {
        // 00    01    10    11
        {  0,    1,   -1,    0 },  // 00
        { -1,    0,    0,    1 },  // 01
        {  1,    0,    0,   -1 },  // 10
        {  0,   -1,    1,    0 }   // 11
    };

    uint8_t encoded = getEncodedState();

    // Check if state changed
    if (encoded == _lastEncoded) {
        return;
    }

    // Get direction from state table
    int8_t direction = STATE_TABLE[_lastEncoded][encoded];

    _lastEncoded = encoded;

    // Update position only when we have a valid transition
    if (direction != 0) {
        // Update position (apply steps per detent)
        _stepAccumulator += direction;

        // Only update position when we've accumulated EXACTLY enough steps for a full detent
        // This ensures we only trigger on complete detent clicks
        if (abs(_stepAccumulator) >= _stepsPerDetent) {
            // Calculate exactly how many complete detents we have
            int8_t detents = _stepAccumulator / _stepsPerDetent;

            // Keep only the remainder for the next cycle
            _stepAccumulator = _stepAccumulator % _stepsPerDetent;

            // Only process exactly ONE detent at a time to prevent double-counting
            if (detents > 0) {
                detents = 1;  // Force to exactly 1
            } else if (detents < 0) {
                detents = -1; // Force to exactly -1
            }

            // Calculate step size with acceleration
            uint8_t stepSize = 1;
            if (_accelerationEnabled) {
                unsigned long currentTime = millis();
                unsigned long timeDelta = currentTime - _lastRotationTime;

                if (timeDelta < _accelerationThreshold) {
                    _accelerationFactor = min((int)(_accelerationFactor + 1), 8);
                    stepSize = _accelerationFactor;
                } else {
                    _accelerationFactor = 1;
                }

                _lastRotationTime = currentTime;
            }

            _position += detents * stepSize;

            // Update direction and trigger events
            if (detents > 0) {
                _direction = EncoderDirection::CLOCKWISE;
                _lastEvent = EncoderEvent::ROTATED_CW;
                triggerCallback(EncoderEvent::ROTATED_CW);
            } else if (detents < 0) {
                _direction = EncoderDirection::COUNTER_CLOCKWISE;
                _lastEvent = EncoderEvent::ROTATED_CCW;
                triggerCallback(EncoderEvent::ROTATED_CCW);
            }
        }
    }
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

void RotaryEncoder::updateButton() {
    if (_button == nullptr) return;

    _button->update();

    // Get button event and convert to encoder event
    ButtonEvent btnEvent = _button->getEvent();

    switch (btnEvent) {
        case ButtonEvent::PRESSED:
            _lastEvent = EncoderEvent::BUTTON_PRESSED;
            triggerCallback(EncoderEvent::BUTTON_PRESSED);
            break;

        case ButtonEvent::RELEASED:
            _lastEvent = EncoderEvent::BUTTON_RELEASED;
            triggerCallback(EncoderEvent::BUTTON_RELEASED);
            break;

        case ButtonEvent::CLICK:
            _lastEvent = EncoderEvent::BUTTON_CLICK;
            triggerCallback(EncoderEvent::BUTTON_CLICK);
            break;

        case ButtonEvent::DOUBLE_CLICK:
            _lastEvent = EncoderEvent::BUTTON_DOUBLE_CLICK;
            triggerCallback(EncoderEvent::BUTTON_DOUBLE_CLICK);
            break;

        case ButtonEvent::LONG_PRESS:
            _lastEvent = EncoderEvent::BUTTON_LONG_PRESS;
            triggerCallback(EncoderEvent::BUTTON_LONG_PRESS);
            break;

        default:
            break;
    }
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

int32_t RotaryEncoder::getPosition() const {
    return _position;
}

int32_t RotaryEncoder::getDelta() {
    int32_t delta = _position - _lastReportedPosition;
    _lastReportedPosition = _position;
    return delta;
}

EncoderDirection RotaryEncoder::getDirection() const {
    return _direction;
}

void RotaryEncoder::setPosition(int32_t position) {
    _position = position;
    _lastReportedPosition = position;
}

void RotaryEncoder::reset() {
    _position = 0;
    _lastReportedPosition = 0;
    _direction = EncoderDirection::NONE;
}

Button* RotaryEncoder::getButton() {
    return _button;
}

bool RotaryEncoder::isRotating() const {
    return _direction != EncoderDirection::NONE;
}

EncoderEvent RotaryEncoder::getEvent() {
    EncoderEvent event = _lastEvent;
    _lastEvent = EncoderEvent::NONE;
    return event;
}

void RotaryEncoder::setCallback(EncoderCallback callback) {
    _callback = callback;
}

void RotaryEncoder::setAcceleration(bool enabled, uint16_t threshold) {
    _accelerationEnabled = enabled;
    _accelerationThreshold = threshold;
    _accelerationFactor = 1;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void RotaryEncoder::triggerCallback(EncoderEvent event) {
    if (_callback != nullptr) {
        _callback(event, _position);
    }
}
