/**
 * @file main.cpp
 * @brief ESP32-C3 Interactive Device - Analog Menu Navigation
 * @version 0.7.0
 * 
 * Phase 7: Smooth potentiometer-based menu navigation
 */

#include <Arduino.h>
#include "config.h"
#include "DisplayManager.h"
#include "InputManager.h"
#include "MotionSensor.h"
#include "TouchSensor.h"
#include "MenuSystem.h"
#include "AnimationEngine.h"
#include "SensorHub.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
DisplayManager display;
InputManager input;
MotionSensor motion;
TouchSensor touch(TOUCH_SENSOR_PIN);
MenuSystem menuSystem(&display);
AnimationEngine animator(&display);
SensorHub sensors;

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

AppMode currentMode = AppMode::ANIMATIONS;

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

MenuItem idleAnimItem("idle");
MenuItem winkAnimItem("wink");
MenuItem surprisedAnimItem("Surprised");
MenuItem dizzyAnimItem("Dizzy");

MenuItem tempHumidItem("Temp/Humidity");
MenuItem soundLevelItem("Sound Level");
MenuItem potValueItem("Potentiometer");

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
    Serial.println("ESP32-C3 Interactive Device v0.7.0");
    Serial.println("Phase 7: Analog Menu Navigation");
    Serial.println("========================================\n");
    
    // Initialize display
    if (!display.init(I2C_SDA_PIN, I2C_SCL_PIN)) {
        Serial.println("[ERROR] Display failed!");
        while (1) delay(1000);
    }
    
    // Initialize animation engine
    animator.init();
    
    // Initialize sensors
    sensors.init(DHT11_PIN, SOUND_SENSOR_PIN, POTENTIOMETER_PIN);
    
    // Initialize input
    if (!input.init(BTN_SELECT_PIN, BTN_BACK_PIN)) {
        Serial.println("[ERROR] Input failed!");
        while (1) delay(1000);
    }
    input.setButtonCallback(ButtonID::SELECT, onButtonEvent);
    
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
    menuSystem.setAnalogDeadzone(3);  // 3% deadzone for smooth scrolling
    
    display.setBrightness(settings.brightness);
    
    Serial.println("\n[INIT] System ready!");
    Serial.println("===========================================");
    Serial.println("NAVIGATION:");
    Serial.println("  Potentiometer → Scroll menu");
    Serial.println("  Click → Select/Confirm");
    Serial.println("  Long Press → Go Back");
    Serial.println("  Shake → Go Back (in menu)");
    Serial.println("===========================================\n");
    
    // Start with idle animation
    animator.play(AnimState::IDLE);
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Update all systems
    input.update();
    motion.update();
    sensors.update();
    
    #if TOUCH_ENABLED
    touch.update();
    #endif
    
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
    
    idleAnimItem.setType(MenuItemType::ACTION);
    winkAnimItem.setType(MenuItemType::ACTION);
    surprisedAnimItem.setType(MenuItemType::ACTION);
    dizzyAnimItem.setType(MenuItemType::ACTION);
    
    animationsMenu.addChild(&idleAnimItem);
    animationsMenu.addChild(&winkAnimItem);
    animationsMenu.addChild(&surprisedAnimItem);
    animationsMenu.addChild(&dizzyAnimItem);
    
    tempHumidItem.setType(MenuItemType::ACTION);
    soundLevelItem.setType(MenuItemType::ACTION);
    potValueItem.setType(MenuItemType::ACTION);
    
    sensorsMenu.addChild(&tempHumidItem);
    sensorsMenu.addChild(&soundLevelItem);
    sensorsMenu.addChild(&potValueItem);
    
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
        // Animation mode
        switch (event) {
            case ButtonEvent::CLICK:
                #if !TOUCH_ENABLED
                // Enter menu
                currentMode = AppMode::MENU;
                menuSystem.draw();
                Serial.println("[NAV] Entered menu");
                #endif
                break;
                
            case ButtonEvent::LONG_PRESS:
                // Enter menu
                currentMode = AppMode::MENU;
                menuSystem.draw();
                Serial.println("[NAV] Entered menu");
                break;
                
            default:
                break;
        }
    } else if (currentMode == AppMode::MENU) {
        // Menu mode - potentiometer scrolls, button confirms/backs
        switch (event) {
            case ButtonEvent::CLICK:
                // Confirm/Select current item
                menuSystem.navigate(MenuNav::SELECT);
                menuSystem.draw();
                Serial.println("[NAV] Item selected");
                break;
                
            case ButtonEvent::LONG_PRESS:
                // Go back
                if (menuSystem.isAtRoot()) {
                    // Exit menu to animations
                    currentMode = AppMode::ANIMATIONS;
                    animator.play(AnimState::IDLE);
                    Serial.println("[NAV] Exited menu");
                } else {
                    menuSystem.navigate(MenuNav::BACK);
                    menuSystem.draw();
                    Serial.println("[NAV] Back");
                }
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
                animator.play(AnimState::WINK, true);
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
                menuSystem.navigate(MenuNav::SELECT);
                menuSystem.draw();
                break;
                
            case TouchEvent::LONG_TOUCH:
                if (menuSystem.isAtRoot()) {
                    currentMode = AppMode::ANIMATIONS;
                    animator.play(AnimState::IDLE);
                } else {
                    menuSystem.navigate(MenuNav::BACK);
                    menuSystem.draw();
                }
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
            // Shake = go back in menu
            if (menuSystem.isAtRoot()) {
                currentMode = AppMode::ANIMATIONS;
                animator.play(AnimState::IDLE);
                Serial.println("[NAV] Shake exit menu");
            } else {
                menuSystem.navigate(MenuNav::BACK);
                menuSystem.draw();
                Serial.println("[NAV] Shake back");
            }
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
    if (strcmp(itemText, "idle") == 0) {
        currentMode = AppMode::ANIMATIONS;
        animator.play(AnimState::IDLE);
    } else if (strcmp(itemText, "wink") == 0) {
        currentMode = AppMode::ANIMATIONS;
        animator.play(AnimState::WINK);
    } else if (strcmp(itemText, "Surprised") == 0) {
        currentMode = AppMode::ANIMATIONS;
        animator.play(AnimState::SURPRISED);
    } else if (strcmp(itemText, "Dizzy") == 0) {
        currentMode = AppMode::ANIMATIONS;
        animator.play(AnimState::DIZZY);
    } else if (strcmp(itemText, "Temp/Humidity") == 0 ||
               strcmp(itemText, "Sound Level") == 0 ||
               strcmp(itemText, "Potentiometer") == 0) {
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
    static unsigned long lastUpdate = 0;
    
    // Update menu navigation with potentiometer (smooth analog scrolling!)
    if (millis() - lastUpdate >= 50) {  // 20 Hz update rate
        lastUpdate = millis();
        
        uint16_t potValue = sensors.getPotValue();
        menuSystem.navigateAnalog(potValue, 4095);
        menuSystem.draw();
    }
}

void updateAnimationsMode() {
    display.clear();
    animator.draw();
    
    // Only show hint for small animations
    if (animator.getCurrentAnimation()->width < 100) {
        display.drawText("Tap/Shake me!", 64, 56, 1, TextAlign::CENTER);
    }
    
    display.update();
}

void updateSensorsMode() {
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate < 500) return;
    lastUpdate = millis();
    
    display.clear();
    display.showTextCentered("SENSORS", 0, 1);
    
    const SensorData& data = sensors.getData();
    
    char buffer[32];
    
    // Temperature & Humidity
    if (sensors.isDHTReady()) {
        if (data.dhtValid) {
            snprintf(buffer, sizeof(buffer), "Temp: %.1fC", data.temperature);
            display.drawText(buffer, 0, 12, 1);
            snprintf(buffer, sizeof(buffer), "Hum: %.1f%%", data.humidity);
            display.drawText(buffer, 0, 22, 1);
        } else {
            display.drawText("DHT: Reading...", 0, 12, 1);
        }
    }
    
    // Sound level
    if (sensors.isSoundReady()) {
        uint8_t soundPercent = sensors.getSoundPercent();
        snprintf(buffer, sizeof(buffer), "Sound: %d%%", soundPercent);
        display.drawText(buffer, 0, 32, 1);
        display.drawProgressBar(0, 40, 127, 6, soundPercent / 100.0f);
    }
    
    // Potentiometer
    if (sensors.isPotReady()) {
        snprintf(buffer, sizeof(buffer), "Pot: %d%%", data.potPercent);
        display.drawText(buffer, 0, 50, 1);
    }
    
    display.drawText("Hold=Menu", 64, 58, 1, TextAlign::CENTER);
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