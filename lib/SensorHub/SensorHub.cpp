/**
 * @file SensorHub.cpp
 * @brief Implementation of SensorHub class
 */

#include "SensorHub.h"

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

SensorHub::SensorHub()
    : _dht(nullptr),
      _dhtPin(0),
      _dhtEnabled(false),
      _lastDHTRead(0),
      _dhtInterval(2000),  // DHT11 requires 2 second minimum
      _lastTemperature(0),
      _soundPin(0),
      _potPin(0),
      _soundEnabled(false),
      _potEnabled(false),
      _lastAnalogRead(0),
      _analogInterval(100),  // 10 Hz for analog
      _soundSampleIndex(0),
      _soundThreshold(2000),
      _soundCallback(nullptr),
      _tempDelta(1.0f),
      _tempCallback(nullptr)
{
    // Initialize sensor data
    _data.temperature = 0;
    _data.humidity = 0;
    _data.dhtValid = false;
    _data.soundLevel = 0;
    _data.soundPeak = 0;
    _data.soundDB = 0;
    _data.potValue = 0;
    _data.potPercent = 0;
    _data.batteryLevel = 0;
    _data.batteryPercent = 0;
    
    // Initialize sound samples
    for (uint8_t i = 0; i < 16; i++) {
        _soundSamples[i] = 0;
    }
}

bool SensorHub::init(uint8_t dhtPin, uint8_t soundPin, uint8_t potPin) {
    Serial.println("[SENSOR] Initializing Sensor Hub...");
    
    bool anyInitialized = false;
    
    // Initialize DHT11
    if (dhtPin > 0) {
        _dhtPin = dhtPin;
        _dht = new DHT(_dhtPin, DHT_TYPE);
        _dht->begin();
        _dhtEnabled = true;
        anyInitialized = true;
        Serial.printf("[SENSOR] DHT11 on GPIO%d\n", dhtPin);
        
        // First reading (may be invalid)
        delay(2000);
        updateDHT();
    }
    
    // Initialize sound sensor
    if (soundPin > 0) {
        _soundPin = soundPin;
        _soundEnabled = true;
        pinMode(_soundPin, INPUT);
        anyInitialized = true;
        Serial.printf("[SENSOR] Sound sensor on GPIO%d\n", soundPin);
    }
    
    // Initialize potentiometer
    if (potPin > 0) {
        _potPin = potPin;
        _potEnabled = true;
        pinMode(_potPin, INPUT);
        anyInitialized = true;
        Serial.printf("[SENSOR] Potentiometer on GPIO%d\n", potPin);
    }
    
    if (anyInitialized) {
        Serial.println("[SENSOR] Sensor Hub ready");
    } else {
        Serial.println("[SENSOR] WARNING: No sensors configured");
    }
    
    return anyInitialized;
}

// ============================================================================
// UPDATE METHODS
// ============================================================================

void SensorHub::update(bool forceUpdate) {
    unsigned long currentTime = millis();
    
    // Update DHT (slow, 2+ second interval)
    if (_dhtEnabled && (forceUpdate || (currentTime - _lastDHTRead >= _dhtInterval))) {
        updateDHT();
        _lastDHTRead = currentTime;
    }
    
    // Update analog sensors (fast, 100ms interval)
    if ((_soundEnabled || _potEnabled) && 
        (forceUpdate || (currentTime - _lastAnalogRead >= _analogInterval))) {
        updateAnalogSensors();
        _lastAnalogRead = currentTime;
    }
}

void SensorHub::updateDHT() {
    if (!_dhtEnabled || _dht == nullptr) return;
    
    float temp = _dht->readTemperature();
    float hum = _dht->readHumidity();
    
    // Check if readings are valid
    if (isnan(temp) || isnan(hum)) {
        _data.dhtValid = false;
        Serial.println("[SENSOR] DHT read failed");
        return;
    }
    
    _data.temperature = temp;
    _data.humidity = hum;
    _data.dhtValid = true;
    
    // Check for significant temperature change
    if (_tempCallback != nullptr) {
        if (abs(temp - _lastTemperature) >= _tempDelta) {
            _tempCallback(temp);
            _lastTemperature = temp;
        }
    }
}

void SensorHub::updateAnalogSensors() {
    // Read sound sensor
    if (_soundEnabled) {
        uint16_t rawSound = analogRead(_soundPin);
        processSoundLevel(rawSound);
        _data.soundLevel = getAverageSoundLevel();
        
        // Update peak
        if (_data.soundLevel > _data.soundPeak) {
            _data.soundPeak = _data.soundLevel;
        }
        
        // Approximate dB (very rough estimate)
        // 0 dB = threshold of hearing, 120 dB = threshold of pain
        // Map 0-4095 to roughly 30-90 dB
        _data.soundDB = map(_data.soundLevel, 0, 4095, 30, 90);
        
        // Check threshold callback
        if (_soundCallback != nullptr && _data.soundLevel > _soundThreshold) {
            _soundCallback(_data.soundLevel);
        }
    }
    
    // Read potentiometer
    if (_potEnabled) {
        _data.potValue = analogRead(_potPin);
        _data.potPercent = map(_data.potValue, 0, 4095, 0, 100);
    }
}

void SensorHub::processSoundLevel(uint16_t rawValue) {
    // Add to ring buffer
    _soundSamples[_soundSampleIndex] = rawValue;
    _soundSampleIndex = (_soundSampleIndex + 1) % 16;
}

uint16_t SensorHub::getAverageSoundLevel() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 16; i++) {
        sum += _soundSamples[i];
    }
    return sum / 16;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void SensorHub::setDHTInterval(uint16_t intervalMs) {
    // DHT11 requires minimum 2 seconds between readings
    _dhtInterval = max(intervalMs, (uint16_t)2000);
}

void SensorHub::setAnalogInterval(uint16_t intervalMs) {
    _analogInterval = intervalMs;
}

void SensorHub::setSoundThreshold(uint16_t threshold, SoundThresholdCallback callback) {
    _soundThreshold = threshold;
    _soundCallback = callback;
}

void SensorHub::setTemperatureCallback(float deltaTemp, TemperatureChangeCallback callback) {
    _tempDelta = deltaTemp;
    _tempCallback = callback;
}

void SensorHub::enableDHT(bool enabled) {
    _dhtEnabled = enabled;
}

void SensorHub::enableSound(bool enabled) {
    _soundEnabled = enabled;
}

void SensorHub::enablePot(bool enabled) {
    _potEnabled = enabled;
}

void SensorHub::resetSoundPeak() {
    _data.soundPeak = 0;
}