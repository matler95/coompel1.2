# Code Cleanup Report
**Project:** ESP32-C3 Interactive Device (coompel1.2)
**Analysis Date:** 2026-01-14
**Status:** Ready for Cleanup

---

## Executive Summary

This report identifies unused, redundant, and dead code in the codebase. The project is well-structured overall, but contains remnants from earlier development phases including:
- **8 unused configuration constants**
- **4 placeholder menu items with no implementation**
- **2 empty stub files**
- **1 complete disabled feature (Motion Test Mode)**
- **Multiple unused function declarations and methods**

**Estimated Impact:** Removing identified code will reduce binary size, improve maintainability, and clarify project structure without affecting functionality.

---

## 1. CRITICAL ISSUES (High Priority)

### 1.1 Duplicate Forward Declaration
**File:** [src/main.cpp](src/main.cpp#L105-L106)
**Lines:** 105-106
**Issue:** Identical function declaration appears twice

```cpp
void onEncoderEvent(EncoderEvent event, int32_t position);
void onEncoderEvent(EncoderEvent event, int32_t position);  // DELETE THIS LINE
```

**Action:** Delete line 106 (duplicate declaration)

---

### 1.2 Dead Code: Disabled Motion Test Feature
**File:** [src/main.cpp](src/main.cpp)
**Impact:** ~50 lines of unreachable code

#### Components to Remove:

**a) Variable Declaration (Line 81)**
```cpp
MenuItem motionTestMenu("Motion Test");  // DELETE
```

**b) Menu Comment (Line 295)**
```cpp
// mainMenu.addChild(&motionTestMenu);  // Disabled - DELETE ENTIRE LINE
```

**c) Menu Setup Code (Lines 316, 336)**
```cpp
motionTestMenu.setType(MenuItemType::ACTION);  // DELETE
motionTestMenu.setID(MenuItemID::MOTION_TEST_MENU);  // DELETE
```

**d) Mode Enum Case (Line 234)**
```cpp
case AppMode::MOTION_TEST:
    updateMotionTestMode();  // DELETE ENTIRE CASE
    break;
```

**e) Menu Selection Handler (Line 563)**
```cpp
case MenuItemID::MOTION_TEST_MENU:
    currentMode = AppMode::MOTION_TEST;  // DELETE ENTIRE CASE
    break;
```

**f) Implementation Function (Lines 740-758)**
```cpp
void updateMotionTestMode() {
    // ... entire function body ...
}
// DELETE ENTIRE FUNCTION
```

**g) AppMode Enum Value**
**File:** [src/main.cpp](src/main.cpp#L28)
**Line:** 28 (within enum)
```cpp
enum class AppMode {
    NORMAL,
    MENU,
    ANIMATION,
    MOTION_TEST,  // DELETE THIS LINE
    BRIGHTNESS_CONTROL
};
```

**h) MenuItemID Enum Value**
**File:** [src/main.cpp](src/main.cpp#L36)
**Line:** 36 (within enum)
```cpp
enum class MenuItemID : uint8_t {
    NONE = 0,
    MAIN_MENU,
    ANIMATION_MENU,
    MOTION_TEST_MENU,  // DELETE THIS LINE
    // ... rest ...
};
```

---

## 2. UNUSED FILES (Medium Priority)

### 2.1 Empty GestureDetector Stubs
**Files:**
- [lib/MotionSensor/GestureDetector.h](lib/MotionSensor/GestureDetector.h)
- [lib/MotionSensor/GestureDetector.cpp](lib/MotionSensor/GestureDetector.cpp)

**Status:** Both files are empty (0 bytes)
**Action:** Delete both files entirely

---

## 3. UNUSED MENU ITEMS (Medium Priority)

### 3.1 Placeholder Test Menu Items
**File:** [src/main.cpp](src/main.cpp)
**Issue:** Four test menu items with no functionality

#### Components to Remove:

**a) Variable Declarations (Lines 83-86)**
```cpp
MenuItem test1Menu("Test 1");  // DELETE
MenuItem test2Menu("Test 2");  // DELETE
MenuItem test3Menu("Test 3");  // DELETE
MenuItem test4Menu("Test 4");  // DELETE
```

**b) Menu Registration (Lines 297-300)**
```cpp
mainMenu.addChild(&test1Menu);  // DELETE
mainMenu.addChild(&test2Menu);  // DELETE
mainMenu.addChild(&test3Menu);  // DELETE
mainMenu.addChild(&test4Menu);  // DELETE
```

**c) Menu ID Assignment (Lines 338-341)**
```cpp
test1Menu.setID(MenuItemID::TEST1);  // DELETE
test2Menu.setID(MenuItemID::TEST2);  // DELETE
test3Menu.setID(MenuItemID::TEST3);  // DELETE
test4Menu.setID(MenuItemID::TEST4);  // DELETE
```

**d) MenuItemID Enum Values**
**File:** [src/main.cpp](src/main.cpp#L36)
**Lines:** Within MenuItemID enum
```cpp
enum class MenuItemID : uint8_t {
    // ... other values ...
    TEST1,  // DELETE
    TEST2,  // DELETE
    TEST3,  // DELETE
    TEST4   // DELETE
};
```

---

## 4. UNUSED CONFIGURATION CONSTANTS (Low Priority)

### 4.1 Hardware Pin Definitions
**File:** [include/config.h](include/config.h)

**Line 48:** Unused Interrupt Pin
```cpp
#define MOTION_INT_PIN      10    // DELETE - never referenced
```

**Line 71:** Unused Buzzer Pin
```cpp
#define BUZZER_PIN          5     // DELETE - no buzzer implementation
```

**Lines 76-77:** Unused LED Pins
```cpp
#define LED_STATUS_PIN      8     // DELETE - no LED control code
#define LED_ERROR_PIN       9     // DELETE - no LED control code
```

---

### 4.2 Unused Sensor Thresholds
**File:** [include/config.h](include/config.h)

**Line 58:** Tap Detection Threshold
```cpp
#define ACCEL_THRESHOLD_TAP     2.5f   // DELETE - tap detection removed
```

**Line 60:** Tilt Threshold
```cpp
#define GYRO_THRESHOLD_TILT     100.0f // DELETE - never used in MotionSensor
```

---

### 4.3 Unused Power Management Constants
**File:** [include/config.h](include/config.h)

**Lines 82-84:**
```cpp
#define BATTERY_ADC_PIN     ADC1_CHANNEL_0  // DELETE - no battery monitoring
#define SLEEP_TIMEOUT_MS    300000          // DELETE - no sleep implementation
#define DEEP_SLEEP_TIMEOUT_MS 900000        // DELETE - no sleep implementation
```

---

## 5. UNUSED CLASS METHODS (Low Priority)

### 5.1 MotionSensor Private Method
**File:** [lib/MotionSensor/MotionSensor.h](lib/MotionSensor/MotionSensor.h#L211)
**Line:** 211

```cpp
private:
    void detectTap();  // DELETE - never implemented or called
```

**Action:** Remove method declaration entirely

---

### 5.2 DisplayManager Animation Methods
**File:** [lib/DisplayManager/DisplayManager.cpp](lib/DisplayManager/DisplayManager.cpp)
**Lines:** 184-216

These methods are declared but never called (AnimationEngine handles all animation):

```cpp
void DisplayManager::playAnimationFrame(/* ... */) { /* ... */ }  // DELETE
void DisplayManager::updateAnimation() { /* ... */ }              // DELETE
void DisplayManager::setAnimationFPS(uint8_t fps) { /* ... */ }   // DELETE
```

**Also remove from header:**
**File:** [lib/DisplayManager/DisplayManager.h](lib/DisplayManager/DisplayManager.h)

```cpp
void playAnimationFrame(const uint8_t* frame, uint8_t width, uint8_t height);  // DELETE
void updateAnimation();                                                         // DELETE
void setAnimationFPS(uint8_t fps);                                             // DELETE
```

---

## 6. UNUSED BITMAP RESOURCES (Low Priority)

### 6.1 Tap Bitmap
**File:** [lib/DisplayManager/DisplayManager.h](lib/DisplayManager/DisplayManager.h#L27)
**Line:** 27

```cpp
extern const unsigned char PROGMEM bitmap_tap[];  // DELETE - never displayed
```

**Note:** The bitmap is defined in `bitmaps.h:38` but never referenced. Consider removing from both files if PROGMEM space is constrained.

---

## 7. DEAD CODE PATHS (Documentation Only)

### 7.1 Touch Feature (Disabled via Config)
**File:** [src/main.cpp](src/main.cpp)
**Lines:** 172, 208, 420
**Status:** Controlled by `TOUCH_ENABLED` flag (currently `false` in config.h)

```cpp
#if TOUCH_ENABLED
    touch.begin();
    touch.setCallback(onTouchEvent);
#else
    touch.setEnabled(false);  // Currently executed
#endif
```

**Action:** No immediate action required - this is proper conditional compilation. If touch will never be used, consider removing all touch-related code.

---

## 8. POTENTIALLY INCOMPLETE FEATURES

### 8.1 SURPRISED Animation State
**File:** [lib/AnimationEngine/AnimationEngine.cpp](lib/AnimationEngine/AnimationEngine.cpp#L44)
**Line:** 44

```cpp
registerAnimation(AnimState::SURPRISED, &anim_surprised);
```

**Issue:** Animation file included but may have minimal/incomplete data.
**Action:** Verify `animations/anim_surprised.h` has proper animation data or remove registration.

---

## Cleanup Checklist

Use this checklist to track cleanup progress:

### High Priority
- [ ] Remove duplicate `onEncoderEvent` declaration ([main.cpp:106](src/main.cpp#L106))
- [ ] Delete Motion Test feature (8 locations in [main.cpp](src/main.cpp))
  - [ ] Variable (line 81)
  - [ ] Comment (line 295)
  - [ ] Menu setup (lines 316, 336)
  - [ ] Loop case (line 234)
  - [ ] Selection case (line 563)
  - [ ] Function (lines 740-758)
  - [ ] AppMode enum (line 28)
  - [ ] MenuItemID enum (line 36)

### Medium Priority
- [ ] Delete empty GestureDetector files
  - [ ] [GestureDetector.h](lib/MotionSensor/GestureDetector.h)
  - [ ] [GestureDetector.cpp](lib/MotionSensor/GestureDetector.cpp)
- [ ] Remove placeholder test menu items (12 locations in [main.cpp](src/main.cpp))
  - [ ] Variables (lines 83-86)
  - [ ] addChild calls (lines 297-300)
  - [ ] setID calls (lines 338-341)
  - [ ] MenuItemID enum values (line 36)

### Low Priority
- [ ] Remove unused config constants from [config.h](include/config.h)
  - [ ] MOTION_INT_PIN (line 48)
  - [ ] ACCEL_THRESHOLD_TAP (line 58)
  - [ ] GYRO_THRESHOLD_TILT (line 60)
  - [ ] BUZZER_PIN (line 71)
  - [ ] LED_STATUS_PIN (line 76)
  - [ ] LED_ERROR_PIN (line 77)
  - [ ] BATTERY_ADC_PIN (line 82)
  - [ ] SLEEP_TIMEOUT_MS (line 83)
  - [ ] DEEP_SLEEP_TIMEOUT_MS (line 84)
- [ ] Remove unused methods
  - [ ] detectTap() from [MotionSensor.h](lib/MotionSensor/MotionSensor.h#L211)
  - [ ] Animation methods from [DisplayManager.h](lib/DisplayManager/DisplayManager.h) & [DisplayManager.cpp](lib/DisplayManager/DisplayManager.cpp)
- [ ] Remove bitmap_tap extern from [DisplayManager.h:27](lib/DisplayManager/DisplayManager.h#L27)

### Optional
- [ ] Verify anim_surprised animation data completeness
- [ ] Consider removing touch-related code if feature won't be implemented

---

## Estimated Impact

| Metric | Before | After | Savings |
|--------|--------|-------|---------|
| Lines of Code (approx) | ~3,500 | ~3,380 | ~120 lines |
| Unused constants | 8 | 0 | -8 |
| Empty files | 2 | 0 | -2 |
| Dead functions | 3+ | 0 | -3+ |
| Unused menu items | 4 | 0 | -4 |

**Build Impact:** Minor reduction in binary size from removed constants and dead code elimination.
**Maintenance Impact:** Significant improvement in code clarity and reduced cognitive load.

---

## Notes

- All file paths use absolute Windows paths relative to project root
- Line numbers are accurate as of analysis date
- Recommend testing thoroughly after cleanup
- Consider creating a git branch for cleanup: `git checkout -b cleanup/remove-unused-code`
- This is a conservative analysis - there may be additional optimizations possible

---

## Next Steps

1. **Backup:** Create git branch for cleanup work
2. **Execute:** Work through checklist in priority order
3. **Test:** Build and verify functionality after each section
4. **Commit:** Commit changes in logical groups
5. **Verify:** Run full system test on hardware

---

**End of Report**
