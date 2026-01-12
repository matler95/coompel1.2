/**
 * @file MotionSensor.h
 * @brief MPU6050 motion sensor interface with gesture detection
 * @version 1.0.0
 * 
 * Features:
 * - 6-axis motion tracking (3-axis gyro + 3-axis accelerometer)
 * - Tap detection (single and double)
 * - Shake detection
 * - Tilt/orientation detection
 * - Configurable sensitivity
 * - Event-driven callbacks
 */

#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ============================================================================
// MOTION EVENT TYPES (Updated - No Tap Events)
// ============================================================================
enum class MotionEvent {
    NONE,
    SHAKE,            // Shake gesture detected
    TILT_LEFT,        // Tilted left
    TILT_RIGHT,       // Tilted right
    TILT_FORWARD,     // Tilted forward
    TILT_BACKWARD,    // Tilted backward
    UPSIDE_DOWN,      // Device flipped upside down
    SUDDEN_MOVEMENT   // Rapid acceleration
};

// ============================================================================
// ORIENTATION ENUM
// ============================================================================
enum class Orientation {
    FLAT,           // Lying flat (screen up)
    UPSIDE_DOWN,    // Lying flat (screen down)
    PORTRAIT,       // Standing upright
    PORTRAIT_INVERTED,
    LANDSCAPE_LEFT,
    LANDSCAPE_RIGHT,
    UNKNOWN
};

// ============================================================================
// CALLBACK TYPE
// ============================================================================
typedef void (*MotionCallback)(MotionEvent event);

// ============================================================================
// MOTION SENSOR CLASS
// ============================================================================
class MotionSensor {
public:
    /**
     * @brief Constructor
     * @param i2c_address MPU6050 I2C address (default: 0x68)
     */
    MotionSensor(uint8_t i2c_address = 0x68);
    
    /**
     * @brief Initialize sensor hardware
     * @param wire Pointer to Wire object (default: &Wire)
     * @return true if initialization successful
     */
    bool init(TwoWire* wire = &Wire);
    
    /**
     * @brief Update sensor readings (call in loop)
     * Must be called frequently for accurate gesture detection
     */
    void update();
    
    /**
     * @brief Get last detected motion event
     * @return MotionEvent type
     */
    MotionEvent getEvent();
    
    /**
     * @brief Get current device orientation
     * @return Orientation enum
     */
    Orientation getOrientation() const;
    
    /**
     * @brief Register callback for motion events
     * @param callback Function to call when event occurs
     */
    void setCallback(MotionCallback callback);
    
    // ========================================================================
    // RAW SENSOR DATA ACCESS
    // ========================================================================
    
    /**
     * @brief Get raw accelerometer data
     * @param x Output for X-axis (m/s²)
     * @param y Output for Y-axis (m/s²)
     * @param z Output for Z-axis (m/s²)
     */
    void getAcceleration(float& x, float& y, float& z);
    
    /**
     * @brief Get raw gyroscope data
     * @param x Output for X-axis (rad/s)
     * @param y Output for Y-axis (rad/s)
     * @param z Output for Z-axis (rad/s)
     */
    void getRotation(float& x, float& y, float& z);
    
    /**
     * @brief Get temperature from sensor
     * @return Temperature in Celsius
     */
    float getTemperature();
    
    /**
     * @brief Get total acceleration magnitude
     * @return Magnitude in m/s²
     */
    float getAccelMagnitude() const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
        
    /**
     * @brief Set shake detection sensitivity
     * @param threshold Acceleration threshold (m/s²), default: 20.0
     */
    void setShakeThreshold(float threshold);
    
    /**
     * @brief Set tilt angle threshold
     * @param degrees Tilt angle in degrees, default: 30
     */
    void setTiltThreshold(float degrees);
    
    /**
     * @brief Enable/disable motion detection
     * @param enabled true to enable
     */
    void setMotionDetection(bool enabled);
    
    /**
     * @brief Calibrate sensor (call when device is stationary)
     * @param samples Number of samples to average (default: 100)
     */
    void calibrate(uint16_t samples = 100);
    
    // ========================================================================
    // STATUS & UTILITIES
    // ========================================================================
    
    /**
     * @brief Check if sensor is ready
     * @return true if initialized and working
     */
    bool isReady() const { return _initialized; }
    
    /**
     * @brief Check if device is stationary
     * @return true if no significant movement
     */
    bool isStationary() const;
    
    /**
     * @brief Reset sensor state
     */
    void reset();

private:
    Adafruit_MPU6050 _mpu;
    
    // Configuration
    uint8_t _i2c_address;
    bool _initialized;
    bool _motionDetectionEnabled;
    
    // Sensor data
    sensors_event_t _accel;
    sensors_event_t _gyro;
    sensors_event_t _temp;
    
    // Previous readings for change detection
    float _lastAccelX, _lastAccelY, _lastAccelZ;
    float _lastAccelMagnitude;
    
    // Calibration offsets
    float _accelOffsetX, _accelOffsetY, _accelOffsetZ;
    
    // Thresholds
    float _shakeThreshold;
    float _tiltThreshold;
    
    // Event detection state
    MotionEvent _lastEvent;
    unsigned long _lastShakeTime;
    bool _isShaking;
    
    // Callback
    MotionCallback _callback;
    
    // Private methods
    void detectTap();
    void detectShake();
    void detectSuddenMovement();
    Orientation calculateOrientation();
    void triggerCallback(MotionEvent event);
};

#endif // MOTION_SENSOR_H