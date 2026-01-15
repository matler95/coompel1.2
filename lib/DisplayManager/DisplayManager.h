/**
 * @file DisplayManager.h
 * @brief High-level abstraction for SH1106 OLED display
 * @version 1.0.0
 * 
 * This library provides a clean API for display operations,
 * hiding the complexity of the Adafruit drivers.
 * 
 * Key Features:
 * - Easy initialization
 * - Text rendering with multiple sizes
 * - Bitmap/animation support
 * - Screen transitions
 * - Power management
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "Fonts/FreeMonoBold12pt7b.h"

// Forward declarations for bitmap data
extern const unsigned char PROGMEM bitmap_idle[];
extern const unsigned char PROGMEM bitmap_tap[];

/**
 * @brief Text alignment options
 */
enum class TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

/**
 * @brief Screen transition effects
 */
enum class Transition {
    NONE,
    FADE,
    SLIDE_LEFT,
    SLIDE_RIGHT
};

/**
 * @brief Main display manager class
 * 
 * Singleton pattern: Only one display instance should exist.
 * Usage example:
 *   DisplayManager display;
 *   display.init();
 *   display.showText("Hello!", TextAlign::CENTER);
 */
class DisplayManager {
public:
    // ========================================================================
    // CONSTRUCTOR & INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param width Display width in pixels (default: 128)
     * @param height Display height in pixels (default: 64)
     * @param i2c_address Display I2C address (default: 0x3C)
     */
    DisplayManager(uint8_t width = 128, uint8_t height = 64, uint8_t i2c_address = 0x3C);
    
    /**
     * @brief Initialize display hardware
     * @param sda_pin I2C SDA pin
     * @param scl_pin I2C SCL pin
     * @param frequency I2C frequency in Hz (default: 400000)
     * @return true if initialization successful
     */
    bool init(uint8_t sda_pin, uint8_t scl_pin, uint32_t frequency = 400000);
    
    // ========================================================================
    // BASIC DISPLAY OPERATIONS
    // ========================================================================
    
    /**
     * @brief Clear display buffer (does not update screen yet)
     */
    void clear();
    
    /**
     * @brief Update physical display with buffer contents
     * Call this after drawing operations to see changes
     */
    void update();
    
    /**
     * @brief Clear and update in one call
     */
    void clearAndUpdate();
    
    /**
     * @brief Set display brightness
     * @param level Brightness (0-255, where 255 is maximum)
     */
    void setBrightness(uint8_t level);
    
    /**
     * @brief Turn display on/off (for power saving)
     * @param on true to turn on, false to turn off
     */
    void setPower(bool on);

    /**
     * @brief Mark display buffer as dirty (needs update)
     */
    void markDirty();

    /**
     * @brief Check if display buffer needs updating
     * @return true if buffer has changed since last update
     */
    bool isDirty() const { return _dirty; }

    void setFont(const GFXfont* font);  
    // ========================================================================
    // TEXT RENDERING
    // ========================================================================
    
    /**
     * @brief Draw text at specific position
     * @param text String to display
     * @param x X coordinate (pixels)
     * @param y Y coordinate (pixels)
     * @param size Text size (1=small, 2=medium, 3=large)
     * @param align Horizontal alignment
     */
    void drawText(const char* text, int16_t x, int16_t y, uint8_t size = 1, 
                  TextAlign align = TextAlign::LEFT);
    
    /**
     * @brief Show centered text (convenience function)
     * @param text String to display
     * @param y Y coordinate (pixels)
     * @param size Text size
     */
    void showTextCentered(const char* text, int16_t y, uint8_t size = 2);
    
    /**
     * @brief Draw multi-line text (auto word wrap)
     * @param text String to display
     * @param x X coordinate
     * @param y Y coordinate
     * @param size Text size
     * @param lineSpacing Pixels between lines
     */
    void drawMultiLineText(const char* text, int16_t x, int16_t y, 
                           uint8_t size = 1, uint8_t lineSpacing = 8);
    
    // ========================================================================
    // GRAPHICS & BITMAPS
    // ========================================================================
    
    /**
     * @brief Draw a bitmap image
     * @param bitmap Pointer to bitmap data (PROGMEM)
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Bitmap width
     * @param height Bitmap height
     * @param color WHITE or BLACK (for monochrome)
     */
    void drawBitmap(const uint8_t* bitmap, int16_t x, int16_t y, 
                    uint8_t width, uint8_t height, uint16_t color = SH110X_WHITE);

    
    /**
     * @brief Draw centered bitmap
     * @param bitmap Pointer to bitmap data
     * @param width Bitmap width
     * @param height Bitmap height
     */
    void drawBitmapCentered(const uint8_t* bitmap, uint8_t width, uint8_t height);
    
    
    // ========================================================================
    // UI ELEMENTS
    // ========================================================================
    
    /**
     * @brief Draw progress bar
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Bar width
     * @param height Bar height
     * @param progress Progress (0.0 to 1.0)
     */
    void drawProgressBar(int16_t x, int16_t y, uint8_t width, 
                        uint8_t height, float progress);
    
    /**
     * @brief Draw battery indicator
     * @param x X coordinate
     * @param y Y coordinate
     * @param percentage Battery level (0-100)
     */
    void drawBattery(int16_t x, int16_t y, uint8_t percentage);
    
    /**
     * @brief Draw menu selection box
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Width
     * @param height Height
     * @param selected true if selected
     */
    void drawMenuBox(int16_t x, int16_t y, uint8_t width, 
                     uint8_t height, bool selected);
    
    // ========================================================================
    // SCREEN TRANSITIONS
    // ========================================================================
    
    /**
     * @brief Fade in from black
     * @param duration Fade duration in milliseconds
     */
    void fadeIn(uint16_t duration = 500);
    
    /**
     * @brief Fade out to black
     * @param duration Fade duration in milliseconds
     */
    void fadeOut(uint16_t duration = 500);
    
    // ========================================================================
    // GETTERS & UTILITIES
    // ========================================================================
    
    /**
     * @brief Get display width
     * @return Width in pixels
     */
    uint8_t getWidth() const { return _width; }
    
    /**
     * @brief Get display height
     * @return Height in pixels
     */
    uint8_t getHeight() const { return _height; }
    
    /**
     * @brief Check if display is initialized
     * @return true if ready to use
     */
    bool isReady() const { return _initialized; }
    
    /**
     * @brief Get raw display object (for advanced users)
     * Use with caution - bypasses abstraction layer
     * @return Pointer to Adafruit display object
     */
    Adafruit_SH1106G* getRawDisplay() { return _display; }

private:
    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================
    
    Adafruit_SH1106G* _display;   // Pointer to Adafruit driver
    
    uint8_t _width;               // Display width
    uint8_t _height;              // Display height
    uint8_t _i2c_address;         // I2C address
    bool _initialized;            // Init status flag
    bool _dirty;                  // Track if buffer needs display update

    // Animation state
    uint8_t _currentFrame;        // Current animation frame
    uint8_t _totalFrames;         // Total frames in animation
    uint8_t _animationFPS;        // Animation playback speed
    unsigned long _lastFrameTime; // Timestamp of last frame
    
    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    /**
     * @brief Calculate text width for alignment
     * @param text String to measure
     * @param size Text size
     * @return Width in pixels
     */
    int16_t getTextWidth(const char* text, uint8_t size);
    
    /**
     * @brief Map progress value to pixel position
     * @param progress Value 0.0-1.0
     * @param maxWidth Maximum width
     * @return Pixel position
     */
    uint8_t mapProgressToPixels(float progress, uint8_t maxWidth);
};

#endif // DISPLAY_MANAGER_H