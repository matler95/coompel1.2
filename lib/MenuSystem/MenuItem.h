/**
 * @file MenuItem.h
 * @brief Individual menu item with callback support
 * @version 1.0.0
 * 
 * Represents a single item in a menu with:
 * - Display text
 * - Action callback
 * - Optional submenu
 * - Enabled/disabled state
 */

#ifndef MENU_ITEM_H
#define MENU_ITEM_H

#include <Arduino.h>

// Forward declaration
class MenuItem;

// ============================================================================
// CALLBACK TYPES
// ============================================================================
typedef void (*MenuCallback)();                    // Simple action callback
typedef void (*MenuValueCallback)(int value);      // Value adjustment callback

// ============================================================================
// MENU ITEM TYPES
// ============================================================================
enum class MenuItemType {
    ACTION,         // Execute callback when selected
    SUBMENU,        // Navigate to child menu
    VALUE,          // Adjustable value (integer)
    TOGGLE,         // On/Off switch
    INFO            // Display-only, no action
};

// ============================================================================
// MENU ITEM IDENTIFIERS
// ============================================================================
enum class MenuItemID : uint16_t {
    NONE = 0,

    // Main menu items
    MAIN_MENU = 1,
    ANIMATIONS_MENU = 10,
    SENSORS_MENU = 20,
    MOTION_TEST_MENU = 30,
    SETTINGS_MENU = 40,

    // Animation items
    ANIM_IDLE = 11,
    ANIM_WINK = 12,
    ANIM_DIZZY = 13,

    // Sensor items
    SENSOR_TEMP_HUM = 21,
    SENSOR_SOUND = 22,

    // Settings items
    SETTING_BRIGHTNESS = 41,
    SETTING_SOUND = 42,
    SETTING_SENSITIVITY = 43
};

// ============================================================================
// MENU ITEM CLASS
// ============================================================================
class MenuItem {
public:
    /**
     * @brief Constructor for action item
     * @param text Display text
     * @param callback Function to call when selected
     */
    MenuItem(const char* text, MenuCallback callback = nullptr);
    
    /**
     * @brief Constructor for value item
     * @param text Display text
     * @param initialValue Starting value
     * @param minValue Minimum value
     * @param maxValue Maximum value
     * @param callback Function to call when value changes
     */
    MenuItem(const char* text, int initialValue, int minValue, int maxValue, 
             MenuValueCallback callback = nullptr);
    
    /**
     * @brief Set item type
     * @param type MenuItemType
     */
    void setType(MenuItemType type);
    
    /**
     * @brief Get item type
     * @return MenuItemType
     */
    MenuItemType getType() const { return _type; }
    
    /**
     * @brief Get display text
     * @return Pointer to text string
     */
    const char* getText() const { return _text; }

    /**
     * @brief Set menu item ID
     * @param id MenuItemID identifier
     */
    void setID(MenuItemID id) { _id = id; }

    /**
     * @brief Get menu item ID
     * @return MenuItemID
     */
    MenuItemID getID() const { return _id; }

    /**
     * @brief Execute item action
     */
    void execute();
    
    /**
     * @brief Increase value (for VALUE type)
     */
    void incrementValue();
    
    /**
     * @brief Decrease value (for VALUE type)
     */
    void decrementValue();
    
    /**
     * @brief Get current value
     * @return Current value (for VALUE/TOGGLE types)
     */
    int getValue() const { return _value; }

    /**
     * @brief Get minimum value
     * @return Minimum value (for VALUE types)
     */
    int getMinValue() const { return _minValue; }

    /**
     * @brief Get maximum value
     * @return Maximum value (for VALUE types)
     */
    int getMaxValue() const { return _maxValue; }

    /**
     * @brief Set value directly
     * @param value New value
     */
    void setValue(int value);
    
    /**
     * @brief Toggle boolean value (for TOGGLE type)
     */
    void toggle();
    
    /**
     * @brief Check if item is enabled
     * @return true if enabled
     */
    bool isEnabled() const { return _enabled; }
    
    /**
     * @brief Enable/disable item
     * @param enabled true to enable
     */
    void setEnabled(bool enabled) { _enabled = enabled; }
    
    /**
     * @brief Add child menu item (for SUBMENU type)
     * @param item Pointer to child MenuItem
     */
    void addChild(MenuItem* item);
    
    /**
     * @brief Get child menu items
     * @return Array of child MenuItem pointers
     */
    MenuItem** getChildren() { return _children; }
    
    /**
     * @brief Get number of children
     * @return Child count
     */
    uint8_t getChildCount() const { return _childCount; }
    
    /**
     * @brief Check if item has children (is a submenu)
     * @return true if has children
     */
    bool hasChildren() const { return _childCount > 0; }

private:
    const char* _text;
    MenuItemType _type;
    bool _enabled;
    MenuItemID _id;

    // Value properties
    int _value;
    int _minValue;
    int _maxValue;

    // Callbacks
    MenuCallback _actionCallback;
    MenuValueCallback _valueCallback;

    // Submenu children
    MenuItem** _children;
    uint8_t _childCount;
    uint8_t _childCapacity;
    
    // Private methods
    void initChildren();
    void expandChildren();
};

#endif // MENU_ITEM_H