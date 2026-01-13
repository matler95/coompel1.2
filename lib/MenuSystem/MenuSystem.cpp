/**
 * @file MenuSystem.cpp
 * @brief Implementation of MenuSystem class
 */

#include "MenuSystem.h"

#define ITEM_HEIGHT 12
#define ITEM_PADDING 2

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

MenuSystem::MenuSystem(DisplayManager* display)
    : _display(display),
      _rootItem(nullptr),
      _currentMenu(nullptr),
      _depth(0),
      _selectedIndex(0),
      _scrollOffset(0),
      _maxVisibleItems(5),
      _stateCallback(nullptr),
      _lastAnalogValue(0),        // ADD THIS
      _analogDeadzone(5)          // ADD THIS (5% deadzone)
{
}

void MenuSystem::init(MenuItem* rootItem) {
    _rootItem = rootItem;
    _currentMenu = rootItem;
    _depth = 0;
    _selectedIndex = 0;
    _scrollOffset = 0;
    
    // Calculate max visible items from display height
    uint8_t displayHeight = _display->getHeight();
    _maxVisibleItems = (displayHeight - 10) / ITEM_HEIGHT;  // Leave space for title
    
    Serial.printf("[MENU] Initialized. Max visible: %d items\n", _maxVisibleItems);
}

// ============================================================================
// NAVIGATION
// ============================================================================

void MenuSystem::navigate(MenuNav direction) {
    uint8_t itemCount = getCurrentMenuItemCount();
    if (itemCount == 0) return;
    
    switch (direction) {
        case MenuNav::UP:
            if (_selectedIndex > 0) {
                _selectedIndex--;
                updateScrollOffset();
            }
            break;
            
        case MenuNav::DOWN:
            if (_selectedIndex < itemCount - 1) {
                _selectedIndex++;
                updateScrollOffset();
            }
            break;
            
        case MenuNav::SELECT:
            executeCurrentItem();
            break;
            
        case MenuNav::BACK:
            exitSubmenu();
            break;
    }
    
    // Trigger state callback
    if (_stateCallback != nullptr) {
        _stateCallback(getCurrentItem());
    }
}

void MenuSystem::enterSubmenu() {
    MenuItem* selected = getCurrentItem();
    
    if (selected != nullptr && selected->hasChildren()) {
        // Save current menu to stack
        if (_depth < 10) {
            _menuStack[_depth] = _currentMenu;
            _depth++;
        }
        
        // Enter submenu
        _currentMenu = selected;
        _selectedIndex = 0;
        _scrollOffset = 0;
        
        Serial.printf("[MENU] Entered: %s (depth %d)\n", selected->getText(), _depth);
    }
}

void MenuSystem::exitSubmenu() {
    if (_depth > 0) {
        _depth--;
        _currentMenu = _menuStack[_depth];
        _selectedIndex = 0;
        _scrollOffset = 0;
        
        Serial.printf("[MENU] Exited to: %s (depth %d)\n", _currentMenu->getText(), _depth);
    }
}

void MenuSystem::executeCurrentItem() {
    MenuItem* selected = getCurrentItem();
    if (selected == nullptr || !selected->isEnabled()) return;
    
    if (selected->hasChildren()) {
        // Navigate into submenu
        enterSubmenu();
    } else {
        // Execute action
        selected->execute();
        Serial.printf("[MENU] Executed: %s\n", selected->getText());
    }
}

void MenuSystem::adjustValue(int delta) {
    MenuItem* selected = getCurrentItem();
    if (selected == nullptr) return;
    
    if (selected->getType() == MenuItemType::VALUE) {
        if (delta > 0) {
            selected->incrementValue();
        } else {
            selected->decrementValue();
        }
    }
}

void MenuSystem::updateScrollOffset() {
    // Adjust scroll offset to keep selection visible
    if (_selectedIndex < _scrollOffset) {
        _scrollOffset = _selectedIndex;
    } else if (_selectedIndex >= _scrollOffset + _maxVisibleItems) {
        _scrollOffset = _selectedIndex - _maxVisibleItems + 1;
    }
}

// ============================================================================
// DRAWING
// ============================================================================

void MenuSystem::draw() {
    _display->clear();
    
    // Draw title (current menu name)
    const char* title = _currentMenu->getText();
    _display->drawText(title, 64, 0, 1, TextAlign::CENTER);
    _display->drawText("__________", 64, 7, 1, TextAlign::CENTER);
    
    // Get current menu items
    MenuItem** items = getCurrentMenuItems();
    uint8_t itemCount = getCurrentMenuItemCount();
    
    if (itemCount == 0) {
        _display->drawText("Empty", 64, 30, 1, TextAlign::CENTER);
        _display->update();
        return;
    }
    
    // Draw visible items - FIX HERE
    int16_t startY = 16;
    uint8_t remaining = itemCount - _scrollOffset;
    uint8_t visibleCount = (remaining < _maxVisibleItems) ? remaining : _maxVisibleItems;
    
    for (uint8_t i = 0; i < visibleCount; i++) {
        uint8_t itemIndex = _scrollOffset + i;
        bool isSelected = (itemIndex == _selectedIndex);
        
        int16_t y = startY + (i * ITEM_HEIGHT);
        drawMenuItem(items[itemIndex], y, isSelected);
    }
    
    // Draw scroll indicators if needed
    if (itemCount > _maxVisibleItems) {
        drawScrollIndicators();
    }
    
    _display->update();
}

void MenuSystem::drawMenuItem(MenuItem* item, int16_t y, bool selected) {
    if (item == nullptr) return;
    
    // Selection box
    if (selected) {
        _display->drawMenuBox(0, y - 1, 127, ITEM_HEIGHT - 1, true);
    }
    
    // Item text (inverted if selected)
    _display->drawText(item->getText(), 4, y + 2, 1);
    
    // Type-specific indicators
    if (item->hasChildren()) {
        // Submenu indicator
        _display->drawText(">", 120, y + 2, 1);
    } else if (item->getType() == MenuItemType::VALUE) {
        // Value display
        char valueStr[8];
        snprintf(valueStr, sizeof(valueStr), "%d", item->getValue());
        _display->drawText(valueStr, 110, y + 2, 1, TextAlign::RIGHT);
    } else if (item->getType() == MenuItemType::TOGGLE) {
        // Toggle state
        const char* state = item->getValue() ? "ON" : "OFF";
        _display->drawText(state, 110, y + 2, 1, TextAlign::RIGHT);
    }
}

void MenuSystem::drawScrollIndicators() {
    uint8_t itemCount = getCurrentMenuItemCount();
    
    // Up arrow
    if (_scrollOffset > 0) {
        _display->drawText("^", 120, 12, 1);
    }
    
    // Down arrow
    if (_scrollOffset + _maxVisibleItems < itemCount) {
        _display->drawText("v", 120, 56, 1);
    }
}

// ============================================================================
// GETTERS
// ============================================================================

MenuItem* MenuSystem::getCurrentItem() {
    MenuItem** items = getCurrentMenuItems();
    uint8_t itemCount = getCurrentMenuItemCount();
    
    if (_selectedIndex < itemCount) {
        return items[_selectedIndex];
    }
    return nullptr;
}

MenuItem** MenuSystem::getCurrentMenuItems() {
    if (_currentMenu != nullptr && _currentMenu->hasChildren()) {
        return _currentMenu->getChildren();
    }
    return nullptr;
}

uint8_t MenuSystem::getCurrentMenuItemCount() {
    if (_currentMenu != nullptr) {
        return _currentMenu->getChildCount();
    }
    return 0;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void MenuSystem::setStateCallback(MenuStateCallback callback) {
    _stateCallback = callback;
}

void MenuSystem::setMaxVisibleItems(uint8_t maxItems) {
    _maxVisibleItems = maxItems;
}

void MenuSystem::returnToRoot() {
    _currentMenu = _rootItem;
    _depth = 0;
    _selectedIndex = 0;
    _scrollOffset = 0;
}

// ============================================================================
// ANALOG NAVIGATION
// ============================================================================

void MenuSystem::navigateAnalog(uint16_t value, uint16_t maxValue) {
    uint8_t itemCount = getCurrentMenuItemCount();
    if (itemCount == 0) return;
    
    // Map analog value to menu items with deadzone
    uint16_t deadzoneValue = (maxValue * _analogDeadzone) / 100;
    
    // Apply deadzone at edges
    uint16_t adjustedValue = value;
    if (value < deadzoneValue) {
        adjustedValue = 0;
    } else if (value > (maxValue - deadzoneValue)) {
        adjustedValue = maxValue;
    }
    
    // Map to menu index
    uint8_t newIndex = map(adjustedValue, 0, maxValue, 0, itemCount - 1);
    
    // Only update if changed
    if (newIndex != _selectedIndex) {
        _selectedIndex = newIndex;
        updateScrollOffset();
        
        // Trigger state callback
        if (_stateCallback != nullptr) {
            _stateCallback(getCurrentItem());
        }
    }
    
    _lastAnalogValue = value;
}

void MenuSystem::setAnalogDeadzone(uint8_t percent) {
    _analogDeadzone = constrain(percent, 0, 20);
}