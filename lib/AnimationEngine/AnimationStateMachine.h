/**
 * @file AnimationStateMachine.h
 * @brief Intelligent animation state management with natural behaviors
 * @version 1.0.0
 */

#ifndef ANIMATION_STATE_MACHINE_H
#define ANIMATION_STATE_MACHINE_H

#include <Arduino.h>
#include "AnimationEngine.h"

// ============================================================================
// BEHAVIOR STATE
// ============================================================================
enum class BehaviorState {
    IDLE_BASE,          // Showing base frame (idle frame 0)
    BLINKING,           // Playing blink animation
    RANDOM_ACTION,      // Playing random action (wink, etc)
    REACTING,           // Reacting to input (dizzy, surprised, etc)
    TRANSITIONING       // Returning to base
};

// ============================================================================
// ANIMATION STATE MACHINE CLASS
// ============================================================================
class AnimationStateMachine {
public:
    /**
     * @brief Constructor
     * @param animator Pointer to AnimationEngine
     */
    AnimationStateMachine(AnimationEngine* animator);
    
    /**
     * @brief Initialize state machine
     */
    void init();
    
    /**
     * @brief Update state machine (call in loop)
     */
    void update();
    
    /**
     * @brief Trigger a reaction animation (interrupts current state)
     * @param state Animation state to play
     * @param loop Whether to loop the animation
     */
    void triggerReaction(AnimState state, bool loop = false);
    
    /**
     * @brief Stop current reaction and return to base
     */
    void stopReaction();
    
    /**
     * @brief Check if currently reacting (for input blocking)
     * @return true if in reaction state
     */
    bool isReacting() const { return _behaviorState == BehaviorState::REACTING; }
    
    /**
     * @brief Set timing parameters
     */
    void setBlinkInterval(uint16_t minSeconds, uint16_t maxSeconds);
    void setRandomActionInterval(uint16_t minSeconds, uint16_t maxSeconds);
    void setRandomActionChance(uint8_t percent);
    
    /**
     * @brief Enable/disable autonomous behaviors
     */
    void enableAutoBlink(bool enabled);
    void enableRandomActions(bool enabled);
    
    /**
     * @brief Get current behavior state
     */
    BehaviorState getBehaviorState() const { return _behaviorState; }

    
private:
    AnimationEngine* _animator;
    BehaviorState _behaviorState;
    
    // Timing
    unsigned long _lastBlinkTime;
    unsigned long _nextBlinkDelay;
    uint16_t _blinkMinInterval;
    uint16_t _blinkMaxInterval;
    
    unsigned long _lastRandomActionTime;
    unsigned long _nextRandomActionDelay;
    uint16_t _randomActionMinInterval;
    uint16_t _randomActionMaxInterval;
    uint8_t _randomActionChance;
    
    // Reaction tracking
    AnimState _currentReaction;
    bool _reactionLooping;
    bool _isShaking;
    
    // Autonomous behavior flags
    bool _autoBlinkEnabled;
    bool _randomActionsEnabled;
    
    // Private methods
    void updateIdleBase();
    void updateBlinking();
    void updateRandomAction();
    void updateReacting();
    void scheduleNextBlink();
    void scheduleNextRandomAction();
    void returnToBase();
};

#endif // ANIMATION_STATE_MACHINE_H