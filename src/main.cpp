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
#include "WeatherService.h"
#include "WeatherIcons.h"

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
WeatherService weatherService;

// ============================================================================
// APPLICATION STATE
// ============================================================================
enum class AppMode {
    ANIMATIONS,
    MENU,
    SENSORS,
    WIFI_SETUP,     // Show captive portal connection info
    WIFI_INFO,      // Show connection status details
    WEATHER_VIEW,   // Show weather forecast
    WEATHER_ABOUT,  // Show weather data attribution
    WEATHER_PRIVACY,// Show privacy info for weather feature
    CLOCK_VIEW,     // Show live clock
    POMODORO_VIEW   // Pomodoro timer
};

AppMode currentMode = AppMode::ANIMATIONS;
// Encoder edit mode: when true, rotating adjusts the current VALUE/TOGGLE item;
// when false, rotating moves selection up/down.
bool encoderEditMode = false;
// Weather view page: 0 = overview, 1-4 = individual day details
uint8_t weatherViewPage = 0;
// NTP time sync state
bool ntpConfigured = false;

// Pomodoro timer state
enum class PomodoroState { IDLE, WORK_RUNNING, WORK_PAUSED, BREAK_RUNNING, BREAK_PAUSED };
PomodoroState pomodoroState = PomodoroState::IDLE;
uint32_t pomodoroTargetMs = 0;        // When current session ends (millis)
uint32_t pomodoroPausedRemaining = 0; // Remaining time when paused
uint8_t pomodoroCount = 0;            // Completed work sessions (0-8)

// Pomodoro timing constants
constexpr uint32_t POMODORO_WORK_MS = 25UL * 60UL * 1000UL;   // 25 minutes
constexpr uint32_t POMODORO_BREAK_MS = 5UL * 60UL * 1000UL;   // 5 minutes

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
MenuItem weatherEnableItem("Weather", 1, 0, 1);  // Toggle: 0=Off, 1=On
MenuItem weatherViewItem("View Forecast");
MenuItem weatherPrivacyItem("Privacy Info");
MenuItem weatherAboutItem("About");

// System menu items (factory reset, re-run setup)
MenuItem systemMenu("System");
MenuItem rerunSetupItem("Re-run Setup");
MenuItem factoryResetItem("Factory Reset");

// Clock menu item
MenuItem clockItem("Clock");

// Pomodoro menu item
MenuItem pomodoroItem("Pomodoro");

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
void updateWeatherViewMode();
void updateWeatherAboutMode();
void updateWeatherPrivacyMode();
void updateClockViewMode();
void configureNTP();
void updatePomodoroViewMode();
void drawPomodoroRing(float progress);
void drawPomodoroCount(uint8_t count);
void setupBuzzer();
void pomodoroBeep();

void resetMenuTimeout();
void checkMenuTimeout();
void applyBrightnessFromSettings();
void applyDeviceConfig();
void showSetupRequiredScreen();
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

    // Initialize buzzer for audio feedback
    setupBuzzer();

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

    // Initialize WiFi (this loads device config internally)
    wifi.init();
    wifi.setEventCallback(onWiFiEvent);

    // Check if initial setup is complete
    if (!wifi.isSetupComplete()) {
        Serial.println("[INIT] Setup wizard not complete - showing setup screen");
        showSetupRequiredScreen();
        currentMode = AppMode::WIFI_SETUP;
    } else {
        Serial.println("[INIT] Setup complete - applying device config");
        applyDeviceConfig();
    }

    // Initialize weather service
    weatherService.init();

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
    weatherService.update();  // Non-blocking weather updates

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
        case AppMode::WEATHER_VIEW:
            updateWeatherViewMode();
            break;
        case AppMode::WEATHER_ABOUT:
            updateWeatherAboutMode();
            break;
        case AppMode::WEATHER_PRIVACY:
            updateWeatherPrivacyMode();
            break;
        case AppMode::CLOCK_VIEW:
            updateClockViewMode();
            break;
        case AppMode::POMODORO_VIEW:
            updatePomodoroViewMode();
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
    mainMenu.addChild(&clockItem);
    mainMenu.addChild(&pomodoroItem);
    mainMenu.addChild(&animationsMenu);
    mainMenu.addChild(&sensorsMenu);
    mainMenu.addChild(&settingsMenu);
    mainMenu.addChild(&test1Menu);
    mainMenu.addChild(&test2Menu);
    mainMenu.addChild(&test3Menu);
    mainMenu.addChild(&test4Menu);

    // Clock item setup
    clockItem.setType(MenuItemType::ACTION);
    clockItem.setID(MenuItemID::CLOCK_VIEW);

    // Pomodoro item setup
    pomodoroItem.setType(MenuItemType::ACTION);
    pomodoroItem.setID(MenuItemID::POMODORO_VIEW);

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
    settingsMenu.addChild(&systemMenu);

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
    weatherEnableItem.setType(MenuItemType::TOGGLE);
    weatherEnableItem.setValue(weatherService.isEnabled() ? 1 : 0);
    weatherViewItem.setType(MenuItemType::ACTION);
    weatherPrivacyItem.setType(MenuItemType::ACTION);
    weatherAboutItem.setType(MenuItemType::ACTION);

    weatherMenu.addChild(&weatherEnableItem);
    weatherMenu.addChild(&weatherViewItem);
    weatherMenu.addChild(&weatherPrivacyItem);
    weatherMenu.addChild(&weatherAboutItem);

    // Setup system menu (factory reset, re-run setup)
    systemMenu.setType(MenuItemType::SUBMENU);
    rerunSetupItem.setType(MenuItemType::ACTION);
    factoryResetItem.setType(MenuItemType::ACTION);

    systemMenu.addChild(&rerunSetupItem);
    systemMenu.addChild(&factoryResetItem);

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
    weatherEnableItem.setID(MenuItemID::WEATHER_ENABLE);
    weatherViewItem.setID(MenuItemID::WEATHER_VIEW);
    weatherPrivacyItem.setID(MenuItemID::WEATHER_PRIVACY);
    weatherAboutItem.setID(MenuItemID::WEATHER_ABOUT);

    systemMenu.setID(MenuItemID::SETTING_SYSTEM);
    rerunSetupItem.setID(MenuItemID::SYSTEM_RERUN_SETUP);
    factoryResetItem.setID(MenuItemID::SYSTEM_FACTORY_RESET);

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
    } else if (currentMode == AppMode::POMODORO_VIEW) {
        // Pomodoro timer controls
        if (event == ButtonEvent::CLICK) {
            // Toggle pause/resume or start
            switch (pomodoroState) {
                case PomodoroState::IDLE:
                    pomodoroState = PomodoroState::WORK_RUNNING;
                    pomodoroTargetMs = millis() + POMODORO_WORK_MS;
                    Serial.println("[Pomodoro] Work session started");
                    break;
                case PomodoroState::WORK_RUNNING:
                    pomodoroPausedRemaining = pomodoroTargetMs - millis();
                    pomodoroState = PomodoroState::WORK_PAUSED;
                    Serial.printf("[Pomodoro] Paused at %lu:%02lu\n",
                                  (pomodoroPausedRemaining / 1000) / 60,
                                  (pomodoroPausedRemaining / 1000) % 60);
                    break;
                case PomodoroState::WORK_PAUSED:
                    pomodoroTargetMs = millis() + pomodoroPausedRemaining;
                    pomodoroState = PomodoroState::WORK_RUNNING;
                    Serial.println("[Pomodoro] Resumed");
                    break;
                case PomodoroState::BREAK_RUNNING:
                    pomodoroPausedRemaining = pomodoroTargetMs - millis();
                    pomodoroState = PomodoroState::BREAK_PAUSED;
                    Serial.println("[Pomodoro] Break paused");
                    break;
                case PomodoroState::BREAK_PAUSED:
                    pomodoroTargetMs = millis() + pomodoroPausedRemaining;
                    pomodoroState = PomodoroState::BREAK_RUNNING;
                    Serial.println("[Pomodoro] Break resumed");
                    break;
            }
        } else if (event == ButtonEvent::LONG_PRESS) {
            // Stop and reset timer completely, then exit
            pomodoroState = PomodoroState::IDLE;
            pomodoroTargetMs = 0;
            pomodoroPausedRemaining = 0;
            pomodoroCount = 0;
            Serial.println("[Pomodoro] Timer stopped and reset");
            currentMode = AppMode::MENU;
            resetMenuTimeout();
            menuSystem.draw();
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
        if (!menuSystem.isAtRoot()) {
                    Serial.println("[MENU] Timeout - going back to root menu");
                    currentMode = AppMode::MENU;
                    menuSystem.init(&mainMenu);
        }
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

    Serial.println("[Weather] Force updating weather service...");

    // Force update to get fresh data
    if (weatherService.forceUpdate()) {
        const GeoLocation& location = weatherService.getLocation();
        const WeatherForecast& forecast = weatherService.getForecast();

        Serial.println("[Weather] Update successful!");
        Serial.printf("[Weather] Location: %.4f, %.4f (%s, %s)\n",
                      location.latitude, location.longitude,
                      location.city, location.country);

        if (forecast.valid) {
            Serial.printf("[Weather] Forecast: %d days\n", forecast.dayCount);
            for (int i = 0; i < forecast.dayCount; i++) {
                Serial.printf("[Weather]   %s: %.1f-%.1f°C, %.0f%%, %s\n",
                              forecast.days[i].date,
                              forecast.days[i].tempMin,
                              forecast.days[i].tempMax,
                              forecast.days[i].humidity,
                              forecast.days[i].symbolCode);
            }
        }

        Serial.printf("[Weather] Data cached, update interval: %lu hours\n",
                     weatherService.getUpdateInterval() / 3600UL);
    } else {
        Serial.println("[Weather] Update failed");
    }
}

void testWeatherForecast() {
    Serial.println("[Weather] Checking cached weather data...");

    if (weatherService.hasValidData()) {
        const GeoLocation& location = weatherService.getLocation();
        const WeatherForecast& forecast = weatherService.getForecast();

        Serial.printf("[Weather] Location: %s, %s\n", location.city, location.country);
        Serial.printf("[Weather] Forecast: %d days\n", forecast.dayCount);

        for (int i = 0; i < forecast.dayCount; i++) {
            Serial.printf("[Weather]   %s: %.1f-%.1f°C, %.0f%%, %s\n",
                          forecast.days[i].date,
                          forecast.days[i].tempMin,
                          forecast.days[i].tempMax,
                          forecast.days[i].humidity,
                          forecast.days[i].symbolCode);
        }

        uint32_t lastUpdate = weatherService.getLastUpdateTime();
        uint32_t now = millis() / 1000;
        uint32_t timeSince = now - lastUpdate;
        Serial.printf("[Weather] Last update: %lu seconds ago\n", timeSince);

        WeatherState state = weatherService.getState();
        Serial.printf("[Weather] State: %s\n",
                     state == WeatherState::CACHED ? "CACHED" :
                     state == WeatherState::STALE ? "STALE" :
                     state == WeatherState::ERROR ? "ERROR" : "OTHER");
    } else {
        Serial.println("[Weather] No valid weather data available");
        Serial.println("[Weather] Use 'Test Geolocation' to fetch data");
    }
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

        case MenuItemID::WEATHER_VIEW:
            weatherViewPage = 0;  // Start at overview
            currentMode = AppMode::WEATHER_VIEW;
            Serial.println("[NAV] Entered weather view");
            break;

        case MenuItemID::WEATHER_PRIVACY:
            currentMode = AppMode::WEATHER_PRIVACY;
            Serial.println("[NAV] Entered weather privacy");
            break;

        case MenuItemID::WEATHER_ABOUT:
            currentMode = AppMode::WEATHER_ABOUT;
            Serial.println("[NAV] Entered weather about");
            break;

        case MenuItemID::CLOCK_VIEW:
            currentMode = AppMode::CLOCK_VIEW;
            Serial.println("[NAV] Entered clock view");
            break;

        case MenuItemID::POMODORO_VIEW:
            currentMode = AppMode::POMODORO_VIEW;
            Serial.println("[NAV] Entered pomodoro timer");
            break;

        case MenuItemID::SYSTEM_RERUN_SETUP:
            Serial.println("[NAV] Re-running setup wizard");
            wifi.resetSetupWizard();  // Will restart device
            break;

        case MenuItemID::SYSTEM_FACTORY_RESET:
            Serial.println("[NAV] Factory reset initiated");
            wifi.factoryReset();  // Will restart device
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

        case MenuItemID::WEATHER_ENABLE: {
            bool enabled = item->getValue() == 1;
            weatherService.setEnabled(enabled);
            Serial.printf("[Weather] %s\n", enabled ? "Enabled" : "Disabled");
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
    // Handle weather view navigation
    if (currentMode == AppMode::WEATHER_VIEW) {
        if (event == EncoderEvent::ROTATED_CW || event == EncoderEvent::ROTATED_CCW) {
            const WeatherForecast& forecast = weatherService.getForecast();
            uint8_t maxPage = forecast.valid ? forecast.dayCount : 0;

            if (event == EncoderEvent::ROTATED_CW) {
                weatherViewPage++;
                if (weatherViewPage > maxPage) weatherViewPage = 0;
            } else {
                if (weatherViewPage == 0) {
                    weatherViewPage = maxPage;
                } else {
                    weatherViewPage--;
                }
            }
        }
        return;
    }

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

// ============================================================================
// WEATHER VIEW MODE
// ============================================================================

void updateWeatherViewMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 100) return;
    lastUpdate = millis();

    display.clear();

    if (!weatherService.hasValidData()) {
        display.showTextCentered("Weather", 0, 1);

        WeatherState state = weatherService.getState();

        if (state == WeatherState::FETCHING_LOCATION) {
            display.drawText("Getting", 0, 20, 1);
            display.drawText("location...", 0, 32, 1);
        } else if (state == WeatherState::FETCHING_WEATHER) {
            display.drawText("Getting", 0, 20, 1);
            display.drawText("forecast...", 0, 32, 1);
        } else if (state == WeatherState::ERROR) {
            display.drawText("Error:", 0, 20, 1);
            display.drawText(weatherService.getErrorString(), 0, 32, 1);

            // Show retry info
            char retryStr[24];
            snprintf(retryStr, sizeof(retryStr), "Retry %d/%d",
                    weatherService.getRetryCount(), 3);
            display.drawText(retryStr, 0, 44, 1);
        } else if (!wifi.isConnected()) {
            display.drawText("No WiFi", 0, 20, 1);
            display.drawText("connection", 0, 32, 1);
        } else {
            display.drawText("No data", 0, 20, 1);
            display.drawText("available", 0, 32, 1);
        }

        display.update();
        return;
    }

    const WeatherForecast& forecast = weatherService.getForecast();
    const GeoLocation& location = weatherService.getLocation();

    if (weatherViewPage == 0) {
        // Overview page: show all 4 days in compact format
        // Header: City name
        display.drawText(location.city, 0, 0, 1);

        // Show 4 days in compact format
        // Each row: icon (8px) + date (5 chars) + temp range
        for (int i = 0; i < forecast.dayCount && i < 4; i++) {
            int y = 14 + (i * 12);

            // Draw weather icon (8x8)
            const uint8_t* icon = getWeatherIcon(forecast.days[i].symbolCode);
            display.drawBitmap(icon, 0, y, 8, 8, 1);

            // Date: show day of month only (chars 8-9 from YYYY-MM-DD)
            char dayNum[4];
            strncpy(dayNum, &forecast.days[i].date[8], 2);
            dayNum[2] = '\0';
            display.drawText(dayNum, 12, y, 1);

            // Temperature range
            char tempStr[16];
            snprintf(tempStr, sizeof(tempStr), "%.0f/%.0f",
                    forecast.days[i].tempMin, forecast.days[i].tempMax);
            display.drawText(tempStr, 30, y, 1);

            // Humidity
            char humStr[8];
            snprintf(humStr, sizeof(humStr), "%.0f%%", forecast.days[i].humidity);
            display.drawText(humStr, 80, y, 1);
        }

        // Footer: scroll hint
        display.drawText("Rotate: details", 0, 56, 1);

    } else {
        // Detail page for single day (1-4)
        uint8_t dayIdx = weatherViewPage - 1;
        if (dayIdx >= forecast.dayCount) {
            weatherViewPage = 0;
            return;
        }

        const DailyForecast& day = forecast.days[dayIdx];

        // Header: Full date
        display.drawText(day.date, 0, 0, 1);

        // Large weather icon (centered, 8x8 but we can draw it larger conceptually)
        const uint8_t* icon = getWeatherIcon(day.symbolCode);
        display.drawBitmap(icon, 60, 0, 8, 8, 1);

        // Temperature
        char tempLine[32];
        snprintf(tempLine, sizeof(tempLine), "Temp: %.1f - %.1f C",
                day.tempMin, day.tempMax);
        display.drawText(tempLine, 0, 16, 1);

        // Humidity
        char humLine[20];
        snprintf(humLine, sizeof(humLine), "Humidity: %.0f%%", day.humidity);
        display.drawText(humLine, 0, 28, 1);

        // Weather condition
        display.drawText("Cond:", 0, 40, 1);
        // Truncate symbol code if too long
        char symbolShort[16];
        strncpy(symbolShort, day.symbolCode, 15);
        symbolShort[15] = '\0';
        display.drawText(symbolShort, 36, 40, 1);

        // Footer: navigation hint
        char navHint[24];
        snprintf(navHint, sizeof(navHint), "Day %d/%d  Rotate:nav",
                dayIdx + 1, forecast.dayCount);
        display.drawText(navHint, 0, 56, 1);
    }

    display.update();
}

// ============================================================================
// WEATHER ABOUT MODE - Attribution screen
// ============================================================================

void updateWeatherAboutMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 500) return;
    lastUpdate = millis();

    display.clear();

    // Title
    display.showTextCentered("Weather Data", 0, 1);

    // Attribution text
    display.drawText("Provided by", 0, 16, 1);
    display.drawText("MET Norway", 0, 28, 1);
    display.drawText("yr.no", 0, 40, 1);

    // Footer hint
    
    display.update();
}

// ============================================================================
// WEATHER PRIVACY MODE - Privacy information screen
// ============================================================================

void updateWeatherPrivacyMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 500) return;
    lastUpdate = millis();

    display.clear();

    // Title
    display.showTextCentered("Privacy Info", 0, 1);

    // Privacy text - explain IP-based geolocation
    display.drawText("Weather uses", 0, 14, 1);
    display.drawText("IP geolocation", 0, 24, 1);
    display.drawText("for city-level", 0, 34, 1);
    display.drawText("location only.", 0, 44, 1);

    // Footer hint
    
    display.update();
}

// ============================================================================
// NTP TIME CONFIGURATION
// ============================================================================

void configureNTP() {
    if (ntpConfigured) return;

    // Get timezone offset from geolocation (default to UTC if not available)
    int32_t gmtOffset = 0;
    if (weatherService.getLocation().valid) {
        gmtOffset = weatherService.getLocation().timezoneOffset;
    }

    // Configure NTP with timezone (no DST adjustment - handled by offset)
    configTime(gmtOffset, 0, "pool.ntp.org", "time.nist.gov");
    ntpConfigured = true;

    Serial.printf("[Time] NTP configured, GMT offset: %ld sec\n", gmtOffset);
}

// ============================================================================
// CLOCK VIEW MODE – Modern live clock
// ============================================================================

void updateClockViewMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 500) return;   // 2 FPS (for blinking colon)
    lastUpdate = millis();

    // Ensure NTP is configured
    if (!ntpConfigured && wifi.isConnected()) {
        configureNTP();
    }

    display.clear();

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        // ---- No time available ----
        display.showTextCentered("NO TIME", 14, 2);
        display.showTextCentered("Connect WiFi", 34, 1);
        display.showTextCentered("to sync clock", 46, 1);
        display.update();
        return;
    }

    // ---- Time (large, modern, blinking colon) ----
    bool blink = (timeinfo.tm_sec % 2) == 0;
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr),
             blink ? "%02d:%02d" : "%02d %02d",
             timeinfo.tm_hour,
             timeinfo.tm_min);

    display.showTextCentered(timeStr, 6, 3);

    // // ---- Subtle separator line ----
    // display.drawFastHLine(32, 30, 64, SH110X_WHITE);

    // ---- Day (small) ----
    char dayStr[10];
    strftime(dayStr, sizeof(dayStr), "%A", &timeinfo);
    display.showTextCentered(dayStr, 36, 1);

    // ---- Date (compact, modern) ----
    char dateStr[16];
    strftime(dateStr, sizeof(dateStr), "%d %b %Y", &timeinfo);
    display.showTextCentered(dateStr, 48, 1);

    display.update();
}

// ============================================================================
// POMODORO TIMER
// ============================================================================

void setupBuzzer() {
    ledcSetup(0, 2000, 8);        // Channel 0, 2kHz default, 8-bit resolution
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, 0);              // Start silent
}

void pomodoroBeep() {
    if (!settings.soundEnabled) return;

    // Double beep pattern
    for (int i = 0; i < 2; i++) {
        ledcWriteTone(0, 2000);   // 2kHz tone
        delay(100);
        ledcWriteTone(0, 0);      // Silence
        delay(100);
    }
}

void drawPomodoroRing(float progress) {
    Adafruit_SH1106G* d = display.getRawDisplay();

    int16_t cx = 96, cy = 32, r = 24;

    // Outer ring (thin)
    d->drawCircle(cx, cy, r, SH110X_WHITE);

    // Progress dots around ring
    int totalDots = 30;  // Number of segments
    int filledDots = (int)(progress * totalDots);

    for (int i = 0; i < totalDots; i++) {
        float angle = ((float)i / totalDots) * 2.0f * PI - PI / 2.0f;
        int16_t x = cx + (int16_t)(cos(angle) * r);
        int16_t y = cy + (int16_t)(sin(angle) * r);

        if (i < filledDots) {
            d->fillCircle(x, y, 2, SH110X_WHITE);  // Filled
        } else {
            d->drawPixel(x, y, SH110X_WHITE);       // Dot
        }
    }
}

void drawPomodoroCount(uint8_t count) {
    // Draw small filled circles for completed pomodoros (monochrome)
    Adafruit_SH1106G* d = display.getRawDisplay();
    for (int i = 0; i < count && i < 8; i++) {
        int16_t x = 90 + (i % 4) * 10;
        int16_t y = 2 + (i / 4) * 8;
        d->fillCircle(x, y + 3, 3, SH110X_WHITE);
    }
}

void updatePomodoroViewMode() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 100) return;  // 10 FPS
    lastUpdate = millis();

    uint32_t now = millis();
    uint32_t remaining = 0;
    float progress = 0.0f;
    bool isBreak = (pomodoroState == PomodoroState::BREAK_RUNNING ||
                    pomodoroState == PomodoroState::BREAK_PAUSED);

    // Calculate remaining time and progress based on state
    if (pomodoroState == PomodoroState::IDLE) {
        remaining = POMODORO_WORK_MS;  // Show full 25:00
        progress = 0.0f;
    } else if (pomodoroState == PomodoroState::WORK_RUNNING ||
               pomodoroState == PomodoroState::BREAK_RUNNING) {
        remaining = (pomodoroTargetMs > now) ? (pomodoroTargetMs - now) : 0;
        uint32_t total = isBreak ? POMODORO_BREAK_MS : POMODORO_WORK_MS;
        progress = 1.0f - ((float)remaining / total);

        // Auto-transition when done
        if (remaining == 0) {
            pomodoroBeep();  // Sound alert
            if (!isBreak) {
                pomodoroCount++;
                pomodoroState = PomodoroState::BREAK_RUNNING;
                pomodoroTargetMs = now + POMODORO_BREAK_MS;
                Serial.println("[Pomodoro] Work complete! Starting break");
            } else {
                pomodoroState = PomodoroState::WORK_RUNNING;
                pomodoroTargetMs = now + POMODORO_WORK_MS;
                Serial.printf("[Pomodoro] Break complete! Starting work #%d\n", pomodoroCount + 1);
            }
        }
    } else if (pomodoroState == PomodoroState::WORK_PAUSED ||
               pomodoroState == PomodoroState::BREAK_PAUSED) {
        remaining = pomodoroPausedRemaining;
        uint32_t total = isBreak ? POMODORO_BREAK_MS : POMODORO_WORK_MS;
        progress = 1.0f - ((float)remaining / total);
    }

    display.clear();

    // Header: Mode label
    display.drawText("POMODORO", 4, 0, 1);

    // Pomodoro count icons
    drawPomodoroCount(pomodoroCount);

    // Draw circular progress ring
    drawPomodoroRing(progress);

    // Time on the left
    uint32_t secs = remaining / 1000;
    char timeStr[8];
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu", secs / 60, secs % 60);
    display.drawText(timeStr, 4, 22, 2);   // LEFT side

    // Status inside ring
    const char* status = "";
    switch (pomodoroState) {
        case PomodoroState::IDLE: status = "START"; break;
        case PomodoroState::WORK_RUNNING: status = "FOCUS"; break;
        case PomodoroState::WORK_PAUSED: status = "PAUSED"; break;
        case PomodoroState::BREAK_RUNNING: status = "RELAX"; break;
        case PomodoroState::BREAK_PAUSED: status = "PAUSED"; break;
    }
    display.drawText(status, 8, 42, 1);


    display.update();
}

// ============================================================================
// DEVICE CONFIGURATION HELPERS
// ============================================================================

void showSetupRequiredScreen() {
    display.clear();
    display.showTextCentered("Setup", 8, 2);
    display.drawText("Connect to:", 16, 32, 1);
    display.showTextCentered(wifi.getAPName().c_str(), 44, 1);
    display.update();
}

void applyDeviceConfig() {
    const DeviceConfig& cfg = wifi.getDeviceConfig();

    Serial.println("[Config] Applying device configuration:");
    Serial.printf("  WiFi: %s\n", cfg.wifiEnabled ? "enabled" : "disabled");
    Serial.printf("  Geolocation: %s\n", cfg.geolocationEnabled ? "enabled" : "disabled");
    Serial.printf("  Weather: %s\n", cfg.weatherEnabled ? "enabled" : "disabled");
    Serial.printf("  NTP: %s\n", cfg.ntpEnabled ? "enabled" : "disabled");
    Serial.printf("  Manual TZ: %ld sec\n", cfg.manualTimezoneOffset);

    // Apply weather service settings based on user consent
    if (cfg.geolocationEnabled && cfg.weatherEnabled) {
        weatherService.setEnabled(true);
        weatherEnableItem.setValue(1);
    } else {
        weatherService.setEnabled(false);
        weatherEnableItem.setValue(0);
    }

    // Apply NTP settings
    if (cfg.ntpEnabled && cfg.geolocationEnabled) {
        // Will use geolocation timezone when available
        ntpConfigured = false;  // Let it configure when WiFi connects
    } else if (cfg.ntpEnabled && !cfg.geolocationEnabled) {
        // Use manual timezone offset
        configTime(cfg.manualTimezoneOffset, 0, "pool.ntp.org", "time.nist.gov");
        ntpConfigured = true;
        Serial.printf("[Time] NTP configured with manual offset: %ld sec\n", cfg.manualTimezoneOffset);
    } else {
        // NTP disabled
        Serial.println("[Time] NTP disabled by user");
    }
}
