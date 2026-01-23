/**
 * @file PongGame.h
 * @brief Pong game for rotary encoder control
 */

#ifndef PONG_GAME_H
#define PONG_GAME_H

#include <Arduino.h>
#include "DisplayManager.h"

// Game constants
namespace PongConfig {
    // Display
    constexpr int16_t SCREEN_WIDTH = 128;
    constexpr int16_t SCREEN_HEIGHT = 64;

    // Paddle
    constexpr int16_t PADDLE_WIDTH = 3;
    constexpr int16_t PADDLE_HEIGHT = 12;
    constexpr int16_t PADDLE_MARGIN = 4;
    constexpr int16_t PADDLE_SPEED = 3;

    // Ball
    constexpr int16_t BALL_SIZE = 3;
    constexpr float BALL_SPEED_INITIAL = 1.5f;
    constexpr float BALL_SPEED_MAX = 3.5f;
    constexpr float BALL_SPEED_INCREMENT = 0.1f;

    // Game
    constexpr uint8_t WINNING_SCORE = 5;
    constexpr uint32_t UPDATE_INTERVAL_MS = 33; // ~30 FPS

    // AI
    constexpr float AI_REACTION_RATE = 0.7f;
    constexpr int16_t AI_ERROR_MARGIN = 4;
}

enum class PongState {
    READY,
    PLAYING,
    PAUSED,
    GAME_OVER
};

class PongGame {
public:
    PongGame();

    void reset();
    void update();
    void render(DisplayManager& display);

    void setPlayerInput(int8_t direction);
    void togglePause();
    void startGame();

    PongState getState() const { return _state; }
    uint8_t getPlayerScore() const { return _playerScore; }
    uint8_t getAIScore() const { return _aiScore; }
    bool isGameOver() const { return _state == PongState::GAME_OVER; }

private:
    PongState _state;

    // Ball
    float _ballX, _ballY;
    float _ballVelX, _ballVelY;
    float _ballSpeed;

    // Paddles
    int16_t _playerPaddleY;
    int16_t _aiPaddleY;

    // Score
    uint8_t _playerScore;
    uint8_t _aiScore;

    // Input
    int8_t _playerInput;

    // Timing
    uint32_t _lastUpdateMs;

    // Physics
    void updateBall();
    void updatePlayerPaddle();
    void updateAIPaddle();

    // Collision
    bool checkPlayerPaddleCollision();
    bool checkAIPaddleCollision();
    void handlePaddleHit(int16_t paddleY, bool isPlayer);

    // Ball
    void serveBall(bool towardsPlayer);
    void resetBall();

    // AI
    int16_t predictBallY();

    // Rendering
    void drawField(Adafruit_SH1106G* d);
    void drawPaddles(Adafruit_SH1106G* d);
    void drawBall(Adafruit_SH1106G* d);
    void drawScore(Adafruit_SH1106G* d);
    void drawReadyScreen(DisplayManager& display);
    void drawPausedOverlay(DisplayManager& display);
    void drawGameOverScreen(DisplayManager& display);
};

#endif
