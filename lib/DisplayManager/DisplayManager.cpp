/**
 * @file DisplayManager.cpp
 * @brief Implementation of DisplayManager class
 */

#include "DisplayManager.h"
#include "bitmaps.h"

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

DisplayManager::DisplayManager(uint8_t width, uint8_t height, uint8_t i2c_address)
    : _display(nullptr),
      _width(width),
      _height(height),
      _i2c_address(i2c_address),
      _initialized(false),
      _dirty(true),
      _currentFrame(0),
      _totalFrames(0),
      _animationFPS(10),
      _lastFrameTime(0)
{
    // Constructor intentionally lightweight
    // Actual hardware initialization happens in init()
}

bool DisplayManager::init(uint8_t sda_pin, uint8_t scl_pin, uint32_t frequency) {
    Serial.println("[DISPLAY] Initializing SH1106 OLED...");
    
    // Initialize I2C
    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(frequency);
    
    // Create display object
    _display = new Adafruit_SH1106G(_width, _height, &Wire, -1);
    
    // Initialize display
    if (!_display->begin(_i2c_address, true)) {
        Serial.println("[DISPLAY] ERROR: Failed to initialize!");
        Serial.printf("[DISPLAY] Check I2C address 0x%02X and wiring\n", _i2c_address);
        return false;
    }
    
    // Configure display
    _display->clearDisplay();
    _display->setTextColor(SH110X_WHITE);
    _display->setTextWrap(false);
    _display->cp437(true);  // Code page 437 font
    
    _initialized = true;
    
    // Show startup splash
    showTextCentered("BOOTING...", 28, 2);
    update();
    delay(1000);
    
    Serial.println("[DISPLAY] Initialization complete");
    Serial.printf("[DISPLAY] Resolution: %dx%d px\n", _width, _height);
    
    return true;
}

// ============================================================================
// BASIC DISPLAY OPERATIONS
// ============================================================================

void DisplayManager::clear() {
    if (!_initialized) return;
    _display->clearDisplay();
    _dirty = true;
}

void DisplayManager::update() {
    if (!_initialized || !_dirty) return;
    _display->display();
    _dirty = false;
}

void DisplayManager::clearAndUpdate() {
    clear();
    update();
}

void DisplayManager::setBrightness(uint8_t level) {
    if (!_initialized) return;

    // SH1106 contrast control (0x81 command)
    // Level: 0-255, where 255 is maximum brightness
    _display->oled_command(0x81);  // Set contrast control
    _display->oled_command(level);  // Contrast value

    Serial.printf("[DISPLAY] Brightness set to %d\n", level);
}

void DisplayManager::setPower(bool on) {
    if (!_initialized) return;
    if (on) {
        _display->oled_command(SH110X_DISPLAYON);
    } else {
        _display->oled_command(SH110X_DISPLAYOFF);
    }
}

void DisplayManager::markDirty() {
    _dirty = true;
}

// ============================================================================
// TEXT RENDERING
// ============================================================================

void DisplayManager::drawText(const char* text, int16_t x, int16_t y,
                              uint8_t size, TextAlign align) {
    if (!_initialized || text == nullptr) return;

    _display->setTextSize(size);

    // Calculate alignment offset
    int16_t xPos = x;
    if (align != TextAlign::LEFT) {
        int16_t textWidth = getTextWidth(text, size);
        if (align == TextAlign::CENTER) {
            xPos = x - (textWidth / 2);
        } else if (align == TextAlign::RIGHT) {
            xPos = x - textWidth;
        }
    }

    _display->setCursor(xPos, y);
    _display->print(text);
    _dirty = true;
}

void DisplayManager::showTextCentered(const char* text, int16_t y, uint8_t size) {
    drawText(text, _width / 2, y, size, TextAlign::CENTER);
}

void DisplayManager::drawMultiLineText(const char* text, int16_t x, int16_t y,
                                       uint8_t size, uint8_t lineSpacing) {
    if (!_initialized || text == nullptr) return;

    _display->setTextSize(size);

    char buffer[64];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* line = strtok(buffer, "\n");
    int16_t currentY = y;

    while (line != nullptr) {
        _display->setCursor(x, currentY);
        _display->print(line);
        currentY += (8 * size) + lineSpacing;
        line = strtok(nullptr, "\n");
    }
    _dirty = true;
}

// ============================================================================
// GRAPHICS & BITMAPS
// ============================================================================

void DisplayManager::drawBitmap(const uint8_t* bitmap, int16_t x, int16_t y,
                                uint8_t width, uint8_t height, uint16_t color) {
    if (!_initialized || bitmap == nullptr) return;
    _display->drawBitmap(x, y, bitmap, width, height, color);
    _dirty = true;
}

void DisplayManager::drawBitmapCentered(const uint8_t* bitmap, 
                                        uint8_t width, uint8_t height) {
    int16_t x = (_width - width) / 2;
    int16_t y = (_height - height) / 2;
    drawBitmap(bitmap, x, y, width, height, SH110X_WHITE);
}

// ============================================================================
// ANIMATION SUPPORT
// ============================================================================

bool DisplayManager::playAnimationFrame(uint8_t frameIndex, uint8_t totalFrames) {
    if (!_initialized || frameIndex >= totalFrames) return false;
    
    _currentFrame = frameIndex;
    _totalFrames = totalFrames;
    
    // For now, just show a placeholder
    // In Phase 7, we'll load actual bitmap frames
    clear();
    char frameText[32];
    snprintf(frameText, sizeof(frameText), "Frame %d/%d", frameIndex + 1, totalFrames);
    showTextCentered(frameText, 28, 1);
    update();
    
    return true;
}

uint8_t DisplayManager::updateAnimation(unsigned long deltaTime) {
    if (!_initialized || _totalFrames == 0) return 0;
    
    _lastFrameTime += deltaTime;
    unsigned long frameDelay = 1000 / _animationFPS;
    
    if (_lastFrameTime >= frameDelay) {
        _lastFrameTime = 0;
        _currentFrame = (_currentFrame + 1) % _totalFrames;
        playAnimationFrame(_currentFrame, _totalFrames);
    }
    
    return _currentFrame;
}

void DisplayManager::setAnimationFPS(uint8_t fps) {
    _animationFPS = constrain(fps, 1, 30);
}

// ============================================================================
// UI ELEMENTS
// ============================================================================

void DisplayManager::drawProgressBar(int16_t x, int16_t y, uint8_t width,
                                     uint8_t height, float progress) {
    if (!_initialized) return;

    progress = constrain(progress, 0.0f, 1.0f);

    // Draw outline
    _display->drawRect(x, y, width, height, SH110X_WHITE);

    // Fill progress
    uint8_t fillWidth = mapProgressToPixels(progress, width - 2);
    if (fillWidth > 0) {
        _display->fillRect(x + 1, y + 1, fillWidth, height - 2, SH110X_WHITE);
    }
    _dirty = true;
}

void DisplayManager::drawBattery(int16_t x, int16_t y, uint8_t percentage) {
    if (!_initialized) return;

    // Battery body (20x10 pixels)
    _display->drawRect(x, y, 20, 10, SH110X_WHITE);
    // Battery tip
    _display->fillRect(x + 20, y + 3, 2, 4, SH110X_WHITE);

    // Fill level
    uint8_t fillWidth = map(percentage, 0, 100, 0, 18);
    if (fillWidth > 0) {
        _display->fillRect(x + 1, y + 1, fillWidth, 8, SH110X_WHITE);
    }
    _dirty = true;
}

void DisplayManager::drawMenuBox(int16_t x, int16_t y, uint8_t width,
                                 uint8_t height, bool selected) {
    if (!_initialized) return;

    if (selected) {
        // Filled box for selected items
        _display->fillRect(x, y, width, height, SH110X_WHITE);
    } else {
        // Outline only for unselected
        _display->drawRect(x, y, width, height, SH110X_WHITE);
    }
    _dirty = true;
}

// ============================================================================
// SCREEN TRANSITIONS
// ============================================================================

void DisplayManager::fadeIn(uint16_t duration) {
    // SH1106 doesn't support hardware fade
    // Could implement by rapidly updating display content
    // For now, simple instant on
    setPower(true);
}

void DisplayManager::fadeOut(uint16_t duration) {
    setPower(false);
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

int16_t DisplayManager::getTextWidth(const char* text, uint8_t size) {
    // Each character is 6 pixels wide in default font
    return strlen(text) * 6 * size;
}

uint8_t DisplayManager::mapProgressToPixels(float progress, uint8_t maxWidth) {
    return (uint8_t)(progress * maxWidth);
}