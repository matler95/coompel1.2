/**
 * @file main.cpp
 * @brief ESP32-C3 Interactive Device - Display Test
 * @version 0.2.0
 * 
 * Phase 2: Testing DisplayManager library
 */

#include <Arduino.h>
#include "config.h"
#include "DisplayManager.h"  // Our new library!

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
DisplayManager display;

// ============================================================================
// DEMO STATE MACHINE
// ============================================================================
enum class DemoMode {
    TEXT_DEMO,
    BITMAP_DEMO,
    UI_DEMO,
    ANIMATION_DEMO
};

DemoMode currentDemo = DemoMode::TEXT_DEMO;
unsigned long lastDemoSwitch = 0;
const unsigned long DEMO_DURATION = 5000;  // 5 seconds per demo

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void runCurrentDemo();
void demoText();
void demoBitmap();
void demoUI();
void demoAnimation();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("ESP32-C3 Interactive Device v0.2.0");
    Serial.println("Phase 2: Display Manager Test");
    Serial.println("========================================\n");
    
    // Initialize display
    Serial.println("[INIT] Starting display...");
    if (!display.init(I2C_SDA_PIN, I2C_SCL_PIN)) {
        Serial.println("[ERROR] Display initialization failed!");
        Serial.println("[ERROR] Check I2C wiring and address");
        while (1) {
            delay(1000);  // Halt on critical error
        }
    }
    
    Serial.println("[INIT] Display ready!");
    Serial.println("========================================\n");
    
    delay(2000);  // Let user see boot screen
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    unsigned long currentTime = millis();
    
    // Auto-switch demo modes
    if (currentTime - lastDemoSwitch >= DEMO_DURATION) {
        lastDemoSwitch = currentTime;
        
        // Cycle through demos
        switch (currentDemo) {
            case DemoMode::TEXT_DEMO:
                currentDemo = DemoMode::BITMAP_DEMO;
                break;
            case DemoMode::BITMAP_DEMO:
                currentDemo = DemoMode::UI_DEMO;
                break;
            case DemoMode::UI_DEMO:
                currentDemo = DemoMode::ANIMATION_DEMO;
                break;
            case DemoMode::ANIMATION_DEMO:
                currentDemo = DemoMode::TEXT_DEMO;
                break;
        }
        
        Serial.printf("[DEMO] Switching to mode %d\n", (int)currentDemo);
    }
    
    // Run current demo
    runCurrentDemo();
    
    delay(100);  // Small delay for stability
}

// ============================================================================
// DEMO FUNCTIONS
// ============================================================================

void runCurrentDemo() {
    switch (currentDemo) {
        case DemoMode::TEXT_DEMO:
            demoText();
            break;
        case DemoMode::BITMAP_DEMO:
            demoBitmap();
            break;
        case DemoMode::UI_DEMO:
            demoUI();
            break;
        case DemoMode::ANIMATION_DEMO:
            demoAnimation();
            break;
    }
}

void demoText() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 1000) return;  // Update every 1 second
    lastUpdate = millis();
    
    display.clear();
    
    // Title
    display.drawText("TEXT DEMO", 64, 0, 1, TextAlign::CENTER);
    
    // Different sizes
    display.showTextCentered("Size 1", 12, 1);
    display.showTextCentered("Size 2", 24, 2);
    
    // Alignment test
    display.drawText("Left", 0, 48, 1, TextAlign::LEFT);
    display.drawText("Center", 64, 48, 1, TextAlign::CENTER);
    display.drawText("Right", 127, 48, 1, TextAlign::RIGHT);
    
    display.update();
}

void demoBitmap() {
    static unsigned long lastUpdate = 0;
    static bool showIdle = true;
    
    if (millis() - lastUpdate < 1000) return;  // Switch every 1 second
    lastUpdate = millis();
    showIdle = !showIdle;
    
    display.clear();
    display.drawText("BITMAP DEMO", 64, 0, 1, TextAlign::CENTER);
    
    // Show alternating bitmaps
    if (showIdle) {
        display.drawBitmapCentered(bitmap_idle, 32, 32);
        display.drawText("Idle", 64, 56, 1, TextAlign::CENTER);
    } else {
        display.drawBitmapCentered(bitmap_tap, 32, 32);
        display.drawText("Tap!", 64, 56, 1, TextAlign::CENTER);
    }
    
    display.update();
}

void demoUI() {
    static unsigned long lastUpdate = 0;
    static float progress = 0.0f;
    static int8_t battery = 100;
    
    if (millis() - lastUpdate < 100) return;
    lastUpdate = millis();
    
    display.clear();
    display.drawText("UI ELEMENTS", 64, 0, 1, TextAlign::CENTER);
    
    // Progress bar
    display.drawText("Progress:", 0, 12, 1);
    display.drawProgressBar(0, 22, 127, 8, progress);
    
    // Battery indicator
    display.drawText("Battery:", 0, 36, 1);
    display.drawBattery(60, 36, battery);
    char battText[8];
    snprintf(battText, sizeof(battText), "%d%%", battery);
    display.drawText(battText, 90, 36, 1);
    
    // Menu boxes
    display.drawText("Menu:", 0, 50, 1);
    display.drawMenuBox(40, 48, 20, 12, true);   // Selected
    display.drawMenuBox(65, 48, 20, 12, false);  // Unselected
    display.drawMenuBox(90, 48, 20, 12, false);
    
    display.update();
    
    // Animate values
    progress += 0.02f;
    if (progress > 1.0f) progress = 0.0f;
    
    battery -= 2;
    if (battery < 0) battery = 100;
}

void demoAnimation() {
    static unsigned long lastUpdate = 0;
    static uint8_t frame = 0;
    const uint8_t totalFrames = 8;
    
    if (millis() - lastUpdate < 200) return;  // 5 FPS
    lastUpdate = millis();
    
    display.clear();
    display.drawText("ANIMATION", 64, 0, 1, TextAlign::CENTER);
    
    // Show frame counter
    char frameText[32];
    snprintf(frameText, sizeof(frameText), "Frame %d/%d", frame + 1, totalFrames);
    display.showTextCentered(frameText, 28, 2);
    
    display.update();
    
    frame = (frame + 1) % totalFrames;
}