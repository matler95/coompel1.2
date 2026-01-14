/**
 * @file MotionSensor.cpp
 * @brief Implementation of MotionSensor class
 */

#include "MotionSensor.h"

// Constants
#define GRAVITY 9.81f           // Standard gravity (m/s²)
#define SHAKE_COOLDOWN_MS 500   // Minimum time between shakes

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

MotionSensor::MotionSensor(uint8_t i2c_address)
    : _i2c_address(i2c_address),
      _initialized(false),
      _motionDetectionEnabled(true),
      _lastAccelX(0), _lastAccelY(0), _lastAccelZ(0),
      _lastAccelMagnitude(0),
      _accelOffsetX(0), _accelOffsetY(0), _accelOffsetZ(0),
      _shakeThreshold(20.0f),
      _tiltThreshold(30.0f),
      _lastEvent(MotionEvent::NONE),
      _lastShakeTime(0),
      _isShaking(false),
      _callback(nullptr)
{
}

bool MotionSensor::init(TwoWire* wire) {
    // Serial.println("[MOTION] Initializing MPU6050...");
    
    // Try to initialize MPU6050
    if (!_mpu.begin(_i2c_address, wire)) {
        // Serial.println("[MOTION] ERROR: Failed to find MPU6050!");
        Serial.printf("[MOTION] Check I2C address 0x%02X and wiring\n", _i2c_address);
        return false;
    }
    
    // Serial.println("[MOTION] MPU6050 found!");
    
    // Configure sensor ranges
    _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    // Serial.println("[MOTION] Accelerometer range: ±8G");
    
    _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    // Serial.println("[MOTION] Gyro range: ±500°/s");
    
    _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    // Serial.println("[MOTION] Filter bandwidth: 21 Hz");
    
    _initialized = true;
    
    // Auto-calibrate
    // Serial.println("[MOTION] Calibrating... keep device still");
    calibrate(100);
    
    // Serial.println("[MOTION] Initialization complete");
    return true;
}

// ============================================================================
// MAIN UPDATE METHOD
// ============================================================================

void MotionSensor::update() {
    if (!_initialized || !_motionDetectionEnabled) return;
    
    _lastEvent = MotionEvent::NONE;
    
    // Read sensor data
    _mpu.getEvent(&_accel, &_gyro, &_temp);
    
    // Store previous values
    _lastAccelX = _accel.acceleration.x - _accelOffsetX;
    _lastAccelY = _accel.acceleration.y - _accelOffsetY;
    _lastAccelZ = _accel.acceleration.z - _accelOffsetZ;
    
    // Calculate magnitude
    _lastAccelMagnitude = sqrt(
        _lastAccelX * _lastAccelX +
        _lastAccelY * _lastAccelY +
        _lastAccelZ * _lastAccelZ
    );
    
    // Detect gestures
    if (_motionDetectionEnabled) {
        detectShake();
        detectSuddenMovement();
    }
}

// ============================================================================
// EVENT DETECTION METHODS
// ============================================================================

void MotionSensor::detectShake() {
    unsigned long currentTime = millis();
    
    // Cooldown period to avoid multiple shake detections
    if ((currentTime - _lastShakeTime) < SHAKE_COOLDOWN_MS) {
        return;
    }
    
    // Check for rapid back-and-forth movement
    if (_lastAccelMagnitude > _shakeThreshold) {
        if (!_isShaking) {
            _isShaking = true;
            _lastShakeTime = currentTime;
            _lastEvent = MotionEvent::SHAKE;
            triggerCallback(MotionEvent::SHAKE);
            // Serial.println("[MOTION] Shake detected!");
        }
    } else {
        _isShaking = false;
    }
}

void MotionSensor::detectSuddenMovement() {
    // Check for rapid change in any axis
    float deltaX = abs(_lastAccelX);
    float deltaY = abs(_lastAccelY);
    float deltaZ = abs(_lastAccelZ);
    
    if (deltaX > 12.0f || deltaY > 12.0f || deltaZ > 12.0f) {
        _lastEvent = MotionEvent::SUDDEN_MOVEMENT;
        // Don't trigger callback for every sudden movement (too noisy)
    }
}

// ============================================================================
// ORIENTATION DETECTION
// ============================================================================

Orientation MotionSensor::getOrientation() const {
    if (!_initialized) return Orientation::UNKNOWN;
    
    float x = _lastAccelX;
    float y = _lastAccelY;
    float z = _lastAccelZ;
    
    // Determine dominant axis
    float absX = abs(x);
    float absY = abs(y);
    float absZ = abs(z);
    
    // Z-axis dominant (flat)
    if (absZ > absX && absZ > absY) {
        if (z > 7.0f) return Orientation::FLAT;
        if (z < -7.0f) return Orientation::UPSIDE_DOWN;
    }
    
    // Y-axis dominant (portrait)
    if (absY > absX && absY > absZ) {
        if (y > 7.0f) return Orientation::PORTRAIT;
        if (y < -7.0f) return Orientation::PORTRAIT_INVERTED;
    }
    
    // X-axis dominant (landscape)
    if (absX > absY && absX > absZ) {
        if (x > 7.0f) return Orientation::LANDSCAPE_RIGHT;
        if (x < -7.0f) return Orientation::LANDSCAPE_LEFT;
    }
    
    return Orientation::UNKNOWN;
}

// ============================================================================
// RAW DATA ACCESS
// ============================================================================

void MotionSensor::getAcceleration(float& x, float& y, float& z) {
    x = _lastAccelX;
    y = _lastAccelY;
    z = _lastAccelZ;
}

void MotionSensor::getRotation(float& x, float& y, float& z) {
    x = _gyro.gyro.x;
    y = _gyro.gyro.y;
    z = _gyro.gyro.z;
}

float MotionSensor::getTemperature() {
    return _temp.temperature;
}

float MotionSensor::getAccelMagnitude() const {
    return _lastAccelMagnitude;
}

MotionEvent MotionSensor::getEvent() {
    return _lastEvent;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void MotionSensor::setShakeThreshold(float threshold) {
    _shakeThreshold = threshold;
}

void MotionSensor::setTiltThreshold(float degrees) {
    _tiltThreshold = degrees;
}

void MotionSensor::setMotionDetection(bool enabled) {
    _motionDetectionEnabled = enabled;
}

void MotionSensor::setCallback(MotionCallback callback) {
    _callback = callback;
}

// ============================================================================
// CALIBRATION
// ============================================================================

void MotionSensor::calibrate(uint16_t samples) {
    if (!_initialized) return;
    
    float sumX = 0, sumY = 0, sumZ = 0;
    
    for (uint16_t i = 0; i < samples; i++) {
        _mpu.getEvent(&_accel, &_gyro, &_temp);
        sumX += _accel.acceleration.x;
        sumY += _accel.acceleration.y;
        sumZ += _accel.acceleration.z - GRAVITY;  // Remove gravity from Z
        delay(10);
    }
    
    _accelOffsetX = sumX / samples;
    _accelOffsetY = sumY / samples;
    _accelOffsetZ = sumZ / samples;
    
    Serial.printf("[MOTION] Calibration offsets: X=%.2f, Y=%.2f, Z=%.2f\n",
                  _accelOffsetX, _accelOffsetY, _accelOffsetZ);
}

// ============================================================================
// UTILITIES
// ============================================================================

bool MotionSensor::isStationary() const {
    // Device is stationary if acceleration is close to gravity
    return abs(_lastAccelMagnitude - GRAVITY) < 1.0f;
}

void MotionSensor::reset() {
    _lastEvent = MotionEvent::NONE;
    _isShaking = false;
    _lastShakeTime = 0;
}

void MotionSensor::triggerCallback(MotionEvent event) {
    if (_callback != nullptr) {
        _callback(event);
    }
}