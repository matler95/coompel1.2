/**
 * @file SensorHub.h
 * @brief Centralized sensor management system
 * @version 1.0.0
 * 
 * Manages all analog and digital sensors:
 * - DHT11 (temperature/humidity)
 * - HW-484 sound sensor
 * - Battery monitoring (future)
 */

#ifndef SENSOR_HUB_H
#define SENSOR_HUB_H

#include <Arduino.h>
#include <DHT.h>

// ============================================================================
// SENSOR CONFIGURATION
// ============================================================================
#define DHT_TYPE DHT11

// ============================================================================
// SENSOR DATA STRUCTURE
// ============================================================================
struct SensorData {
    // DHT11 data
    float temperature;      // Celsius
    float humidity;         // Percentage
    bool dhtValid;          // True if reading successful
    
    // Sound sensor data
    uint16_t soundLevel;    // Raw ADC value (0-4095)
    uint16_t soundPeak;     // Peak level in last period
    float soundDB;          // Approximate decibels
     
    // Battery (future)
    uint16_t batteryLevel;
    uint8_t batteryPercent;
};

// ============================================================================
// SENSOR EVENT CALLBACKS
// ============================================================================
typedef void (*SoundThresholdCallback)(uint16_t level);
typedef void (*TemperatureChangeCallback)(float temp);

// ============================================================================
// SENSOR HUB CLASS
// ============================================================================
class SensorHub {
public:
    /**
     * @brief Constructor
     */
    SensorHub();
    
    /**
     * @brief Initialize all sensors
     * @param dhtPin DHT11 data pin
     * @param soundPin HW-484 analog pin
     * @return true if at least one sensor initialized
     */
    bool init(uint8_t dhtPin, uint8_t soundPin);
    
    /**
     * @brief Update all sensor readings (call in loop)
     * @param forceUpdate If true, ignore update intervals
     */
    void update(bool forceUpdate = false);
    
    /**
     * @brief Get current sensor data
     * @return Reference to SensorData structure
     */
    const SensorData& getData() const { return _data; }
    
    /**
     * @brief Get temperature in Celsius
     * @return Temperature value
     */
    float getTemperature() const { return _data.temperature; }
    
    /**
     * @brief Get temperature in Fahrenheit
     * @return Temperature in F
     */
    float getTemperatureFahrenheit() const { 
        return (_data.temperature * 9.0f / 5.0f) + 32.0f; 
    }
    
    /**
     * @brief Get humidity percentage
     * @return Humidity value
     */
    float getHumidity() const { return _data.humidity; }
    
    /**
     * @brief Get current sound level (0-4095)
     * @return Sound level raw value
     */
    uint16_t getSoundLevel() const { return _data.soundLevel; }
    
    /**
     * @brief Get sound level as percentage (0-100)
     * @return Sound percentage
     */
    uint8_t getSoundPercent() const { 
        return map(_data.soundLevel, 0, 4095, 0, 100); 
    }
    
    /**
     * @brief Get peak sound level
     * @return Peak value
     */
    uint16_t getSoundPeak() const { return _data.soundPeak; }
        
    /**
     * @brief Set DHT update interval
     * @param intervalMs Milliseconds between readings (min 2000)
     */
    void setDHTInterval(uint16_t intervalMs);
    
    /**
     * @brief Set analog sensor update interval
     * @param intervalMs Milliseconds between readings
     */
    void setAnalogInterval(uint16_t intervalMs);
    
    /**
     * @brief Set sound threshold for callback
     * @param threshold Level that triggers callback
     * @param callback Function to call
     */
    void setSoundThreshold(uint16_t threshold, SoundThresholdCallback callback);
    
    /**
     * @brief Set temperature change callback
     * @param deltaTemp Minimum change to trigger callback
     * @param callback Function to call
     */
    void setTemperatureCallback(float deltaTemp, TemperatureChangeCallback callback);
    
    /**
     * @brief Enable/disable specific sensors
     */
    void enableDHT(bool enabled);
    void enableSound(bool enabled);
    
    /**
     * @brief Check if sensors are initialized
     */
    bool isDHTReady() const { return _dhtEnabled && _dhtPin > 0; }
    bool isSoundReady() const { return _soundEnabled && _soundPin > 0; }
    
    /**
     * @brief Reset peak sound level
     */
    void resetSoundPeak();

private:
    // DHT sensor
    DHT* _dht;
    uint8_t _dhtPin;
    bool _dhtEnabled;
    unsigned long _lastDHTRead;
    uint16_t _dhtInterval;
    float _lastTemperature;
    
    // Analog sensors
    uint8_t _soundPin;
    bool _soundEnabled;
    unsigned long _lastAnalogRead;
    uint16_t _analogInterval;
    
    // Sensor data
    SensorData _data;
    
    // Sound processing
    uint16_t _soundSamples[16];  // Ring buffer for averaging
    uint8_t _soundSampleIndex;
    uint16_t _soundThreshold;
    SoundThresholdCallback _soundCallback;
    
    // Temperature monitoring
    float _tempDelta;
    TemperatureChangeCallback _tempCallback;
    
    // Private methods
    void updateDHT();
    void updateAnalogSensors();
    void processSoundLevel(uint16_t rawValue);
    uint16_t getAverageSoundLevel();
};

#endif // SENSOR_HUB_H