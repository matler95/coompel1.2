/**
 * @file MenuItem.cpp
 * @brief Implementation of MenuItem class
 */

#include "MenuItem.h"

#define MAX_CHILDREN 10

// ============================================================================
// CONSTRUCTORS
// ============================================================================

MenuItem::MenuItem(const char* text, MenuCallback callback)
    : _text(text),
      _type(MenuItemType::ACTION),
      _enabled(true),
      _value(0),
      _minValue(0),
      _maxValue(0),
      _actionCallback(callback),
      _valueCallback(nullptr),
      _children(nullptr),
      _childCount(0),
      _childCapacity(0)
{
}

MenuItem::MenuItem(const char* text, int initialValue, int minValue, int maxValue,
                   MenuValueCallback callback)
    : _text(text),
      _type(MenuItemType::VALUE),
      _enabled(true),
      _value(initialValue),
      _minValue(minValue),
      _maxValue(maxValue),
      _actionCallback(nullptr),
      _valueCallback(callback),
      _children(nullptr),
      _childCount(0),
      _childCapacity(0)
{
}

// ============================================================================
// TYPE & STATE
// ============================================================================

void MenuItem::setType(MenuItemType type) {
    _type = type;
}

// ============================================================================
// ACTIONS
// ============================================================================

void MenuItem::execute() {
    if (!_enabled) return;
    
    if (_type == MenuItemType::ACTION && _actionCallback != nullptr) {
        _actionCallback();
    } else if (_type == MenuItemType::TOGGLE) {
        toggle();
    }
}

void MenuItem::incrementValue() {
    if (_type != MenuItemType::VALUE && _type != MenuItemType::TOGGLE) return;
    
    if (_type == MenuItemType::TOGGLE) {
        _value = 1;
    } else {
        _value++;
        if (_value > _maxValue) _value = _maxValue;
    }
    
    if (_valueCallback != nullptr) {
        _valueCallback(_value);
    }
}

void MenuItem::decrementValue() {
    if (_type != MenuItemType::VALUE && _type != MenuItemType::TOGGLE) return;
    
    if (_type == MenuItemType::TOGGLE) {
        _value = 0;
    } else {
        _value--;
        if (_value < _minValue) _value = _minValue;
    }
    
    if (_valueCallback != nullptr) {
        _valueCallback(_value);
    }
}

void MenuItem::setValue(int value) {
    if (_type == MenuItemType::TOGGLE) {
        _value = value ? 1 : 0;
    } else {
        _value = constrain(value, _minValue, _maxValue);
    }
    
    if (_valueCallback != nullptr) {
        _valueCallback(_value);
    }
}

void MenuItem::toggle() {
    if (_type != MenuItemType::TOGGLE) return;
    
    _value = !_value;
    
    if (_valueCallback != nullptr) {
        _valueCallback(_value);
    }
}

// ============================================================================
// SUBMENU MANAGEMENT
// ============================================================================

void MenuItem::addChild(MenuItem* item) {
    if (_children == nullptr) {
        initChildren();
    }
    
    if (_childCount >= _childCapacity) {
        expandChildren();
    }
    
    _children[_childCount++] = item;
    _type = MenuItemType::SUBMENU;
}

void MenuItem::initChildren() {
    _childCapacity = 5;
    _children = new MenuItem*[_childCapacity];
    _childCount = 0;
}

void MenuItem::expandChildren() {
    uint8_t newCapacity = _childCapacity + 5;
    if (newCapacity > MAX_CHILDREN) return;
    
    MenuItem** newChildren = new MenuItem*[newCapacity];
    
    for (uint8_t i = 0; i < _childCount; i++) {
        newChildren[i] = _children[i];
    }
    
    delete[] _children;
    _children = newChildren;
    _childCapacity = newCapacity;
}