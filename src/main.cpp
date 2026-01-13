/**
 * @file main.cpp
 * @brief ESP32-C3 Interactive Device
 * @version 0.9.0
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
    ANIMATIONS,
    MENU,
    SENSORS,
    MOTION_TEST
};

AppMode currentMode = AppMode::ANIMATIONS;

// ============================================================================
// ANIMATION STATE - Simple & Clean
// ============================================================================
enum class AnimationState {
    IDLE_BASE,      // Showing static base frame
    PLAYING         // Playing an animation
};

AnimationState animState = AnimationState::IDLE_BASE;

// Timing for natural behaviors
unsigned long lastBlinkTime = 0;
unsigned long nextBlinkDelay = 5000;  // Next blink in 5 seconds
unsigned long lastWinkCheck = 0;
unsigned long nextWinkDelay = 20000;  // Check for wink in 20 seconds

// Shake detection
bool isShaking = false;
unsigned long lastShakeTime = 0;
const unsigned long SHAKE_COOLDOWN = 1000;  // 1 second

// Menu timeout
unsigned long lastMenuActivity = 0;
const unsigned long MENU_TIMEOUT_MS = 10000;  // 10 seconds

// ============================================================================
// SETTINGS
// ============================================================================
struct Settings {
    uint8_t brightness;
    bool soundEnabled;
    uint8_t motionSensitivity;
} settings = {
    .brightness = 255,
    .soundEnabled = true,
    .motionSensitivity = 5
};

// ============================================================================
// MENU ITEMS
// ============================================================================
MenuItem mainMenu("Main Menu");
MenuItem animationsMenu("Animations");
MenuItem sensorsMenu("Sensors");
MenuItem motionTestMenu("Motion Test");
MenuItem settingsMenu("Settings");

MenuItem brightnessItem("Brightness", 255, 0, 255);
MenuItem soundItem("Sound", 1, 0, 1);
MenuItem sensitivityItem("Sensitivity", 5, 1, 10);

MenuItem idleAnimItem("Idle Blink");
MenuItem winkAnimItem("Wink");
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

void updateAnimationBehaviors();
void playAnimation(AnimState anim, bool forceLoop = false);
void returnToBaseFrame();
void scheduleNextBlink();
void scheduleNextWinkCheck();

void updateAnimationsMode();
void updateMenuMode();
void updateSensorsMode();
void updateMotionTestMode();

void resetMenuTimeout();
void checkMenuTimeout();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("ESP32-C3 Interactive Device v0.9.0");
    Serial.println("========================================\n");
    
    // Initialize display
    if (!display.init(I2C_SDA_PIN, I2C_SCL_PIN)) {
        Serial.println("[ERROR] Display failed!");
        while (1) delay(1000);
    }
    
    // Initialize animation engine
    animator.init();
    animator.setAutoReturnToIdle(false);  // We'll manage state ourselves
    
    // Start with base frame (idle frame 0, paused)
    animator.play(AnimState::IDLE);
    animator.pauseOnFrame(0);
    animState = AnimationState::IDLE_BASE;
    
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
    menuSystem.setAnalogDeadzone(3);
    display.setBrightness(settings.brightness);
    
    // Schedule first behaviors
    scheduleNextBlink();
    scheduleNextWinkCheck();
    
    Serial.println("\n[INIT] System ready!");
    Serial.println("Natural behaviors:");
    Serial.println("  - Random blinks");
    Serial.println("  - Rare winks (easter egg)");
    Serial.println("  - Shake = dizzy loop");
    Serial.println("  - Menu timeout: 10s");
    Serial.println("========================================\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    unsigned long currentTime = millis();
    
    // Update all systems
    input.update();
    motion.update();
    sensors.update();
    
    #if TOUCH_ENABLED
    touch.update();
    #endif
    
    animator.update();
    
    // Update animation behaviors (only in animation mode)
    if (currentMode == AppMode::ANIMATIONS) {
        updateAnimationBehaviors();
        
        // Check if shaking stopped
        if (isShaking && (currentTime - lastShakeTime > SHAKE_COOLDOWN)) {
            Serial.println("[ANIM] Shake stopped");
            isShaking = false;
            // Don't force stop - let dizzy finish current loop
            // It will return to base automatically
        }
    }
    
    // Update current mode
    switch (currentMode) {
        case AppMode::ANIMATIONS:
            updateAnimationsMode();
            break;
        case AppMode::MENU:
            updateMenuMode();
            checkMenuTimeout();
            break;
        case AppMode::SENSORS:
            updateSensorsMode();
            break;
        case AppMode::MOTION_TEST:
            updateMotionTestMode();
            break;
    }
    
    delay(10);
}

// ============================================================================
// ANIMATION BEHAVIOR LOGIC
// ============================================================================

void updateAnimationBehaviors() {
    unsigned long currentTime = millis();
    
    // If currently playing an animation
    if (animState == AnimationState::PLAYING) {
        // Check if animation finished
        if (!animator.isPlaying()) {
            // If we're NOT shaking anymore, return to base
            if (!isShaking) {
                Serial.println("[ANIM] Animation complete, returning to base");
                returnToBaseFrame();
            } else {
                // Still shaking - restart dizzy animation
                Serial.println("[ANIM] Still shaking, looping dizzy");
                animator.play(AnimState::DIZZY, true);
            }
        }
        return;  // Don't trigger new animations while one is playing
    }
    
    // We're at base frame - check for natural behaviors
    if (animState == AnimationState::IDLE_BASE) {
        // Check for scheduled blink
        if (currentTime - lastBlinkTime >= nextBlinkDelay) {
            Serial.println("[ANIM] Natural blink");
            playAnimation(AnimState::IDLE);
            lastBlinkTime = currentTime;
            scheduleNextBlink();
            return;
        }
        
        // Check for wink (easter egg)
        if (currentTime - lastWinkCheck >= nextWinkDelay) {
            if (random(100) < 30) {  // 30% chance
                Serial.println("[ANIM] Easter egg wink!");
                playAnimation(AnimState::WINK);  // Using HAPPY for wink
            }
            lastWinkCheck = currentTime;
            scheduleNextWinkCheck();
        }
    }
}

void playAnimation(AnimState anim, bool forceLoop) {
    animator.play(anim, true, forceLoop);
    animState = AnimationState::PLAYING;
}

void returnToBaseFrame() {
    animator.play(AnimState::IDLE);
    animator.pauseOnFrame(0);
    animState = AnimationState::IDLE_BASE;
    Serial.println("[ANIM] Back to base frame");
}

void scheduleNextBlink() {
    nextBlinkDelay = random(3000, 8000);  // 3-8 seconds
    Serial.printf("[ANIM] Next blink in %lu ms\n", nextBlinkDelay);
}

void scheduleNextWinkCheck() {
    nextWinkDelay = random(15000, 45000);  // 15-45 seconds
    Serial.printf("[ANIM] Next wink check in %lu ms\n", nextWinkDelay);
}

// ============================================================================
// MENU SETUP
// ============================================================================

void setupMenu() {
    mainMenu.addChild(&animationsMenu);
    mainMenu.addChild(&sensorsMenu);
    mainMenu.addChild(&motionTestMenu);
    mainMenu.addChild(&settingsMenu);
    
    idleAnimItem.setType(MenuItemType::ACTION);
    winkAnimItem.setType(MenuItemType::ACTION);
    dizzyAnimItem.setType(MenuItemType::ACTION);
    
    animationsMenu.addChild(&idleAnimItem);
    animationsMenu.addChild(&winkAnimItem);
    animationsMenu.addChild(&dizzyAnimItem);
    
    tempHumidItem.setType(MenuItemType::ACTION);
    soundLevelItem.setType(MenuItemType::ACTION);
    potValueItem.setType(MenuItemType::ACTION);
    
    sensorsMenu.addChild(&tempHumidItem);
    sensorsMenu.addChild(&soundLevelItem);
    sensorsMenu.addChild(&potValueItem);
    
    motionTestMenu.setType(MenuItemType::ACTION);
    
    brightnessItem.setType(MenuItemType::VALUE);
    brightnessItem.setValue(settings.brightness);
    soundItem.setType(MenuItemType::TOGGLE);
    soundItem.setValue(settings.soundEnabled ? 1 : 0);
    sensitivityItem.setType(MenuItemType::VALUE);
    sensitivityItem.setValue(settings.motionSensitivity);
    
    settingsMenu.addChild(&brightnessItem);
    settingsMenu.addChild(&soundItem);
    settingsMenu.addChild(&sensitivityItem);
    
    menuSystem.init(&mainMenu);
    menuSystem.setStateCallback(onMenuStateChange);
}

// ============================================================================
// INPUT EVENT HANDLERS
// ============================================================================

void onButtonEvent(ButtonEvent event) {
    if (currentMode == AppMode::ANIMATIONS) {
        if (event == ButtonEvent::CLICK || event == ButtonEvent::LONG_PRESS) {
            currentMode = AppMode::MENU;
            resetMenuTimeout();
            menuSystem.draw();
            Serial.println("[NAV] Entered menu");
        }
    } else if (currentMode == AppMode::MENU) {
        resetMenuTimeout();
        
        switch (event) {
            case ButtonEvent::CLICK:
                menuSystem.navigate(MenuNav::SELECT);
                menuSystem.draw();
                break;
                
            case ButtonEvent::LONG_PRESS:
                if (menuSystem.isAtRoot()) {
                    currentMode = AppMode::ANIMATIONS;
                    Serial.println("[NAV] Exited menu");
                } else {
                    menuSystem.navigate(MenuNav::BACK);
                    menuSystem.draw();
                }
                break;
                
            default:
                break;
        }
    } else {
        if (event == ButtonEvent::LONG_PRESS) {
            currentMode = AppMode::MENU;
            resetMenuTimeout();
            menuSystem.draw();
        }
    }
}

void onTouchEvent(TouchEvent event) {
    #if TOUCH_ENABLED
    if (currentMode == AppMode::ANIMATIONS) {
        switch (event) {
            case TouchEvent::TAP:
                if (animState == AnimationState::IDLE_BASE) {
                    playAnimation(AnimState::SURPRISED);
                }
                break;
                
            case TouchEvent::DOUBLE_TAP:
                if (animState == AnimationState::IDLE_BASE) {
                    playAnimation(AnimState::HAPPY);
                }
                break;
                
            case TouchEvent::LONG_TOUCH:
                currentMode = AppMode::MENU;
                resetMenuTimeout();
                menuSystem.draw();
                break;
                
            default:
                break;
        }
    } else if (currentMode == AppMode::MENU) {
        resetMenuTimeout();
        
        switch (event) {
            case TouchEvent::TAP:
                menuSystem.navigate(MenuNav::SELECT);
                menuSystem.draw();
                break;
                
            case TouchEvent::LONG_TOUCH:
                if (menuSystem.isAtRoot()) {
                    currentMode = AppMode::ANIMATIONS;
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
    if (event == MotionEvent::SHAKE) {
        lastShakeTime = millis();
        
        if (currentMode == AppMode::ANIMATIONS) {
            if (!isShaking) {
                Serial.println("[SHAKE] Started - playing dizzy");
                isShaking = true;
                playAnimation(AnimState::DIZZY, false);  // Don't force loop
            } else {
                Serial.println("[SHAKE] Continues");
            }
            
        } else if (currentMode == AppMode::MENU) {
            resetMenuTimeout();
            
            if (menuSystem.isAtRoot()) {
                currentMode = AppMode::ANIMATIONS;
            } else {
                menuSystem.navigate(MenuNav::BACK);
                menuSystem.draw();
            }
        }
    }
}

// ============================================================================
// MENU TIMEOUT
// ============================================================================

void resetMenuTimeout() {
    lastMenuActivity = millis();
}

void checkMenuTimeout() {
    if (millis() - lastMenuActivity > MENU_TIMEOUT_MS) {
        Serial.println("[MENU] Timeout - returning to animations");
        currentMode = AppMode::ANIMATIONS;
    }
}

// ============================================================================
// MENU STATE CALLBACK
// ============================================================================

void onMenuStateChange(MenuItem* item) {
    if (item == nullptr) return;
    
    resetMenuTimeout();
    
    const char* itemText = item->getText();
    
    if (strcmp(itemText, "Idle Blink") == 0) {
        currentMode = AppMode::ANIMATIONS;
        playAnimation(AnimState::IDLE);
    } else if (strcmp(itemText, "Wink") == 0) {
        currentMode = AppMode::ANIMATIONS;
        playAnimation(AnimState::WINK);
    } else if (strcmp(itemText, "Dizzy") == 0) {
        currentMode = AppMode::ANIMATIONS;
        playAnimation(AnimState::DIZZY);
    } else if (strcmp(itemText, "Temp/Humidity") == 0 ||
               strcmp(itemText, "Sound Level") == 0 ||
               strcmp(itemText, "Potentiometer") == 0) {
        currentMode = AppMode::SENSORS;
    } else if (strcmp(itemText, "Motion Test") == 0) {
        currentMode = AppMode::MOTION_TEST;
    }
    
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
    
    if (millis() - lastUpdate >= 50) {
        lastUpdate = millis();
        
        uint16_t potValue = sensors.getPotValue();
        menuSystem.navigateAnalog(potValue, 4095);
        menuSystem.draw();
    }
}

void updateAnimationsMode() {
    display.clear();
    animator.draw();
    display.drawText("Hold=Menu", 64, 56, 1, TextAlign::CENTER);
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
    
    if (sensors.isSoundReady()) {
        uint8_t soundPercent = sensors.getSoundPercent();
        snprintf(buffer, sizeof(buffer), "Sound: %d%%", soundPercent);
        display.drawText(buffer, 0, 32, 1);
        display.drawProgressBar(0, 40, 127, 6, soundPercent / 100.0f);
    }
    
    if (sensors.isPotReady()) {
        snprintf(buffer, sizeof(buffer), "Pot: %d%%", data.potPercent);
        display.drawText(buffer, 0, 50, 1);
    }
    
    display.drawText("Hold=Menu", 64, 58, 1, TextAlign::CENTER);
    display.update();
}

void updateMotionTestMode() {
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate < 100) return;
    lastUpdate = millis();
    
    display.clear();
    display.showTextCentered("MOTION TEST", 0, 1);
    
    if (isShaking) {
        display.showTextCentered("SHAKING!", 24, 2);
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