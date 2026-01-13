/**
 * @file MenuSystem.h
 * @brief Hierarchical menu system with navigation
 * @version 1.0.0
 * 
 * Features:
 * - Multi-level menu navigation
 * - Smooth scrolling (if items exceed screen)
 * - Visual selection indicator
 * - Back navigation
 * - Event callbacks for menu changes
 */

#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>
#include "MenuItem.h"
#include "DisplayManager.h"

// ============================================================================
// MENU NAVIGATION ENUM
// ============================================================================
enum class MenuNav {
    UP,
    DOWN,
    SELECT,
    BACK
};

// ============================================================================
// MENU STATE CALLBACK
// ============================================================================
typedef void (*MenuStateCallback)(MenuItem* item);

// ============================================================================
// MENU SYSTEM CLASS
// ============================================================================
class MenuSystem {
public:
    /**
     * @brief Constructor
     * @param display Pointer to DisplayManager
     */
    MenuSystem(DisplayManager* display);
    
    /**
     * @brief Initialize menu system
     * @param rootItem Pointer to root menu item
     */
    void init(MenuItem* rootItem);
    
    /**
     * @brief Navigate menu
     * @param direction Navigation direction
     */
    void navigate(MenuNav direction);
    
    /**
     * @brief Draw current menu to display
     */
    void draw();
    
    /**
     * @brief Get currently selected item
     * @return Pointer to MenuItem
     */
    MenuItem* getCurrentItem();
    
    /**
     * @brief Get current menu depth
     * @return Depth level (0 = root)
     */
    uint8_t getDepth() const { return _depth; }
    
    /**
     * @brief Check if at root menu
     * @return true if at root
     */
    bool isAtRoot() const { return _depth == 0; }
    
    /**
     * @brief Set callback for menu state changes
     * @param callback Function to call
     */
    void setStateCallback(MenuStateCallback callback);
    
    /**
     * @brief Set maximum visible items (auto from display height)
     * @param maxItems Number of items visible at once
     */
    void setMaxVisibleItems(uint8_t maxItems);
    
    /**
     * @brief Return to root menu
     */
    void returnToRoot();

    /**
     * @brief Navigate using analog value (potentiometer/encoder)
     * @param value Analog input (0-4095 or 0-100)
     * @param maxValue Maximum input value (4095 for ADC, 100 for percent)
     */
    void navigateAnalog(uint16_t value, uint16_t maxValue = 4095);

    /**
     * @brief Set deadzone for analog navigation
     * @param percent Deadzone percentage (0-20, default 5)
     */
    void setAnalogDeadzone(uint8_t percent);

private:
    DisplayManager* _display;
    
    // Menu hierarchy
    MenuItem* _rootItem;
    MenuItem* _currentMenu;
    MenuItem* _menuStack[10];  // Navigation history
    uint8_t _depth;
    
    // Selection state
    uint8_t _selectedIndex;
    uint8_t _scrollOffset;
    uint8_t _maxVisibleItems;
    
    // Callback
    MenuStateCallback _stateCallback;
    
    // Private methods
    void enterSubmenu();
    void exitSubmenu();
    void executeCurrentItem();
    void adjustValue(int delta);
    void updateScrollOffset();
    void drawMenuItem(MenuItem* item, int16_t y, bool selected);
    void drawScrollIndicators();
    MenuItem** getCurrentMenuItems();
    uint8_t getCurrentMenuItemCount();
    uint16_t _lastAnalogValue;
    uint8_t _analogDeadzone;  // Percentage
};

#endif // MENU_SYSTEM_H