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
#include "WiFiManager.h"
#include "GeoLocationClient.h"
#include "WeatherClient.h"

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
WiFiManager wifi;

// ============================================================================
// APPLICATION STATE
// ============================================================================
enum class AppMode {
    ANIMATIONS,
    MENU,
    SENSORS,
    WIFI_SETUP,    // Show captive portal connection info
    WIFI_INFO      // Show connection status details
};

AppMode currentMode = AppMode::ANIMATIONS;
// Encoder edit mode: when true, rotating adjusts the current VALUE/TOGGLE item;
// when false, rotating moves selection up/down.
bool encoderEditMode = false;

// ============================================================================
// TIMING FOR NATURAL BEHAVIORS
// ============================================================================
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
    // Brightness stored as percent (10-100), mapped to 0-255 for the display
    uint8_t brightness;
    bool soundEnabled;
    uint8_t motionSensitivity;
} settings = {
    .brightness = 100,   // 100% user brightness
    .soundEnabled = true,
    .motionSensitivity = 5
};

// ============================================================================
// MENU ITEMS
// ============================================================================
MenuItem mainMenu("Main Menu");
MenuItem animationsMenu("Animations");
MenuItem sensorsMenu("Sensors");
MenuItem settingsMenu("Settings");
MenuItem test1Menu("Test 1");
MenuItem test2Menu("Test 2");
MenuItem test3Menu("Test 3");
MenuItem test4Menu("Test 4");

// Brightness as user-facing percent (10-100, in steps of 10)
MenuItem brightnessItem("Brightness", 100, 10, 100);
MenuItem soundItem("Sound", 1, 0, 1);
MenuItem sensitivityItem("Sensitivity", 5, 1, 10);

MenuItem idleAnimItem("Idle Blink");
MenuItem winkAnimItem("Wink");
MenuItem dizzyAnimItem("Dizzy");

MenuItem tempHumidItem("Temp/Humidity");
MenuItem soundLevelItem("Sound Level");

// WiFi menu items
MenuItem wifiMenu("WiFi");
MenuItem wifiConfigureItem("Configure");
MenuItem wifiStatusItem("Status");
MenuItem wifiForgetItem("Forget Network");

// Weather menu items
MenuItem weatherMenu("Weather");
MenuItem weatherTestGeoItem("Test Location");
MenuItem weatherTestForecastItem("Test Forecast");
MenuItem weatherEnableItem("Enable Weather");
MenuItem weatherViewItem("View Forecast");
MenuItem weatherPrivacyItem("Privacy Info");

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void setupMenu();
void onMenuStateChange(MenuItem* item);
void onEncoderEvent(EncoderEvent event, int32_t position);
void onButtonEvent(ButtonEvent event);
void onTouchEvent(TouchEvent event);
void onMotionEvent(MotionEvent event);

void checkRandomAnimations();
void scheduleNextBlink();
void scheduleNextWinkCheck();

void updateAnimationsMode();
void updateMenuMode();
void updateSensorsMode();
void updateWiFiSetupMode();
void updateWiFiInfoMode();

void resetMenuTimeout();
void checkMenuTimeout();
void applyBrightnessFromSettings();
void onWiFiEvent(WiFiEvent event);
void drawWiFiStatusIcon();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
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

    // Start with static base frame (idle frame 0)
    animator.showStaticFrame(AnimState::IDLE, 0);
    
    // Initialize sensors
    sensors.init(DHT11_PIN, SOUND_SENSOR_PIN);

    // Initialize input
    // NOTE: Pass 0 for backPin unless you have a *separate* back button on a dedicated GPIO.
    // Using the same GPIO as encoder CLK/DT will cause undefined behavior.
    if (!input.initWithEncoder(ENCODER_CLK_PIN, ENCODER_DT_PIN, ENCODER_SW_PIN, 0, ENCODER_STEPS_PER_DETENT)) {
        Serial.println("[ERROR] Input failed!");
        while (1) delay(1000);
    }
    // Use encoder button for select, and encoder rotation for navigation/value changes
    input.setButtonCallback(ButtonID::SELECT, onButtonEvent);
    input.setEncoderCallback(onEncoderEvent);
    Serial.println("[INIT] Using rotary encoder for input");
    
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
    applyBrightnessFromSettings();

    // Initialize WiFi
    wifi.init();
    wifi.setEventCallback(onWiFiEvent);

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
    wifi.update();  // Handle WiFi state machine

    #if TOUCH_ENABLED
    touch.update();
    #endif

    animator.update();

    // Check if shaking stopped
    if (isShaking && (currentTime - lastShakeTime > SHAKE_COOLDOWN)) {
        Serial.println("[SHAKE] Stopped - finishing current cycle");
        isShaking = false;
        animator.stopLoopingGracefully();  // Will finish current cycle then stop
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
        case AppMode::WIFI_SETUP:
            updateWiFiSetupMode();
            break;
        case AppMode::WIFI_INFO:
            updateWiFiInfoMode();
            break;
    }
    
    delay(10);
}

// ============================================================================
// RANDOM ANIMATION LOGIC
// ============================================================================

void checkRandomAnimations() {
    unsigned long currentTime = millis();

    // Don't interrupt running animations
    if (animator.isPlaying()) {
        return;
    }

    // Don't run animations while shaking (dizzy handles it)
    if (isShaking) {
        return;
    }

    // Check for scheduled blink
    if (currentTime - lastBlinkTime >= nextBlinkDelay) {
        // Serial.println("[ANIM] Natural blink");
        animator.play(AnimState::IDLE, true);  // Priority play
        lastBlinkTime = currentTime;
        scheduleNextBlink();
        return;
    }

    // Check for wink (easter egg)
    if (currentTime - lastWinkCheck >= nextWinkDelay) {
        if (random(100) < 30) {  // 30% chance
            // Serial.println("[ANIM] Easter egg wink!");
            animator.play(AnimState::WINK, true);
        }
        lastWinkCheck = currentTime;
        scheduleNextWinkCheck();
    }
}

void scheduleNextBlink() {
    nextBlinkDelay = random(3000, 8000);  // 3-8 seconds
    // Serial.printf("[ANIM] Next blink in %lu ms\n", nextBlinkDelay);
}

void scheduleNextWinkCheck() {
    nextWinkDelay = random(15000, 45000);  // 15-45 seconds
    // Serial.printf("[ANIM] Next wink check in %lu ms\n", nextWinkDelay);
}

// ============================================================================
// MENU SETUP
// ============================================================================

void setupMenu() {
    mainMenu.addChild(&animationsMenu);
    mainMenu.addChild(&sensorsMenu);
    mainMenu.addChild(&settingsMenu);
    mainMenu.addChild(&test1Menu);
    mainMenu.addChild(&test2Menu);
    mainMenu.addChild(&test3Menu);
    mainMenu.addChild(&test4Menu);
    
    idleAnimItem.setType(MenuItemType::ACTION);
    winkAnimItem.setType(MenuItemType::ACTION);
    dizzyAnimItem.setType(MenuItemType::ACTION);
    
    animationsMenu.addChild(&idleAnimItem);
    animationsMenu.addChild(&winkAnimItem);
    animationsMenu.addChild(&dizzyAnimItem);
    
    tempHumidItem.setType(MenuItemType::ACTION);
    soundLevelItem.setType(MenuItemType::ACTION);

    sensorsMenu.addChild(&tempHumidItem);
    sensorsMenu.addChild(&soundLevelItem);
    
    
    brightnessItem.setType(MenuItemType::VALUE);
    // Ensure brightness starts on a valid 10% step between 10 and 100
    settings.brightness = constrain(settings.brightness, 10, 100);
    settings.brightness = (settings.brightness / 10) * 10;
    brightnessItem.setValue(settings.brightness);
    soundItem.setType(MenuItemType::TOGGLE);
    soundItem.setValue(settings.soundEnabled ? 1 : 0);
    sensitivityItem.setType(MenuItemType::VALUE);
    sensitivityItem.setValue(settings.motionSensitivity);
    
    settingsMenu.addChild(&brightnessItem);
    settingsMenu.addChild(&soundItem);
    settingsMenu.addChild(&sensitivityItem);
    settingsMenu.addChild(&wifiMenu);
    settingsMenu.addChild(&weatherMenu);

    // WiFi submenu
    wifiMenu.setType(MenuItemType::SUBMENU);
    wifiConfigureItem.setType(MenuItemType::ACTION);
    wifiStatusItem.setType(MenuItemType::INFO);
    wifiForgetItem.setType(MenuItemType::ACTION);
    

    wifiMenu.addChild(&wifiConfigureItem);
    wifiMenu.addChild(&wifiStatusItem);
    wifiMenu.addChild(&wifiForgetItem);

    // Setup weather menu
    weatherMenu.setType(MenuItemType::SUBMENU);
    weatherTestGeoItem.setType(MenuItemType::ACTION);
    weatherTestForecastItem.setType(MenuItemType::ACTION);
    weatherEnableItem.setType(MenuItemType::ACTION);
    weatherViewItem.setType(MenuItemType::ACTION);
    weatherPrivacyItem.setType(MenuItemType::ACTION);

    weatherMenu.addChild(&weatherTestGeoItem);
    weatherMenu.addChild(&weatherTestForecastItem);
    weatherMenu.addChild(&weatherEnableItem);
    weatherMenu.addChild(&weatherViewItem);
    weatherMenu.addChild(&weatherPrivacyItem);

    // Assign menu item IDs for fast lookup
    mainMenu.setID(MenuItemID::MAIN_MENU);
    animationsMenu.setID(MenuItemID::ANIMATIONS_MENU);
    sensorsMenu.setID(MenuItemID::SENSORS_MENU);
    settingsMenu.setID(MenuItemID::SETTINGS_MENU);
    test1Menu.setID(MenuItemID::TEST1);
    test2Menu.setID(MenuItemID::TEST2);
    test3Menu.setID(MenuItemID::TEST3);
    test4Menu.setID(MenuItemID::TEST4);

    idleAnimItem.setID(MenuItemID::ANIM_IDLE);
    winkAnimItem.setID(MenuItemID::ANIM_WINK);
    dizzyAnimItem.setID(MenuItemID::ANIM_DIZZY);

    tempHumidItem.setID(MenuItemID::SENSOR_TEMP_HUM);
    soundLevelItem.setID(MenuItemID::SENSOR_SOUND);

    brightnessItem.setID(MenuItemID::SETTING_BRIGHTNESS);
    soundItem.setID(MenuItemID::SETTING_SOUND);
    sensitivityItem.setID(MenuItemID::SETTING_SENSITIVITY);

    wifiMenu.setID(MenuItemID::SETTING_WIFI);
    wifiConfigureItem.setID(MenuItemID::WIFI_CONFIGURE);
    wifiStatusItem.setID(MenuItemID::WIFI_STATUS);
    wifiForgetItem.setID(MenuItemID::WIFI_FORGET);

    weatherMenu.setID(MenuItemID::SETTING_WEATHER);
    weatherTestGeoItem.setID(MenuItemID::WEATHER_TEST_GEO);
    weatherTestForecastItem.setID(MenuItemID::WEATHER_TEST_FORECAST);
    weatherEnableItem.setID(MenuItemID::WEATHER_ENABLE);
    weatherViewItem.setID(MenuItemID::WEATHER_VIEW);
    weatherPrivacyItem.setID(MenuItemID::WEATHER_PRIVACY);

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
                // In menu mode, CLICK either toggles encoder edit mode for VALUE/TOGGLE
                // items, or performs normal SELECT (enter submenu / run action).
                {
                    MenuItem* currentItem = menuSystem.getCurrentItem();
                    if (currentItem != nullptr &&
                        (currentItem->getType() == MenuItemType::VALUE ||
                         currentItem->getType() == MenuItemType::TOGGLE)) {
                        // Toggle edit mode
                        encoderEditMode = !encoderEditMode;
                        menuSystem.setEditMode(encoderEditMode);
                        menuSystem.draw();
                    } else {
                        menuSystem.navigate(MenuNav::SELECT);
                        menuSystem.draw();
                    }
                }
                break;
                
            case ButtonEvent::LONG_PRESS:
                if (menuSystem.isAtRoot()) {
                    currentMode = AppMode::ANIMATIONS;
                    encoderEditMode = false;
                    menuSystem.setEditMode(false);
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
            encoderEditMode = false;
            menuSystem.setEditMode(false);
            menuSystem.draw();
        }
    }
}

void onTouchEvent(TouchEvent event) {
    #if TOUCH_ENABLED
    if (currentMode == AppMode::ANIMATIONS) {
        switch (event) {
            case TouchEvent::TAP:
                if (!animator.isPlaying()) {
                    animator.play(AnimState::SURPRISED, true);
                }
                break;

            case TouchEvent::DOUBLE_TAP:
                if (!animator.isPlaying()) {
                    animator.play(AnimState::WINK, true);  // Use wink for double tap
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
            // Only start dizzy if not already playing it
            if (!isShaking && animator.getCurrentState() != AnimState::DIZZY) {
                Serial.println("[SHAKE] Started - playing dizzy loop");
                isShaking = true;
                // Start continuous dizzy loop
                animator.play(AnimState::DIZZY, true, true);  // priority, forceLoop
            } else {
                // Already playing dizzy - just update shake time
                isShaking = true;
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
        // Show base frame immediately
        animator.showStaticFrame(AnimState::IDLE, 0);
    }
}

// ============================================================================
// BRIGHTNESS HELPER
// ============================================================================

void applyBrightnessFromSettings() {
    // settings.brightness is 10–100% in steps of 10
    uint8_t percent = constrain(settings.brightness, 10, 100);
    percent = (percent / 10) * 10;
    settings.brightness = percent;

    // Map 10–100% to a usable contrast range (approx. 10–100% of 255)
    uint8_t level = map(percent, 10, 100, 26, 255);
    display.setBrightness(level);
}

// ============================================================================
// WEATHER HELPERS
// ============================================================================

void testGeolocation() {
    if (!wifi.isConnected()) {
        Serial.println("[Weather] Not connected to WiFi");
        return;
    }

    Serial.println("[Weather] Testing geolocation...");
    size_t heapBefore = ESP.getFreeHeap();

    GeoLocationClient geoClient;
    GeoLocation location;

    if (geoClient.fetchLocation(location)) {
        Serial.printf("[Weather] Success! Location: %.4f, %.4f\n",
                      location.latitude, location.longitude);
        Serial.printf("[Weather] City: %s, Country: %s\n",
                      location.city, location.country);
    } else {
        Serial.println("[Weather] Geolocation failed");
    }

    size_t heapAfter = ESP.getFreeHeap();
    Serial.printf("[Weather] Heap used: %d bytes\n", heapBefore - heapAfter);
}

void testWeatherForecast() {
    if (!wifi.isConnected()) {
        Serial.println("[Weather] Not connected to WiFi");
        return;
    }

    Serial.println("[Weather] Testing weather forecast...");
    size_t heapBefore = ESP.getFreeHeap();

    // First get location
    GeoLocationClient geoClient;
    GeoLocation location;

    if (!geoClient.fetchLocation(location)) {
        Serial.println("[Weather] Failed to get location");
        return;
    }

    Serial.printf("[Weather] Location: %.4f, %.4f (%s, %s)\n",
                  location.latitude, location.longitude,
                  location.city, location.country);

    // Now get weather forecast
    WeatherClient weatherClient;
    WeatherForecast forecast;

    if (weatherClient.fetchForecast(location.latitude, location.longitude, forecast)) {
        Serial.println("[Weather] Forecast fetched successfully!");
        for (int i = 0; i < forecast.dayCount; i++) {
            Serial.printf("[Weather] %s: %.1f-%.1f°C, %.0f%% humidity, %s\n",
                          forecast.days[i].date,
                          forecast.days[i].tempMin,
                          forecast.days[i].tempMax,
                          forecast.days[i].humidity,
                          forecast.days[i].symbolCode);
        }
    } else {
        Serial.println("[Weather] Failed to fetch forecast");
    }

    size_t heapAfter = ESP.getFreeHeap();
    Serial.printf("[Weather] Total heap used: %d bytes\n", heapBefore - heapAfter);
}

// ============================================================================
// MENU STATE CALLBACK
// ============================================================================

void onMenuStateChange(MenuItem* item) {
    if (item == nullptr) return;

    resetMenuTimeout();

    MenuItemID itemID = item->getID();

    // Handle animation triggers
    switch (itemID) {
        case MenuItemID::ANIM_IDLE:
            currentMode = AppMode::ANIMATIONS;
            animator.play(AnimState::IDLE, true);
            break;

        case MenuItemID::ANIM_WINK:
            currentMode = AppMode::ANIMATIONS;
            animator.play(AnimState::WINK, true);
            break;

        case MenuItemID::ANIM_DIZZY:
            currentMode = AppMode::ANIMATIONS;
            animator.play(AnimState::DIZZY, true);
            break;

        case MenuItemID::SENSOR_TEMP_HUM:
        case MenuItemID::SENSOR_SOUND:
            currentMode = AppMode::SENSORS;
            break;

        case MenuItemID::WIFI_CONFIGURE:
            wifi.startCaptivePortal();
            currentMode = AppMode::WIFI_SETUP;
            break;

        case MenuItemID::WIFI_STATUS:
            currentMode = AppMode::WIFI_INFO;
            break;

        case MenuItemID::WIFI_FORGET:
            wifi.clearCredentials();
            wifi.disconnect();
            menuSystem.draw();  // Refresh display
            break;

        case MenuItemID::WEATHER_TEST_GEO:
            testGeolocation();
            break;

        case MenuItemID::WEATHER_TEST_FORECAST:
            testWeatherForecast();
            break;

        default:
            break;
    }

    // Handle settings changes
    switch (itemID) {
        case MenuItemID::SETTING_BRIGHTNESS: {
            // Item value is user-facing percent (10-100 in steps of 10)
            settings.brightness = (uint8_t)item->getValue();
            applyBrightnessFromSettings();
            break;
        }

        case MenuItemID::SETTING_SOUND:
            settings.soundEnabled = item->getValue() == 1;
            break;

        case MenuItemID::SETTING_SENSITIVITY: {
            settings.motionSensitivity = item->getValue();
            float threshold = 30.0f - (settings.motionSensitivity * 2.0f);
            motion.setShakeThreshold(threshold);
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// MODE UPDATE FUNCTIONS
// ============================================================================

void updateMenuMode() {
    // In encoder mode we navigate via onEncoderEvent() callbacks.
    // Here we just ensure the menu is drawn if needed (e.g., initial entry).
    static bool firstDraw = true;
    if (firstDraw || display.isDirty()) {
        firstDraw = false;
        menuSystem.draw();
    }
}

// ============================================================================
// ENCODER EVENT HANDLER
// ============================================================================

void onEncoderEvent(EncoderEvent event, int32_t /*position*/) {
    if (currentMode != AppMode::MENU) {
        return;
    }

    // One detent -> one logical step
    if (event == EncoderEvent::ROTATED_CW || event == EncoderEvent::ROTATED_CCW) {
        MenuItem* currentItem = menuSystem.getCurrentItem();
        bool isValueItem = currentItem != nullptr &&
                           (currentItem->getType() == MenuItemType::VALUE ||
                            currentItem->getType() == MenuItemType::TOGGLE);

        if (encoderEditMode && isValueItem) {
            // Edit mode: rotate to change the current VALUE/TOGGLE item
            int step = (event == EncoderEvent::ROTATED_CW) ? 1 : -1;

            int currentValue = currentItem->getValue();

            // For brightness setting, use 10% steps (10,20,...,100)
            if (currentItem->getID() == MenuItemID::SETTING_BRIGHTNESS) {
                // Snap to nearest 10 and step by ±10
                currentValue = (currentValue / 10) * 10;
                step *= 10;
            }

            int newValue = currentValue + step;
            newValue = constrain(newValue, currentItem->getMinValue(), currentItem->getMaxValue());

            if (newValue != currentValue) {
                currentItem->setValue(newValue);
                onMenuStateChange(currentItem);
                menuSystem.draw();
            }
        } else {
            // Navigation mode: rotate to move selection up/down
            if (event == EncoderEvent::ROTATED_CW) {
                menuSystem.navigate(MenuNav::DOWN);
            } else {
                menuSystem.navigate(MenuNav::UP);
            }
            // Leaving the current item cancels edit mode
            encoderEditMode = false;
            menuSystem.setEditMode(false);
            menuSystem.draw();
        }

        resetMenuTimeout();
    }
}

void updateAnimationsMode() {
    static bool wasPlaying = false;
    static uint8_t lastFrame = 255;
    bool isPlaying = animator.isPlaying();
    uint8_t currentFrame = animator.getCurrentFrame();

    // Track if redraw needed
    bool needsRedraw = false;

    // If animation just stopped, show base frame
    if (wasPlaying && !isPlaying && !isShaking) {
        animator.showStaticFrame(AnimState::IDLE, 0);
        needsRedraw = true;
        // Serial.println("[ANIM] Transition to base frame");
    }

    // Check if frame changed
    if (currentFrame != lastFrame) {
        needsRedraw = true;
    }

    // State changed
    if (wasPlaying != isPlaying) {
        needsRedraw = true;
    }

    wasPlaying = isPlaying;
    lastFrame = currentFrame;

    // Check for random animations (blink, wink)
    if (!isPlaying && !isShaking) {
        checkRandomAnimations();
    }

    // Only redraw when necessary
    if (needsRedraw || display.isDirty()) {
        display.clear();
        animator.draw();

        // Draw WiFi status icon in top-right corner
        drawWiFiStatusIcon();

        display.update();
    }
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

    display.update();
}

// ============================================================================
// WIFI FUNCTIONS
// ============================================================================

void onWiFiEvent(WiFiEvent event) {
    switch (event) {
        case WiFiEvent::AP_STARTED:
            Serial.println("[WiFi] Captive portal started");
            if (currentMode != AppMode::WIFI_SETUP) {
                currentMode = AppMode::WIFI_SETUP;
            }
            break;

        case WiFiEvent::CONNECTED:
            Serial.printf("[WiFi] Connected to %s\n", wifi.getSSID());
            Serial.printf("[WiFi] IP: %s\n", wifi.getIPAddress().c_str());
            if (currentMode == AppMode::WIFI_SETUP) {
                currentMode = AppMode::ANIMATIONS;
            }
            break;

        case WiFiEvent::DISCONNECTED:
            Serial.println("[WiFi] Disconnected");
            break;

        case WiFiEvent::FAILED:
            Serial.println("[WiFi] Connection failed");
            break;

        default:
            break;
    }
}

void drawWiFiStatusIcon() {
    #include "WiFiIcons.h"

    const uint8_t* icon = nullptr;

    if (wifi.isConnected()) {
        icon = wifi_connected;
    } else if (wifi.isAPActive()) {
        icon = wifi_ap;
    } else {
        WiFiState state = wifi.getState();
        switch (state) {
            case WiFiState::CONNECTING: icon = wifi_connecting; break;
            case WiFiState::DISCONNECTED:
            case WiFiState::FAILED:
            default: icon = wifi_disconnected; break;
        }
    }

    if (icon != nullptr) {
        display.drawBitmap(icon, 120, 0, 8, 8); // Top-right corner
    }
}

void updateWiFiSetupMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 500) return;
    lastUpdate = millis();

    display.clear();
    display.showTextCentered("WiFi Setup", 0, 1);

    display.drawText("Connect to:", 0, 16, 1);
    display.drawText(wifi.getAPName().c_str(), 0, 28, 1);
    display.drawText("Open browser:", 0, 40, 1);
    display.drawText("192.168.4.1", 0, 52, 1);

    display.update();
}

void updateWiFiInfoMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 1000) return;
    lastUpdate = millis();

    display.clear();
    display.showTextCentered("WiFi Status", 0, 1);

    bool connected = (WiFi.status() == WL_CONNECTED);
    WiFiState state = wifi.getState();

    if (connected) {
        // WiFi is really connected
        display.drawText("Connected", 0, 16, 1);
        display.drawText(wifi.getSSID(), 0, 28, 1);
        display.drawText(wifi.getIPAddress().c_str(), 0, 40, 1);

        char rssi[16];
        snprintf(rssi, sizeof(rssi), "RSSI: %d dBm", wifi.getSignalStrength());
        display.drawText(rssi, 0, 52, 1);

    } else if (wifi.hasCredentials()) {
        // WiFi credentials stored but not connected
        display.drawText("Configured only", 0, 16, 1);
        display.drawText(wifi.getConfiguredSSID(), 0, 28, 1);

        const char* stateStr = "Unknown";
        switch (state) {
            case WiFiState::IDLE: stateStr = "Idle"; break;
            case WiFiState::AP_MODE: stateStr = "AP Mode"; break;
            case WiFiState::CONNECTING: stateStr = "Connecting..."; break;
            case WiFiState::DISCONNECTED: stateStr = "Disconnected"; break;
            case WiFiState::FAILED: stateStr = "Failed"; break;
            default: break;
        }
        display.drawText("Status:", 0, 40, 1);
        display.drawText(stateStr, 0, 52, 1);

    } else {
        // No credentials at all
        display.drawText("Not configured", 0, 16, 1);
    }

    display.update();
}
