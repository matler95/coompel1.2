/**
 * @file InputManager.cpp
 * @brief Implementation of InputManager class
 */

#include "InputManager.h"

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

InputManager::InputManager()
    : _selectButton(nullptr),
      _backButton(nullptr),
      _selectConfigured(false),
      _backConfigured(false)
{
}

bool InputManager::init(uint8_t selectPin, uint8_t backPin) {
    Serial.println("[INPUT] Initializing input manager...");
    
    // Configure select button (required)
    if (selectPin > 0) {
        _selectButton = new Button(selectPin, true, true);
        _selectButton->begin();
        _selectConfigured = true;
        Serial.printf("[INPUT] Select button on GPIO%d\n", selectPin);
    } else {
        Serial.println("[INPUT] ERROR: Select button required!");
        return false;
    }
    
    // Configure back button (optional)
    if (backPin > 0) {
        _backButton = new Button(backPin, true, true);
        _backButton->begin();
        _backConfigured = true;
        Serial.printf("[INPUT] Back button on GPIO%d\n", backPin);
    }
    
    Serial.println("[INPUT] Input manager ready");
    return true;
}

// ============================================================================
// UPDATE METHOD
// ============================================================================

void InputManager::update() {
    if (_selectConfigured) {
        _selectButton->update();
    }
    
    if (_backConfigured) {
        _backButton->update();
    }
}

// ============================================================================
// BUTTON ACCESS
// ============================================================================

Button* InputManager::getButton(ButtonID id) {
    switch (id) {
        case ButtonID::SELECT:
            return _selectConfigured ? _selectButton : nullptr;
        case ButtonID::BACK:
            return _backConfigured ? _backButton : nullptr;
        default:
            return nullptr;
    }
}

bool InputManager::anyButtonPressed() const {
    if (_selectConfigured && _selectButton->isPressed()) return true;
    if (_backConfigured && _backButton->isPressed()) return true;
    return false;
}

void InputManager::setButtonCallback(ButtonID id, ButtonCallback callback) {
    Button* btn = const_cast<InputManager*>(this)->getButton(id);
    if (btn != nullptr) {
        btn->setCallback(callback);
    }
}