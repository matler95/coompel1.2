/**
 * @file InputManager.cpp
 * @brief Implementation of InputManager class
 */

#include "InputManager.h"

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

InputManager::InputManager()
    : _encoder(nullptr),
      _encoderMode(false),
      _selectButton(nullptr),
      _backButton(nullptr),
      _selectConfigured(false),
      _backConfigured(false)
{
}

bool InputManager::initWithEncoder(uint8_t clkPin, uint8_t dtPin, uint8_t swPin, uint8_t backPin, uint8_t stepsPerDetent) {
    Serial.println("[INPUT] Initializing input manager with rotary encoder...");

    // Configure rotary encoder (replaces potentiometer + select button)
    // Note: KY-040 can be 1, 2, or 4 steps per detent depending on version
    _encoder = new RotaryEncoder(clkPin, dtPin, swPin, stepsPerDetent);
    _encoder->begin();
    _encoder->setAcceleration(false); // Disable acceleration for more precise control
    _encoderMode = true;

    Serial.printf("[INPUT] Rotary encoder on CLK=%d, DT=%d, SW=%d (steps=%d)\n", clkPin, dtPin, swPin, stepsPerDetent);

    // Configure back button (optional)
    if (backPin > 0) {
        _backButton = new Button(backPin, true, true);
        _backButton->begin();
        _backConfigured = true;
        Serial.printf("[INPUT] Back button on GPIO%d\n", backPin);
    }

    Serial.println("[INPUT] Input manager ready (encoder mode)");
    return true;
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
    // Update encoder if in encoder mode
    if (_encoderMode && _encoder != nullptr) {
        _encoder->update();
    }

    // Update buttons
    if (_selectConfigured && _selectButton != nullptr) {
        _selectButton->update();
    }

    if (_backConfigured && _backButton != nullptr) {
        _backButton->update();
    }
}

// ============================================================================
// BUTTON ACCESS
// ============================================================================

Button* InputManager::getButton(ButtonID id) {
    switch (id) {
        case ButtonID::SELECT:
            // In encoder mode, return encoder's button
            if (_encoderMode && _encoder != nullptr) {
                return _encoder->getButton();
            }
            return _selectConfigured ? _selectButton : nullptr;
        case ButtonID::BACK:
            return _backConfigured ? _backButton : nullptr;
        default:
            return nullptr;
    }
}

RotaryEncoder* InputManager::getEncoder() {
    return _encoderMode ? _encoder : nullptr;
}

bool InputManager::anyButtonPressed() const {
    if (_encoderMode && _encoder != nullptr) {
        if (_encoder->getButton() && _encoder->getButton()->isPressed()) return true;
    }
    if (_selectConfigured && _selectButton != nullptr && _selectButton->isPressed()) return true;
    if (_backConfigured && _backButton != nullptr && _backButton->isPressed()) return true;
    return false;
}

void InputManager::setButtonCallback(ButtonID id, ButtonCallback callback) {
    Button* btn = const_cast<InputManager*>(this)->getButton(id);
    if (btn != nullptr) {
        btn->setCallback(callback);
    }
}

void InputManager::setEncoderCallback(EncoderCallback callback) {
    if (_encoderMode && _encoder != nullptr) {
        _encoder->setCallback(callback);
    }
}

bool InputManager::isEncoderMode() const {
    return _encoderMode;
}