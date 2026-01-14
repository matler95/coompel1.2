/**
 * @file InputManager.h
 * @brief High-level input management for rotary encoder and buttons
 * @version 2.0.0
 *
 * Manages all input devices (rotary encoder, buttons, sensors)
 * Provides centralized input polling and event distribution
 */

#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "Button.h"
#include "RotaryEncoder.h"

// ============================================================================
// BUTTON IDENTIFIERS
// ============================================================================
enum class ButtonID {
    SELECT,    // Main select/enter button (or encoder button)
    BACK,      // Back/cancel button
    UP,        // Optional up button
    DOWN       // Optional down button
};

// ============================================================================
// INPUT MANAGER CLASS
// ============================================================================
class InputManager {
public:
    /**
     * @brief Constructor
     */
    InputManager();

    /**
     * @brief Initialize input hardware with rotary encoder
     * @param clkPin Encoder CLK pin
     * @param dtPin Encoder DT pin
     * @param swPin Encoder SW (button) pin
     * @param backPin Optional back button pin (0 = not used)
     * @param stepsPerDetent Steps per detent for encoder (1, 2, or 4)
     */
    bool initWithEncoder(uint8_t clkPin, uint8_t dtPin, uint8_t swPin, uint8_t backPin = 0, uint8_t stepsPerDetent = 2);

    /**
     * @brief Initialize input hardware with buttons only (legacy)
     * @param selectPin GPIO pin for select button
     * @param backPin GPIO pin for back button (0 = not used)
     */
    bool init(uint8_t selectPin, uint8_t backPin = 0);

    /**
     * @brief Update all inputs (call in loop)
     */
    void update();

    /**
     * @brief Get button object by ID
     * @param id Button identifier
     * @return Pointer to Button object, or nullptr if not configured
     */
    Button* getButton(ButtonID id);

    /**
     * @brief Get rotary encoder object
     * @return Pointer to RotaryEncoder, or nullptr if not configured
     */
    RotaryEncoder* getEncoder();

    /**
     * @brief Check if any button is pressed
     * @return true if any button is active
     */
    bool anyButtonPressed() const;

    /**
     * @brief Set callback for specific button
     * @param id Button identifier
     * @param callback Function to call on events
     */
    void setButtonCallback(ButtonID id, ButtonCallback callback);

    /**
     * @brief Set callback for encoder events
     * @param callback Function to call on encoder events
     */
    void setEncoderCallback(EncoderCallback callback);

    /**
     * @brief Check if using encoder mode
     * @return true if encoder is configured
     */
    bool isEncoderMode() const;

private:
    // Rotary encoder
    RotaryEncoder* _encoder;
    bool _encoderMode;

    // Buttons
    Button* _selectButton;
    Button* _backButton;

    bool _selectConfigured;
    bool _backConfigured;
};

#endif // INPUT_MANAGER_H