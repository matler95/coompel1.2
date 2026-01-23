/**
 * @file PongGame.cpp
 * @brief Pong game implementation
 */

#include "PongGame.h"
#include <math.h>

using namespace PongConfig;

PongGame::PongGame()
    : _state(PongState::READY)
    , _ballX(0), _ballY(0)
    , _ballVelX(0), _ballVelY(0)
    , _ballSpeed(BALL_SPEED_INITIAL)
    , _playerPaddleY(0), _aiPaddleY(0)
    , _playerScore(0), _aiScore(0)
    , _playerInput(0)
    , _lastUpdateMs(0)
{
    reset();
}

void PongGame::reset() {
    _state = PongState::READY;
    _playerScore = 0;
    _aiScore = 0;
    _ballSpeed = BALL_SPEED_INITIAL;

    // Center paddles
    _playerPaddleY = (SCREEN_HEIGHT - PADDLE_HEIGHT) / 2;
    _aiPaddleY = (SCREEN_HEIGHT - PADDLE_HEIGHT) / 2;

    resetBall();
    _playerInput = 0;
    _lastUpdateMs = millis();
}

void PongGame::resetBall() {
    _ballX = (SCREEN_WIDTH - BALL_SIZE) / 2.0f;
    _ballY = (SCREEN_HEIGHT - BALL_SIZE) / 2.0f;
    _ballVelX = 0;
    _ballVelY = 0;
}

void PongGame::serveBall(bool towardsPlayer) {
    _ballSpeed = BALL_SPEED_INITIAL;

    // Random vertical angle between -0.5 and 0.5
    float angle = (random(100) - 50) / 100.0f * 0.5f;

    _ballVelX = towardsPlayer ? -_ballSpeed : _ballSpeed;
    _ballVelY = _ballSpeed * angle;
}

void PongGame::startGame() {
    if (_state == PongState::READY || _state == PongState::GAME_OVER) {
        reset();
        _state = PongState::PLAYING;
        serveBall(random(2) == 0);
    }
}

void PongGame::togglePause() {
    if (_state == PongState::PLAYING) {
        _state = PongState::PAUSED;
    } else if (_state == PongState::PAUSED) {
        _state = PongState::PLAYING;
        _lastUpdateMs = millis();
    }
}

void PongGame::setPlayerInput(int8_t direction) {
    // Move paddle immediately on encoder input (works in any state)
    if (direction != 0) {
        _playerPaddleY += direction * PADDLE_SPEED;
        _playerPaddleY = constrain(_playerPaddleY, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
    }
}

void PongGame::update() {
    if (_state != PongState::PLAYING) return;

    uint32_t now = millis();
    if (now - _lastUpdateMs < UPDATE_INTERVAL_MS) return;
    _lastUpdateMs = now;

    updatePlayerPaddle();
    updateAIPaddle();
    updateBall();

    // Check win condition
    if (_playerScore >= WINNING_SCORE || _aiScore >= WINNING_SCORE) {
        _state = PongState::GAME_OVER;
    }
}

void PongGame::updatePlayerPaddle() {
    // Paddle movement now handled directly in setPlayerInput()
}

void PongGame::updateAIPaddle() {
    // Only react when ball is moving towards AI
    if (_ballVelX <= 0) return;

    // Reaction rate check
    if (random(100) >= (int)(AI_REACTION_RATE * 100)) return;

    int16_t targetY = predictBallY();
    targetY += random(-AI_ERROR_MARGIN, AI_ERROR_MARGIN + 1);

    int16_t paddleCenter = _aiPaddleY + PADDLE_HEIGHT / 2;

    if (targetY < paddleCenter - 2) {
        _aiPaddleY -= PADDLE_SPEED;
    } else if (targetY > paddleCenter + 2) {
        _aiPaddleY += PADDLE_SPEED;
    }

    _aiPaddleY = constrain(_aiPaddleY, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
}

int16_t PongGame::predictBallY() {
    if (_ballVelX <= 0) return SCREEN_HEIGHT / 2;

    float aiPaddleX = SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH;
    float timeToReach = (aiPaddleX - _ballX) / _ballVelX;
    float predictedY = _ballY + _ballVelY * timeToReach;

    // Account for bounces
    int bounces = 0;
    while ((predictedY < 0 || predictedY > SCREEN_HEIGHT - BALL_SIZE) && bounces < 10) {
        if (predictedY < 0) {
            predictedY = -predictedY;
        }
        if (predictedY > SCREEN_HEIGHT - BALL_SIZE) {
            predictedY = 2 * (SCREEN_HEIGHT - BALL_SIZE) - predictedY;
        }
        bounces++;
    }

    return (int16_t)(predictedY + BALL_SIZE / 2);
}

void PongGame::updateBall() {
    // Move ball
    _ballX += _ballVelX;
    _ballY += _ballVelY;

    // Top/bottom wall bounce
    if (_ballY <= 0) {
        _ballY = 0;
        _ballVelY = -_ballVelY;
    }
    if (_ballY >= SCREEN_HEIGHT - BALL_SIZE) {
        _ballY = SCREEN_HEIGHT - BALL_SIZE;
        _ballVelY = -_ballVelY;
    }

    // Check paddle collisions
    if (_ballVelX < 0 && checkPlayerPaddleCollision()) {
        handlePaddleHit(_playerPaddleY, true);
    }
    if (_ballVelX > 0 && checkAIPaddleCollision()) {
        handlePaddleHit(_aiPaddleY, false);
    }

    // Check scoring
    if (_ballX < -BALL_SIZE) {
        _aiScore++;
        resetBall();
        if (_aiScore < WINNING_SCORE) {
            serveBall(true);
        }
    }
    if (_ballX > SCREEN_WIDTH) {
        _playerScore++;
        resetBall();
        if (_playerScore < WINNING_SCORE) {
            serveBall(false);
        }
    }
}

bool PongGame::checkPlayerPaddleCollision() {
    float paddleX = PADDLE_MARGIN;
    float paddleRight = paddleX + PADDLE_WIDTH;

    if (_ballX <= paddleRight && _ballX + BALL_SIZE >= paddleX) {
        if (_ballY + BALL_SIZE >= _playerPaddleY && _ballY <= _playerPaddleY + PADDLE_HEIGHT) {
            return true;
        }
    }
    return false;
}

bool PongGame::checkAIPaddleCollision() {
    float paddleX = SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH;
    float paddleRight = paddleX + PADDLE_WIDTH;

    if (_ballX + BALL_SIZE >= paddleX && _ballX <= paddleRight) {
        if (_ballY + BALL_SIZE >= _aiPaddleY && _ballY <= _aiPaddleY + PADDLE_HEIGHT) {
            return true;
        }
    }
    return false;
}

void PongGame::handlePaddleHit(int16_t paddleY, bool isPlayer) {
    // Calculate hit position on paddle (-1 to +1)
    float hitPos = (_ballY + BALL_SIZE / 2.0f - paddleY) / PADDLE_HEIGHT;
    hitPos = constrain(hitPos * 2.0f - 1.0f, -0.8f, 0.8f);

    // Speed up slightly
    _ballSpeed = min(_ballSpeed + BALL_SPEED_INCREMENT, BALL_SPEED_MAX);

    // Reflect and adjust angle
    _ballVelX = isPlayer ? _ballSpeed : -_ballSpeed;
    _ballVelY = _ballSpeed * hitPos * 0.6f;

    // Push ball out of paddle
    if (isPlayer) {
        _ballX = PADDLE_MARGIN + PADDLE_WIDTH + 1;
    } else {
        _ballX = SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH - BALL_SIZE - 1;
    }
}

void PongGame::render(DisplayManager& display) {
    display.clear();

    if (_state == PongState::READY) {
        drawReadyScreen(display);
    } else {
        Adafruit_SH1106G* d = display.getRawDisplay();
        drawField(d);
        drawPaddles(d);
        drawBall(d);
        drawScore(d);

        if (_state == PongState::PAUSED) {
            drawPausedOverlay(display);
        } else if (_state == PongState::GAME_OVER) {
            drawGameOverScreen(display);
        }
    }

    display.update();
}

void PongGame::drawField(Adafruit_SH1106G* d) {
    // Dashed center line
    for (int16_t y = 0; y < SCREEN_HEIGHT; y += 8) {
        d->drawFastVLine(SCREEN_WIDTH / 2, y, 4, SH110X_WHITE);
    }
}

void PongGame::drawPaddles(Adafruit_SH1106G* d) {
    // Player paddle (left)
    d->fillRect(PADDLE_MARGIN, _playerPaddleY, PADDLE_WIDTH, PADDLE_HEIGHT, SH110X_WHITE);

    // AI paddle (right)
    d->fillRect(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH, _aiPaddleY,
                PADDLE_WIDTH, PADDLE_HEIGHT, SH110X_WHITE);
}

void PongGame::drawBall(Adafruit_SH1106G* d) {
    d->fillRect((int16_t)_ballX, (int16_t)_ballY, BALL_SIZE, BALL_SIZE, SH110X_WHITE);
}

void PongGame::drawScore(Adafruit_SH1106G* d) {
    d->setTextSize(1);
    d->setTextColor(SH110X_WHITE);

    // Player score (left of center)
    d->setCursor(SCREEN_WIDTH / 2 - 16, 2);
    d->print(_playerScore);

    // AI score (right of center)
    d->setCursor(SCREEN_WIDTH / 2 + 10, 2);
    d->print(_aiScore);
}

void PongGame::drawReadyScreen(DisplayManager& display) {
    display.showTextCentered("PONG", 10, 2);
    display.showTextCentered("Press to Start", 35, 1);
    display.showTextCentered("Long press: Exit", 50, 1);
}

void PongGame::drawPausedOverlay(DisplayManager& display) {
    Adafruit_SH1106G* d = display.getRawDisplay();

    // Draw semi-transparent box in center
    d->fillRect(24, 22, 80, 20, SH110X_BLACK);
    d->drawRect(24, 22, 80, 20, SH110X_WHITE);

    display.showTextCentered("PAUSED", 28, 1);
}

void PongGame::drawGameOverScreen(DisplayManager& display) {
    Adafruit_SH1106G* d = display.getRawDisplay();

    // Draw box
    d->fillRect(14, 18, 100, 28, SH110X_BLACK);
    d->drawRect(14, 18, 100, 28, SH110X_WHITE);

    if (_playerScore >= WINNING_SCORE) {
        display.showTextCentered("YOU WIN!", 24, 1);
    } else {
        display.showTextCentered("GAME OVER", 24, 1);
    }
    display.showTextCentered("Press to restart", 36, 1);
}
