/**
 * @file AnimationEngine.cpp
 * @brief Implementation of AnimationEngine class
 */

#include "AnimationEngine.h"

// Include animation data
#include "animations/anim_idle.h"
#include "animations/anim_wink.h"
#include "animations/anim_surprised.h"
#include "animations/anim_dizzy.h"

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

AnimationEngine::AnimationEngine(DisplayManager* display)
    : _display(display),
      _currentState(AnimState::IDLE),
      _previousState(AnimState::IDLE),
      _currentAnimation(nullptr),
      _currentFrame(0),
      _lastFrameTime(0),
      _globalFPS(0),  // 0 = use animation's own FPS
      _playing(false),
      _paused(false),
      _autoReturnToIdle(true)
{
    // Initialize animation registry
    for (uint8_t i = 0; i < 8; i++) {
        _animations[i] = nullptr;
    }
}

void AnimationEngine::init() {
    Serial.println("[ANIM] Initializing animation engine...");
    
    // Register built-in animations
    registerAnimation(AnimState::IDLE, &anim_idle);
    registerAnimation(AnimState::WINK, &anim_wink);
    registerAnimation(AnimState::SURPRISED, &anim_surprised);
    registerAnimation(AnimState::DIZZY, &anim_dizzy);
    
    // Start with idle animation
    play(AnimState::IDLE);
    
    Serial.println("[ANIM] Animation engine ready");
}

// ============================================================================
// ANIMATION PLAYBACK
// ============================================================================

void AnimationEngine::play(AnimState state, bool priority) {
    // Check if already playing this animation
    if (_currentState == state && _playing && !priority) {
        return;
    }
    
    // Don't interrupt high-priority animations unless forced
    if (_playing && !_currentAnimation->loop && !priority) {
        return;
    }
    
    const Animation* anim = getAnimation(state);
    if (anim == nullptr) {
        Serial.printf("[ANIM] ERROR: Animation %d not found\n", (int)state);
        return;
    }
    
    _previousState = _currentState;
    _currentState = state;
    _currentAnimation = anim;
    _currentFrame = 0;
    _lastFrameTime = millis();
    _playing = true;
    _paused = false;
    
    Serial.printf("[ANIM] Playing: %s (%d frames @ %d FPS)\n", 
                  anim->name, anim->frameCount, anim->fps);
}

void AnimationEngine::stop() {
    _playing = false;
    _currentFrame = 0;
}

void AnimationEngine::setPaused(bool paused) {
    _paused = paused;
    if (!paused) {
        _lastFrameTime = millis();  // Reset timing when resuming
    }
}

// ============================================================================
// UPDATE & DRAWING
// ============================================================================

void AnimationEngine::update() {
    if (!_playing || _paused || _currentAnimation == nullptr) {
        return;
    }
    
    unsigned long currentTime = millis();
    uint16_t frameDelay = getFrameDelay();
    
    if (currentTime - _lastFrameTime >= frameDelay) {
        _lastFrameTime = currentTime;
        advanceFrame();
    }
}

void AnimationEngine::draw() {
    if (_currentAnimation == nullptr) return;
    
    // Get current frame bitmap
    const uint8_t* frameBitmap = _currentAnimation->frames[_currentFrame];
    
    // Draw centered on display
    _display->drawBitmapCentered(
        frameBitmap,
        _currentAnimation->width,
        _currentAnimation->height
    );
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void AnimationEngine::advanceFrame() {
    if (_currentAnimation == nullptr) return;
    
    _currentFrame++;
    
    // Check if animation complete
    if (_currentFrame >= _currentAnimation->frameCount) {
        if (_currentAnimation->loop) {
            // Loop back to start
            _currentFrame = 0;
        } else {
            // Animation finished
            _currentFrame = _currentAnimation->frameCount - 1;  // Hold last frame
            onAnimationComplete();
        }
    }
}

void AnimationEngine::onAnimationComplete() {
    Serial.printf("[ANIM] Animation complete: %s\n", _currentAnimation->name);
    
    if (_autoReturnToIdle && _currentState != AnimState::IDLE) {
        // Return to idle after non-looping animation
        play(AnimState::IDLE);
    } else {
        _playing = false;
    }
}

uint16_t AnimationEngine::getFrameDelay() {
    // Check if animation has per-frame delays
    if (_currentAnimation->frameDelays != nullptr) {
        // Use specific delay for current frame
        float delaySeconds = pgm_read_float(&_currentAnimation->frameDelays[_currentFrame]);
        return (uint16_t)(delaySeconds * 1000.0f);  // Convert to milliseconds
    }
    
    // Fall back to FPS-based timing
    uint8_t fps = _globalFPS > 0 ? _globalFPS : _currentAnimation->fps;
    return 1000 / fps;
}

const Animation* AnimationEngine::getAnimation(AnimState state) {
    uint8_t index = (uint8_t)state;
    if (index < 8) {
        return _animations[index];
    }
    return nullptr;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void AnimationEngine::registerAnimation(AnimState state, const Animation* animation) {
    uint8_t index = (uint8_t)state;
    if (index < 8) {
        _animations[index] = animation;
        Serial.printf("[ANIM] Registered: %s\n", animation->name);
    }
}

void AnimationEngine::setGlobalFPS(uint8_t fps) {
    _globalFPS = constrain(fps, 1, 30);
}

void AnimationEngine::setAutoReturnToIdle(bool enabled) {
    _autoReturnToIdle = enabled;
}