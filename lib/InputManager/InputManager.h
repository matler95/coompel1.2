/**
 * @file InputManager.h
 * @brief High-level input management for multiple buttons
 * @version 1.0.0
 * 
 * Manages all input devices (buttons, eventually sensors)
 * Provides centralized input polling and event distribution
 */

#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "Button.h"

// ============================================================================
// BUTTON IDENTIFIERS
// ============================================================================
enum class ButtonID {
    SELECT,    // Main select/enter button
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
     * @brief Initialize input hardware
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

private:
    Button* _selectButton;
    Button* _backButton;
    
    bool _selectConfigured;
    bool _backConfigured;
};

#endif // INPUT_MANAGER_H