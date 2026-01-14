/**
 * @file RotaryEncoder.h
 * @brief KY-040 Rotary Encoder with built-in button support
 * @version 1.0.0
 *
 * Features:
 * - Detent-based rotation tracking (perfect for KY-040 with 20 detents)
 * - Direction detection (CW/CCW)
 * - Built-in button handling
 * - Position tracking (relative and absolute)
 * - Configurable step size and acceleration
 */

#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>
#include "Button.h"

// ============================================================================
// ENCODER DIRECTION
// ============================================================================
enum class EncoderDirection {
    NONE,
    CLOCKWISE,
    COUNTER_CLOCKWISE
};

// ============================================================================
// ENCODER EVENT TYPES
// ============================================================================
enum class EncoderEvent {
    NONE,
    ROTATED_CW,      // Rotated clockwise
    ROTATED_CCW,     // Rotated counter-clockwise
    BUTTON_PRESSED,  // Button pressed
    BUTTON_RELEASED, // Button released
    BUTTON_CLICK,    // Button single click
    BUTTON_DOUBLE_CLICK, // Button double click
    BUTTON_LONG_PRESS    // Button long press
};

// ============================================================================
// CALLBACK FUNCTION TYPE
// ============================================================================
typedef void (*EncoderCallback)(EncoderEvent event, int32_t position);

// ============================================================================
// ROTARY ENCODER CLASS
// ============================================================================
class RotaryEncoder {
public:
    /**
     * @brief Constructor
     * @param clkPin CLK pin (A pin, output A)
     * @param dtPin DT pin (B pin, output B)
     * @param swPin SW pin (button, active LOW)
     * @param stepsPerDetent Number of encoder steps per physical detent (usually 4 for KY-040)
     */
    RotaryEncoder(uint8_t clkPin, uint8_t dtPin, uint8_t swPin, uint8_t stepsPerDetent = 4);

    /**
     * @brief Initialize encoder hardware
     */
    void begin();

    /**
     * @brief Update encoder state (call in loop)
     * Must be called frequently for accurate reading
     */
    void update();

    /**
     * @brief Get current position
     * @return Current position (in detents)
     */
    int32_t getPosition() const;

    /**
     * @brief Get position change since last check
     * @return Delta position (in detents)
     */
    int32_t getDelta();

    /**
     * @brief Get current direction
     * @return EncoderDirection
     */
    EncoderDirection getDirection() const;

    /**
     * @brief Set current position
     * @param position New position value
     */
    void setPosition(int32_t position);

    /**
     * @brief Reset position to zero
     */
    void reset();

    /**
     * @brief Get button object for direct access
     * @return Pointer to Button object
     */
    Button* getButton();

    /**
     * @brief Check if encoder is rotating
     * @return true if currently rotating
     */
    bool isRotating() const;

    /**
     * @brief Get last event
     * @return Last detected EncoderEvent
     */
    EncoderEvent getEvent();

    /**
     * @brief Set event callback
     * @param callback Function to call on events
     */
    void setCallback(EncoderCallback callback);

    /**
     * @brief Enable/disable acceleration
     * When enabled, faster rotation = larger steps
     * @param enabled true to enable acceleration
     * @param threshold Time threshold in ms for acceleration (default 50ms)
     */
    void setAcceleration(bool enabled, uint16_t threshold = 50);

private:
    // Hardware pins
    uint8_t _clkPin;
    uint8_t _dtPin;
    uint8_t _swPin;

    // Configuration
    uint8_t _stepsPerDetent;

    // State tracking
    volatile int32_t _position;      // Current position in detents
    int32_t _lastReportedPosition;   // Last position reported to getDelta()
    volatile uint8_t _lastEncoded;   // Last encoder state
    EncoderDirection _direction;     // Current direction
    EncoderEvent _lastEvent;         // Last event
    int8_t _stepAccumulator;         // Accumulates steps to form detents

    // Acceleration
    bool _accelerationEnabled;
    uint16_t _accelerationThreshold;
    unsigned long _lastRotationTime;
    uint8_t _accelerationFactor;

    // Button
    Button* _button;

    // Callback
    EncoderCallback _callback;

    // Private methods
    void readEncoder();
    void updateButton();
    void triggerCallback(EncoderEvent event);
    uint8_t getEncodedState();
};

#endif // ROTARY_ENCODER_H
