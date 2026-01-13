/**
 * @file AnimationEngine.h
 * @brief Multi-frame bitmap animation system
 * @version 1.0.0
 * 
 * Features:
 * - Frame-based animation playback
 * - Multiple animation states
 * - Configurable FPS (1-30)
 * - Event-triggered transitions
 * - Memory-efficient PROGMEM storage
 */

#ifndef ANIMATION_ENGINE_H
#define ANIMATION_ENGINE_H

#include <Arduino.h>
#include "DisplayManager.h"

// ============================================================================
// ANIMATION STATES
// ============================================================================
enum class AnimState {
    IDLE,           // Default resting state
    WINK,           // positive emotion
    SURPRISED,      // Sudden reaction (tap)
    DIZZY,          // Confused/shaken
    SLEEPING,       // Low power mode
    THINKING,       // Processing
    SAD,            // Negative emotion
    CUSTOM          // User-defined animation
};

// ============================================================================
// ANIMATION STRUCTURE
// ============================================================================
struct Animation {
    const uint8_t* const* frames;   // Array of bitmap frame pointers
    uint8_t frameCount;             // Number of frames in animation
    uint8_t width;                  // Frame width in pixels
    uint8_t height;                 // Frame height in pixels
    uint8_t fps;                    // Playback speed (frames per second) - FALLBACK
    bool loop;                      // True if animation loops
    const char* name;               // Animation name for debugging
    const float* frameDelays;       // Per-frame delays in seconds (NULL = use fps) ⭐ NEW
};

// ============================================================================
// ANIMATION ENGINE CLASS
// ============================================================================
class AnimationEngine {
public:
    /**
     * @brief Constructor
     * @param display Pointer to DisplayManager
     */
    AnimationEngine(DisplayManager* display);
    
    /**
     * @brief Initialize animation engine
     */
    void init();
    
    /**
     * @brief Update animation (call in loop)
     * Must be called frequently for smooth playback
     */
    void update();
    
    /**
     * @brief Play specific animation state
     * @param state AnimState to play
     * @param priority If true, interrupt current animation
     */
    void play(AnimState state, bool priority = false);
    
    /**
     * @brief Stop current animation
     */
    void stop();
    
    /**
     * @brief Pause/resume animation
     * @param paused True to pause
     */
    void setPaused(bool paused);
    
    /**
     * @brief Check if animation is playing
     * @return True if actively animating
     */
    bool isPlaying() const { return _playing; }
    
    /**
     * @brief Get current animation state
     * @return Current AnimState
     */
    AnimState getCurrentState() const { return _currentState; }
    
    /**
     * @brief Get current frame number
     * @return Frame index
     */
    uint8_t getCurrentFrame() const { return _currentFrame; }
    
    /**
     * @brief Set global FPS (overrides individual animation FPS)
     * @param fps Frames per second (1-30)
     */
    void setGlobalFPS(uint8_t fps);
    
    /**
     * @brief Enable/disable auto-return to idle
     * @param enabled If true, non-looping animations return to idle
     */
    void setAutoReturnToIdle(bool enabled);
    
    /**
     * @brief Draw current frame to display
     */
    void draw();
    
    /**
     * @brief Register custom animation
     * @param state AnimState slot (use CUSTOM)
     * @param animation Pointer to Animation structure
     */
    void registerAnimation(AnimState state, const Animation* animation);
    
    /**
     * @brief Get current animation info
     * @return Pointer to current Animation structure
     */
    const Animation* getCurrentAnimation() const { return _currentAnimation; }

private:
    DisplayManager* _display;
    
    // Current animation state
    AnimState _currentState;
    AnimState _previousState;
    const Animation* _currentAnimation;
    
    // Playback state
    uint8_t _currentFrame;
    unsigned long _lastFrameTime;
    uint8_t _globalFPS;
    bool _playing;
    bool _paused;
    bool _autoReturnToIdle;
    
    // Animation registry
    const Animation* _animations[8];  // One slot per AnimState
    
    // Private methods
    const Animation* getAnimation(AnimState state);
    void advanceFrame();
    void onAnimationComplete();
    uint16_t getFrameDelay();
};

#endif // ANIMATION_ENGINE_H