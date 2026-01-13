/**
 * @file main.cpp
 * @brief ESP32-C3 Interactive Device - With Animation Engine
 * @version 0.6.0
 * 
 * Phase 6: Animation engine integrated
 */

#include <Arduino.h>
#include "config.h"
#include "DisplayManager.h"
#include "InputManager.h"
#include "MotionSensor.h"
#include "TouchSensor.h"
#include "MenuSystem.h"
#include "AnimationEngine.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
DisplayManager display;
InputManager input;
MotionSensor motion;
TouchSensor touch(TOUCH_SENSOR_PIN);
MenuSystem menuSystem(&display);
AnimationEngine animator(&display);

// ============================================================================
// APPLICATION STATE
// ============================================================================
enum class AppMode {
    MENU,
    ANIMATIONS,
    SENSORS,
    MOTION_TEST,
    GAME
};

AppMode currentMode = AppMode::ANIMATIONS;  // Start with animations!
AppMode lastMode = AppMode::MENU;

// ============================================================================
// SETTINGS
// ============================================================================
struct Settings {
    uint8_t brightness;
    bool soundEnabled;
    uint8_t motionSensitivity;
    bool autoSleep;
} settings = {
    .brightness = 255,
    .soundEnabled = true,
    .motionSensitivity = 5,
    .autoSleep = true
};

// ============================================================================
// MENU ITEMS
// ============================================================================
MenuItem mainMenu("Main Menu");
MenuItem animationsMenu("Animations");
MenuItem sensorsMenu("Sensors");
MenuItem motionTestMenu("Motion Test");
MenuItem gamesMenu("Games");
MenuItem settingsMenu("Settings");

MenuItem brightnessItem("Brightness", 255, 0, 255);
MenuItem soundItem("Sound", 1, 0, 1);
MenuItem sensitivityItem("Sensitivity", 5, 1, 10);
MenuItem aboutItem("About");

MenuItem idleAnimItem("Idle Face");
MenuItem happyAnimItem("Happy");
MenuItem surprisedAnimItem("Surprised");
MenuItem dizzyAnimItem("Dizzy");

MenuItem tempHumidItem("Temp/Humidity");
MenuItem soundLevelItem("Sound Level");
MenuItem accelDataItem("Accelerometer");

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void setupMenu();
void onMenuStateChange(MenuItem* item);
void onButtonEvent(ButtonEvent event);
void onTouchEvent(TouchEvent event);
void onMotionEvent(MotionEvent event);

void updateMenuMode();
void updateAnimationsMode();
void updateSensorsMode();
void updateMotionTestMode();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("ESP32-C3 Interactive Device v0.6.0");
    Serial.println("Phase 6: Animation Engine");
    Serial.println("========================================\n");
    
    // Initialize display
    if (!display.init(I2C_SDA_PIN, I2C_SCL_PIN)) {
        Serial.println("[ERROR] Display failed!");
        while (1) delay(1000);
    }
    
    // Initialize animation engine
    animator.init();
    
    // Initialize input
    if (!input.init(BTN_SELECT_PIN, BTN_BACK_PIN)) {
        Serial.println("[ERROR] Input failed!");
        while (1) delay(1000);
    }
    input.setButtonCallback(ButtonID::SELECT, onButtonEvent);
    if (BTN_BACK_PIN > 0) {
        input.setButtonCallback(ButtonID::BACK, onButtonEvent);
    }
    
    // Initialize motion sensor
    if (!motion.init(&Wire)) {
        Serial.println("[WARN] Motion sensor not found");
    } else {
        motion.setCallback(onMotionEvent);
        motion.setShakeThreshold(20.0f);
    }
    
    // Initialize touch sensor
    #if TOUCH_ENABLED
    touch.begin();
    touch.setCallback(onTouchEvent);
    #else
    touch.setEnabled(false);
    #endif
    
    // Setup menu
    setupMenu();
    
    display.setBrightness(settings.brightness);
    
    Serial.println("[INIT] System ready!");
    Serial.println("Touch/Click to trigger animations!");
    Serial.println("Shake for dizzy animation!");
    Serial.println("Long press for menu");
    Serial.println("========================================\n");
    
    // Start with idle animation
    animator.play(AnimState::IDLE);
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Update all input systems
    input.update();
    motion.update();
    
    #if TOUCH_ENABLED
    touch.update();
    #endif
    
    // Update animator
    animator.update();
    
    // Update current mode
    switch (currentMode) {
        case AppMode::MENU:
            updateMenuMode();
            break;
        case AppMode::ANIMATIONS:
            updateAnimationsMode();
            break;
        case AppMode::SENSORS:
            updateSensorsMode();
            break;
        case AppMode::MOTION_TEST:
            updateMotionTestMode();
            break;
        case AppMode::GAME:
            break;
    }
    
    delay(10);
}

// ============================================================================
// MENU SETUP
// ============================================================================

void setupMenu() {
    mainMenu.addChild(&animationsMenu);
    mainMenu.addChild(&sensorsMenu);
    mainMenu.addChild(&motionTestMenu);
    mainMenu.addChild(&gamesMenu);
    mainMenu.addChild(&settingsMenu);
    
    animationsMenu.addChild(&idleAnimItem);
    animationsMenu.addChild(&happyAnimItem);
    animationsMenu.addChild(&surprisedAnimItem);
    animationsMenu.addChild(&dizzyAnimItem);
    
    sensorsMenu.addChild(&tempHumidItem);
    sensorsMenu.addChild(&soundLevelItem);
    sensorsMenu.addChild(&accelDataItem);
    
    motionTestMenu.setType(MenuItemType::ACTION);
    gamesMenu.setType(MenuItemType::INFO);
    
    brightnessItem.setType(MenuItemType::VALUE);
    brightnessItem.setValue(settings.brightness);
    soundItem.setType(MenuItemType::TOGGLE);
    soundItem.setValue(settings.soundEnabled ? 1 : 0);
    sensitivityItem.setType(MenuItemType::VALUE);
    sensitivityItem.setValue(settings.motionSensitivity);
    aboutItem.setType(MenuItemType::ACTION);
    
    settingsMenu.addChild(&brightnessItem);
    settingsMenu.addChild(&soundItem);
    settingsMenu.addChild(&sensitivityItem);
    settingsMenu.addChild(&aboutItem);
    
    menuSystem.init(&mainMenu);
    menuSystem.setStateCallback(onMenuStateChange);
}

// ============================================================================
// INPUT EVENT HANDLERS
// ============================================================================

void onButtonEvent(ButtonEvent event) {
    if (currentMode == AppMode::ANIMATIONS) {
        // Animation mode - trigger animations!
        switch (event) {
            case ButtonEvent::CLICK:
                #if !TOUCH_ENABLED
                animator.play(AnimState::SURPRISED, true);
                #endif
                break;
                
            case ButtonEvent::DOUBLE_CLICK:
                #if !TOUCH_ENABLED
                animator.play(AnimState::HAPPY, true);
                #endif
                break;
                
            case ButtonEvent::LONG_PRESS:
                // Enter menu
                currentMode = AppMode::MENU;
                menuSystem.draw();
                break;
                
            default:
                break;
        }
    } else if (currentMode == AppMode::MENU) {
        switch (event) {
            case ButtonEvent::CLICK:
                #if !TOUCH_ENABLED
                menuSystem.navigate(MenuNav::DOWN);
                menuSystem.draw();
                #endif
                break;
                
            case ButtonEvent::DOUBLE_CLICK:
                #if !TOUCH_ENABLED
                menuSystem.navigate(MenuNav::SELECT);
                menuSystem.draw();
                #endif
                break;
                
            case ButtonEvent::LONG_PRESS:
                menuSystem.navigate(MenuNav::BACK);
                menuSystem.draw();
                break;
                
            default:
                break;
        }
    } else {
        // Other modes - long press to menu
        if (event == ButtonEvent::LONG_PRESS) {
            currentMode = AppMode::MENU;
            menuSystem.draw();
        }
    }
}

void onTouchEvent(TouchEvent event) {
    #if TOUCH_ENABLED
    if (currentMode == AppMode::ANIMATIONS) {
        switch (event) {
            case TouchEvent::TAP:
                animator.play(AnimState::SURPRISED, true);
                break;
                
            case TouchEvent::DOUBLE_TAP:
                animator.play(AnimState::HAPPY, true);
                break;
                
            case TouchEvent::LONG_TOUCH:
                currentMode = AppMode::MENU;
                menuSystem.draw();
                break;
                
            default:
                break;
        }
    } else if (currentMode == AppMode::MENU) {
        switch (event) {
            case TouchEvent::TAP:
                menuSystem.navigate(MenuNav::DOWN);
                menuSystem.draw();
                break;
                
            case TouchEvent::DOUBLE_TAP:
                menuSystem.navigate(MenuNav::SELECT);
                menuSystem.draw();
                break;
                
            case TouchEvent::LONG_TOUCH:
                menuSystem.navigate(MenuNav::BACK);
                menuSystem.draw();
                break;
                
            default:
                break;
        }
    }
    #endif
}

void onMotionEvent(MotionEvent event) {
    if (currentMode == AppMode::ANIMATIONS) {
        if (event == MotionEvent::SHAKE) {
            animator.play(AnimState::DIZZY, true);
        }
    } else if (currentMode == AppMode::MENU) {
        if (event == MotionEvent::SHAKE) {
            menuSystem.navigate(MenuNav::BACK);
            menuSystem.draw();
        }
    }
}

// ============================================================================
// MENU STATE CALLBACK
// ============================================================================

void onMenuStateChange(MenuItem* item) {
    if (item == nullptr) return;
    
    const char* itemText = item->getText();
    
    // Handle menu selections
    if (strcmp(itemText, "Idle Face") == 0) {
        currentMode = AppMode::ANIMATIONS;
        animator.play(AnimState::IDLE);
    } else if (strcmp(itemText, "Happy") == 0) {
        currentMode = AppMode::ANIMATIONS;
        animator.play(AnimState::HAPPY);
    } else if (strcmp(itemText, "Surprised") == 0) {
        currentMode = AppMode::ANIMATIONS;
        animator.play(AnimState::SURPRISED);
    } else if (strcmp(itemText, "Dizzy") == 0) {
        currentMode = AppMode::ANIMATIONS;
        animator.play(AnimState::DIZZY);
    } else if (strcmp(itemText, "Accelerometer") == 0) {
        currentMode = AppMode::SENSORS;
    } else if (strcmp(itemText, "Motion Test") == 0) {
        currentMode = AppMode::MOTION_TEST;
    }
    
    // Handle settings
    if (strcmp(itemText, "Brightness") == 0) {
        settings.brightness = item->getValue();
        display.setBrightness(settings.brightness);
    } else if (strcmp(itemText, "Sound") == 0) {
        settings.soundEnabled = item->getValue() == 1;
    } else if (strcmp(itemText, "Sensitivity") == 0) {
        settings.motionSensitivity = item->getValue();
        float threshold = 30.0f - (settings.motionSensitivity * 2.0f);
        motion.setShakeThreshold(threshold);
    }
}

// ============================================================================
// MODE UPDATE FUNCTIONS
// ============================================================================

void updateMenuMode() {
    // Menu updates on events only
}

void updateAnimationsMode() {
    // Draw current animation frame
    display.clear();
    animator.draw();
    
    // Show hint text at bottom
    display.drawText("Tap/Shake me!", 64, 56, 1, TextAlign::CENTER);
    
    display.update();
}

void updateSensorsMode() {
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate < 500) return;
    lastUpdate = millis();
    
    display.clear();
    display.showTextCentered("SENSORS", 0, 1);
    
    float x, y, z;
    motion.getAcceleration(x, y, z);
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "X: %.1f", x);
    display.drawText(buffer, 0, 16, 1);
    
    snprintf(buffer, sizeof(buffer), "Y: %.1f", y);
    display.drawText(buffer, 0, 26, 1);
    
    snprintf(buffer, sizeof(buffer), "Z: %.1f", z);
    display.drawText(buffer, 0, 36, 1);
    
    Orientation orient = motion.getOrientation();
    const char* orientText = "?";
    switch (orient) {
        case Orientation::FLAT: orientText = "FLAT"; break;
        case Orientation::UPSIDE_DOWN: orientText = "UPSIDE"; break;
        case Orientation::PORTRAIT: orientText = "PORTRAIT"; break;
        case Orientation::LANDSCAPE_LEFT: orientText = "LEFT"; break;
        case Orientation::LANDSCAPE_RIGHT: orientText = "RIGHT"; break;
        default: orientText = "UNKNOWN"; break;
    }
    
    snprintf(buffer, sizeof(buffer), "Orient: %s", orientText);
    display.drawText(buffer, 0, 46, 1);
    
    display.drawText("Hold=Menu", 64, 56, 1, TextAlign::CENTER);
    display.update();
}

void updateMotionTestMode() {
    static unsigned long lastUpdate = 0;
    static String lastGesture = "None";
    static unsigned long gestureTime = 0;
    
    MotionEvent event = motion.getEvent();
    if (event == MotionEvent::SHAKE) {
        lastGesture = "SHAKE!";
        gestureTime = millis();
    }
    
    if (millis() - lastUpdate < 100) return;
    lastUpdate = millis();
    
    display.clear();
    display.showTextCentered("MOTION TEST", 0, 1);
    
    if (millis() - gestureTime < 2000) {
        display.showTextCentered(lastGesture.c_str(), 24, 2);
    } else {
        display.showTextCentered("Shake me!", 24, 1);
    }
    
    float magnitude = motion.getAccelMagnitude();
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Accel: %.1f", magnitude);
    display.drawText(buffer, 0, 40, 1);
    
    float progress = min(magnitude / 30.0f, 1.0f);
    display.drawProgressBar(0, 50, 127, 6, progress);
    
    display.drawText("Hold=Menu", 64, 58, 1, TextAlign::CENTER);
    display.update();
}