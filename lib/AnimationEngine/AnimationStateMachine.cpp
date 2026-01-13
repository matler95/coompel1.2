/**
 * @file AnimationStateMachine.cpp
 * @brief Implementation of AnimationStateMachine
 */

#include "AnimationStateMachine.h"

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

AnimationStateMachine::AnimationStateMachine(AnimationEngine* animator)
    : _animator(animator),
      _behaviorState(BehaviorState::IDLE_BASE),
      _lastBlinkTime(0),
      _nextBlinkDelay(0),
      _blinkMinInterval(3),     // 3 seconds minimum
      _blinkMaxInterval(8),     // 8 seconds maximum
      _lastRandomActionTime(0),
      _nextRandomActionDelay(0),
      _randomActionMinInterval(15),  // 15 seconds minimum
      _randomActionMaxInterval(45),  // 45 seconds maximum
      _randomActionChance(30),       // 30% chance when interval hits
      _currentReaction(AnimState::IDLE),
      _reactionLooping(false),
      _isShaking(false),
      _autoBlinkEnabled(true),
      _randomActionsEnabled(true)
{
}

void AnimationStateMachine::init() {
    Serial.println("[STATE] Initializing animation state machine...");
    
    // Start with base frame (idle frame 0)
    _animator->play(AnimState::IDLE);
    _animator->setPaused(true);  // Pause on first frame
    _behaviorState = BehaviorState::IDLE_BASE;
    
    // Schedule first blink
    scheduleNextBlink();
    scheduleNextRandomAction();
    
    Serial.println("[STATE] State machine ready - showing base frame");
}

// ============================================================================
// UPDATE METHOD
// ============================================================================

void AnimationStateMachine::update() {
    switch (_behaviorState) {
        case BehaviorState::IDLE_BASE:
            updateIdleBase();
            break;
            
        case BehaviorState::BLINKING:
            updateBlinking();
            break;
            
        case BehaviorState::RANDOM_ACTION:
            updateRandomAction();
            break;
            
        case BehaviorState::REACTING:
            updateReacting();
            break;
            
        case BehaviorState::TRANSITIONING:
            returnToBase();
            break;
    }
}

// ============================================================================
// STATE UPDATE METHODS
// ============================================================================

void AnimationStateMachine::updateIdleBase() {
    unsigned long currentTime = millis();
    
    // Check for scheduled blink
    if (_autoBlinkEnabled && (currentTime - _lastBlinkTime >= _nextBlinkDelay * 1000UL)) {
        Serial.println("[STATE] Natural blink triggered");
        _animator->play(AnimState::IDLE);
        _animator->setPaused(false);
        _behaviorState = BehaviorState::BLINKING;
        _lastBlinkTime = currentTime;
        scheduleNextBlink();
        return;
    }
    
    // Check for random action
    if (_randomActionsEnabled && 
        (currentTime - _lastRandomActionTime >= _nextRandomActionDelay * 1000UL)) {
        
        // Random chance to trigger
        if (random(100) < _randomActionChance) {
            Serial.println("[STATE] Random wink triggered!");
            _animator->play(AnimState::WINK);  // Using HAPPY for wink, change if you have separate wink state
            _behaviorState = BehaviorState::RANDOM_ACTION;
        }
        
        _lastRandomActionTime = currentTime;
        scheduleNextRandomAction();
    }
}

void AnimationStateMachine::updateBlinking() {
    // Check if blink animation finished
    if (!_animator->isPlaying()) {
        Serial.println("[STATE] Blink complete, returning to base");
        returnToBase();
    }
}

void AnimationStateMachine::updateRandomAction() {
    // Check if random action finished
    if (!_animator->isPlaying()) {
        Serial.println("[STATE] Random action complete, returning to base");
        returnToBase();
    }
}

void AnimationStateMachine::updateReacting() {
    // If we stopped the loop but animation is still finishing
    if (!_reactionLooping && !_animator->isPlaying()) {
        Serial.println("[STATE] Reaction finished, returning to base");
        returnToBase();
    }
    
    // If looping, animation continues until stopReaction() is called
}


// ============================================================================
// REACTION CONTROL
// ============================================================================

void AnimationStateMachine::triggerReaction(AnimState state, bool loop) {
    // Don't restart if already playing the same looping reaction
    if (_behaviorState == BehaviorState::REACTING && 
        _currentReaction == state && 
        _reactionLooping &&
        loop) {
        // Already playing this looping animation, don't restart
        Serial.println("[STATE] Already reacting with this animation (no restart)");
        return;
    }
    
    Serial.printf("[STATE] Triggering reaction: %d (loop: %s)\n", 
                  (int)state, loop ? "yes" : "no");
    
    _currentReaction = state;
    _reactionLooping = loop;
    _behaviorState = BehaviorState::REACTING;
    
    // Use forceLoop parameter for animations that should loop during reaction
    _animator->play(state, true, loop);  // ⭐ Pass loop to animator
}

void AnimationStateMachine::stopReaction() {
    if (_behaviorState == BehaviorState::REACTING && _reactionLooping) {
        Serial.println("[STATE] Stopping reaction loop");
        _reactionLooping = false;
        _animator->stopForcedLoop();  // Stop the loop
        // Animation will finish current cycle, then updateReacting() will return to base
    }
}


// ============================================================================
// STATE TRANSITIONS
// ============================================================================

void AnimationStateMachine::returnToBase() {
    Serial.println("[STATE] Returning to base frame");
    _animator->play(AnimState::IDLE);
    _animator->setPaused(true);  // Pause on frame 0
    _behaviorState = BehaviorState::IDLE_BASE;
}

// ============================================================================
// SCHEDULING
// ============================================================================

void AnimationStateMachine::scheduleNextBlink() {
    _nextBlinkDelay = random(_blinkMinInterval, _blinkMaxInterval + 1);
    Serial.printf("[STATE] Next blink scheduled in %d seconds\n", _nextBlinkDelay);
}

void AnimationStateMachine::scheduleNextRandomAction() {
    _nextRandomActionDelay = random(_randomActionMinInterval, _randomActionMaxInterval + 1);
    Serial.printf("[STATE] Next random action check in %d seconds\n", _nextRandomActionDelay);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void AnimationStateMachine::setBlinkInterval(uint16_t minSeconds, uint16_t maxSeconds) {
    _blinkMinInterval = minSeconds;
    _blinkMaxInterval = maxSeconds;
}

void AnimationStateMachine::setRandomActionInterval(uint16_t minSeconds, uint16_t maxSeconds) {
    _randomActionMinInterval = minSeconds;
    _randomActionMaxInterval = maxSeconds;
}

void AnimationStateMachine::setRandomActionChance(uint8_t percent) {
    _randomActionChance = constrain(percent, 0, 100);
}

void AnimationStateMachine::enableAutoBlink(bool enabled) {
    _autoBlinkEnabled = enabled;
}

void AnimationStateMachine::enableRandomActions(bool enabled) {
    _randomActionsEnabled = enabled;
}