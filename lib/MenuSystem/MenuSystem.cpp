/**
 * @file MenuSystem.cpp
 * @brief Implementation of MenuSystem class
 */

#include "MenuSystem.h"

// ============================================================================
// LAYOUT CONSTANTS
// ============================================================================
#define TITLE_HEIGHT          10      // Reserved space for title bar
#define TITLE_Y               0       // Title Y position
#define TITLE_UNDERLINE_Y     7       // Underline Y position
#define ITEM_HEIGHT           12      // Height of each menu item row
#define ITEM_START_Y          16      // Y position where menu items start
#define ITEM_TEXT_OFFSET_Y    2       // Vertical offset for text within item row
#define ITEM_TEXT_X           4       // X position for unselected item text
#define ITEM_TEXT_X_SELECTED  10      // X position for selected item text (with arrow)
#define ITEM_ARROW_X          2       // X position for selection arrow
#define ITEM_RIGHT_INDICATOR_X 120    // X position for right-side indicators (> or E)
#define ITEM_VALUE_X          110     // X position for value/toggle display
#define ITEM_BOX_WIDTH        127     // Width of selection box (display width - 1)
#define ITEM_BOX_Y_OFFSET     -1      // Y offset for selection box

// Scrollbar constants
#define SCROLLBAR_X           124     // X position of scrollbar
#define SCROLLBAR_WIDTH       3       // Width of scrollbar track/thumb
#define SCROLLBAR_MIN_THUMB   4       // Minimum thumb height in pixels

// Empty menu message
#define EMPTY_MSG_X           64      // Center X (display width / 2)
#define EMPTY_MSG_Y           30      // Center Y

// Menu stack
#define MAX_MENU_DEPTH         10      // Maximum menu nesting depth

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
      _stateCallback(nullptr)
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
    _maxVisibleItems = (displayHeight - TITLE_HEIGHT) / ITEM_HEIGHT;
    
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
        if (_depth < MAX_MENU_DEPTH) {
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
    
    // Draw title (current menu name) with underline
    const char* title = _currentMenu->getText();
    uint8_t centerX = _display->getWidth() / 2;
    _display->drawText(title, centerX, TITLE_Y, 1, TextAlign::CENTER);
    _display->drawText("__________", centerX, TITLE_UNDERLINE_Y, 1, TextAlign::CENTER);
    
    // Get current menu items
    MenuItem** items = getCurrentMenuItems();
    uint8_t itemCount = getCurrentMenuItemCount();
    
    if (itemCount == 0) {
        _display->drawText("Empty", EMPTY_MSG_X, EMPTY_MSG_Y, 1, TextAlign::CENTER);
        _display->update();
        return;
    }
    
    // Draw visible items
    int16_t startY = ITEM_START_Y;
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
        _display->drawMenuBox(0, y + ITEM_BOX_Y_OFFSET, ITEM_BOX_WIDTH, ITEM_HEIGHT - 1, true);
        // Leading arrow to emphasize current selection
        _display->drawText(">", ITEM_ARROW_X, y + ITEM_TEXT_OFFSET_Y, 1);
    }
    
    // Item text
    int16_t textX = selected ? ITEM_TEXT_X_SELECTED : ITEM_TEXT_X;
    _display->drawText(item->getText(), textX, y + ITEM_TEXT_OFFSET_Y, 1);
    
    // Type-specific indicators
    if (item->hasChildren()) {
        // Submenu indicator
        _display->drawText(">", ITEM_RIGHT_INDICATOR_X, y + ITEM_TEXT_OFFSET_Y, 1);
    } else if (item->getType() == MenuItemType::VALUE) {
        // Value display
        char valueStr[8];
        snprintf(valueStr, sizeof(valueStr), "%d", item->getValue());
        _display->drawText(valueStr, ITEM_VALUE_X, y + ITEM_TEXT_OFFSET_Y, 1, TextAlign::RIGHT);

        // If in edit mode and this is the selected item, show a small edit tag
        if (selected && _editMode) {
            _display->drawText("E", ITEM_RIGHT_INDICATOR_X, y + ITEM_TEXT_OFFSET_Y, 1);
        }
    } else if (item->getType() == MenuItemType::TOGGLE) {
        // Toggle state
        const char* state = item->getValue() ? "ON" : "OFF";
        _display->drawText(state, ITEM_VALUE_X, y + ITEM_TEXT_OFFSET_Y, 1, TextAlign::RIGHT);

        if (selected && _editMode) {
            _display->drawText("E", ITEM_RIGHT_INDICATOR_X, y + ITEM_TEXT_OFFSET_Y, 1);
        }
    }
}

void MenuSystem::drawScrollIndicators() {
    uint8_t itemCount = getCurrentMenuItemCount();
    if (itemCount <= _maxVisibleItems) {
        return;
    }

    // Simple vertical scrollbar on the right edge
    int16_t trackX = SCROLLBAR_X;
    int16_t trackY = ITEM_START_Y;
    uint8_t trackHeight = _maxVisibleItems * ITEM_HEIGHT;

    // Draw scrollbar track
    _display->drawMenuBox(trackX, trackY, SCROLLBAR_WIDTH, trackHeight, false);

    // Thumb height proportional to visible items
    uint8_t thumbHeight = (uint8_t)max<int>(SCROLLBAR_MIN_THUMB, (trackHeight * _maxVisibleItems) / itemCount);

    // Thumb position based on scroll offset
    uint8_t maxOffset = itemCount - _maxVisibleItems;
    uint8_t thumbY = trackY;
    if (maxOffset > 0) {
        thumbY = trackY + (trackHeight - thumbHeight) * _scrollOffset / maxOffset;
    }

    _display->drawMenuBox(trackX, thumbY, SCROLLBAR_WIDTH, thumbHeight, true);
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

// Removed: legacy analog navigation (potentiometer-based) – encoder-only UX now.