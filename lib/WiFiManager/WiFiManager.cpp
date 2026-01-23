#include "WiFiManager.h"
#include "WebInterface.h"

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

// Constants
static constexpr unsigned long WIFI_CONNECT_TIMEOUT = 30000;   // 30s
static constexpr unsigned long WIFI_RECONNECT_DELAY = 30000;   // 30s
static constexpr uint8_t WIFI_MAX_RETRIES = 3;
static constexpr unsigned long DEFAULT_AP_AUTO_SHUTDOWN_MS = 15UL * 60UL * 1000UL; // 15 minutes

// NVS namespace and keys for device config
static constexpr const char* NVS_CONFIG_NS = "config";
static constexpr const char* KEY_SETUP_COMPLETE = "setup_ok";
static constexpr const char* KEY_WIFI_ENABLED = "wifi_en";
static constexpr const char* KEY_GEO_ENABLED = "geo_en";
static constexpr const char* KEY_WEATHER_ENABLED = "wx_en";
static constexpr const char* KEY_NTP_ENABLED = "ntp_en";
static constexpr const char* KEY_MANUAL_TZ = "manual_tz";
static constexpr const char* KEY_TERMS_OK = "terms_ok";
static constexpr const char* KEY_PRIVACY_OK = "privacy_ok";
static constexpr const char* KEY_CONSENT_TIME = "consent_ts";

// --------------------------------------------------
// Constructor / Destructor
// --------------------------------------------------

WiFiManager::WiFiManager() {
    memset(&_config, 0, sizeof(_config));
}

WiFiManager::~WiFiManager() {
    freeWebServerMemory();
}

// --------------------------------------------------
// Initialization
// --------------------------------------------------

bool WiFiManager::init() {
    Serial.println("[WiFi] Initializing WiFiManager");

    generateAPName();
    loadCredentials();
    loadDeviceConfig();

    // Apply default AP auto-shutdown if not configured
    if (_apAutoShutdownMs == 0) {
        _apAutoShutdownMs = DEFAULT_AP_AUTO_SHUTDOWN_MS;
    }

    // If setup not complete, always start captive portal
    if (!_deviceConfig.setupComplete) {
        Serial.println("[WiFi] Setup not complete, starting captive portal");
        startCaptivePortal();
        return true;
    }

    // Setup complete - check if WiFi is enabled and configured
    if (_deviceConfig.wifiEnabled && _config.configured && strlen(_config.ssid) > 0) {
        Serial.printf("[WiFi] Stored SSID found: %s\n", _config.ssid);
        connect();
    } else if (!_deviceConfig.wifiEnabled) {
        Serial.println("[WiFi] WiFi disabled by user, staying offline");
        _state = WiFiState::IDLE;
    } else {
        Serial.println("[WiFi] No credentials found, starting AP mode");
        startCaptivePortal();
    }

    return true;
}

// --------------------------------------------------
// Main update loop
// --------------------------------------------------

void WiFiManager::update() {
    unsigned long now = millis();

    if (now - _lastUpdateTime < 100) {
        return;
    }
    _lastUpdateTime = now;

    // Delayed connect after form submit
    if (_pendingConnection && (now - _pendingConnectionTime >= 2000)) {
        _pendingConnection = false;
        Serial.println("[WiFi] Executing delayed connection");
        connect();
        return;
    }

    switch (_state) {
        case WiFiState::AP_MODE:
            handleAPMode();
            break;

        case WiFiState::CONNECTING:
            handleConnectionState();
            break;

        case WiFiState::CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] Connection lost");
                _state = WiFiState::DISCONNECTED;
                triggerEvent(WiFiEvent::DISCONNECTED);
            }
            break;

        case WiFiState::DISCONNECTED:
            if (now - _lastConnectionAttempt > WIFI_RECONNECT_DELAY) {
                Serial.println("[WiFi] Reconnecting...");
                connect();
            }
            break;

        default:
            break;
    }
}

// --------------------------------------------------
// Captive portal
// --------------------------------------------------

void WiFiManager::startCaptivePortal() {
    Serial.println("[WiFi] Starting captive portal");

    WiFi.disconnect(true);
    delay(100);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(_apIP, _apGateway, _apSubnet);
    WiFi.softAP(_apName.c_str());

    Serial.printf("[WiFi] AP: %s (%s)\n",
                  _apName.c_str(),
                  _apIP.toString().c_str());

    _apStartTime = millis();

    if (!_dnsServer) {
        _dnsServer = new DNSServer();
    }
    _dnsServer->start(53, "*", _apIP);

    if (!_webServer) {
        _webServer = new AsyncWebServer(80);
    }

    setupWebInterface();

    _webServerActive = true;
    _state = WiFiState::AP_MODE;

    Serial.printf("[WiFi] Heap free: %u\n", ESP.getFreeHeap());

    triggerEvent(WiFiEvent::AP_STARTED);
}

void WiFiManager::stopCaptivePortal() {
    Serial.println("[WiFi] Stopping captive portal");

    if (_dnsServer) {
        _dnsServer->stop();
    }

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
}

// --------------------------------------------------
// WiFi connection
// --------------------------------------------------

void WiFiManager::connect() {
    if (!_config.configured || strlen(_config.ssid) == 0) {
        Serial.println("[WiFi] No credentials available");
        _state = WiFiState::FAILED;
        return;
    }

    if (_state == WiFiState::AP_MODE) {
        stopCaptivePortal();
    }

    Serial.printf("[WiFi] Connecting to %s\n", _config.ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(_config.ssid, _config.password);

    _state = WiFiState::CONNECTING;
    _lastConnectionAttempt = millis();
    _config.lastConnectAttempt = _lastConnectionAttempt;
}

void WiFiManager::disconnect() {
    Serial.println("[WiFi] Disconnecting");
    WiFi.disconnect(true);
    _state = WiFiState::DISCONNECTED;
}

// --------------------------------------------------
// Credentials
// --------------------------------------------------

void WiFiManager::saveCredentials(const char* ssid, const char* password) {
    if (!validateCredentials(ssid, password)) {
        Serial.println("[WiFi] Invalid credentials");
        return;
    }

    strncpy(_config.ssid, ssid, sizeof(_config.ssid) - 1);
    strncpy(_config.password, password, sizeof(_config.password) - 1);

    _config.configured = true;
    _config.retryCount = 0;

    storeCredentials();
    triggerEvent(WiFiEvent::CREDENTIALS_SAVED);

    _pendingConnection = true;
    _pendingConnectionTime = millis();

    Serial.println("[WiFi] Credentials saved, connecting soon");
}

void WiFiManager::clearCredentials() {
    Serial.println("[WiFi] Clearing credentials");

    memset(&_config, 0, sizeof(_config));

    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();

    disconnect();
}

// --------------------------------------------------
// Info
// --------------------------------------------------

int8_t WiFiManager::getSignalStrength() const {
    return (_state == WiFiState::CONNECTED) ? WiFi.RSSI() : 0;
}

const char* WiFiManager::getSSID() const {
    return (_state == WiFiState::CONNECTED) ? _config.ssid : "";
}

String WiFiManager::getIPAddress() const {
    return (_state == WiFiState::CONNECTED)
           ? WiFi.localIP().toString()
           : "0.0.0.0";
}

void WiFiManager::setEventCallback(WiFiEventCallback callback) {
    _eventCallback = callback;
}

// --------------------------------------------------
// Memory cleanup
// --------------------------------------------------

void WiFiManager::freeWebServerMemory() {
    if (!_webServerActive) return;

    Serial.println("[WiFi] Freeing web server memory");
    size_t before = ESP.getFreeHeap();

    delete _webInterface;
    _webInterface = nullptr;

    if (_webServer) {
        _webServer->end();
        delete _webServer;
        _webServer = nullptr;
    }

    if (_dnsServer) {
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer = nullptr;
    }

    _webServerActive = false;

    size_t after = ESP.getFreeHeap();
    Serial.printf("[WiFi] Heap freed: %u bytes\n", after - before);
}

// --------------------------------------------------
// Internal helpers
// --------------------------------------------------

void WiFiManager::loadCredentials() {
    Preferences prefs;
    prefs.begin("wifi", true);

    _config.configured = prefs.getBool("configured", false);

    if (_config.configured) {
        prefs.getString("ssid", _config.ssid, sizeof(_config.ssid));
        prefs.getString("password", _config.password, sizeof(_config.password));
        Serial.printf("[WiFi] Loaded SSID: %s\n", _config.ssid);
    }

    prefs.end();
}

void WiFiManager::storeCredentials() {
    Preferences prefs;
    prefs.begin("wifi", false);

    prefs.putBool("configured", _config.configured);
    prefs.putString("ssid", _config.ssid);
    prefs.putString("password", _config.password);

    prefs.end();
}

void WiFiManager::handleConnectionState() {
    unsigned long now = millis();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Connected");
        Serial.println(WiFi.localIP());

        _state = WiFiState::CONNECTED;
        _config.retryCount = 0;

        triggerEvent(WiFiEvent::CONNECTED);
        freeWebServerMemory();
        return;
    }

    if (now - _lastConnectionAttempt > WIFI_CONNECT_TIMEOUT) {
        _config.retryCount++;

        if (_config.retryCount >= WIFI_MAX_RETRIES) {
            Serial.println("[WiFi] Connection failed");
            _state = WiFiState::FAILED;
            triggerEvent(WiFiEvent::FAILED);
            startCaptivePortal();
        } else {
            Serial.println("[WiFi] Retrying connection");
            _lastConnectionAttempt = now;
            WiFi.disconnect();
            delay(100);
            WiFi.begin(_config.ssid, _config.password);
        }
    }
}

void WiFiManager::handleAPMode() {
    if (_dnsServer) {
        _dnsServer->processNextRequest();
    }

    // Check for AP auto-shutdown timeout
    if (_apAutoShutdownMs > 0 && _apStartTime > 0) {
        unsigned long now = millis();
        if (now - _apStartTime >= _apAutoShutdownMs) {
            Serial.println("[WiFi] AP auto-shutdown timeout reached, stopping captive portal");
            stopCaptivePortal();
            freeWebServerMemory();
            _state = WiFiState::IDLE;
            triggerEvent(WiFiEvent::DISCONNECTED);
            _apStartTime = 0;
        }
    }
}

void WiFiManager::triggerEvent(WiFiEvent event) {
    if (_eventCallback) {
        _eventCallback(event);
    }
}

bool WiFiManager::validateCredentials(const char* ssid, const char* password) {
    size_t ssidLen = strlen(ssid);
    size_t passLen = strlen(password);

    if (ssidLen == 0 || ssidLen > 32) return false;
    if (passLen > 0 && passLen < 8) return false;
    if (passLen > 63) return false;

    return true;
}

void WiFiManager::generateAPName() {
    uint8_t mac[6];
    WiFi.macAddress(mac);

    char buf[32];
    snprintf(buf, sizeof(buf), "coompel-%02X%02X", mac[4], mac[5]);
    _apName = buf;
}

void WiFiManager::setupWebInterface() {
    if (!_webServer) return;

    delete _webInterface;
    _webInterface = new WebInterface(this);
    _webInterface->setupRoutes(_webServer);
}

// --------------------------------------------------
// Device Configuration (Setup Wizard)
// --------------------------------------------------

void WiFiManager::loadDeviceConfig() {
    Preferences prefs;
    prefs.begin(NVS_CONFIG_NS, true);

    _deviceConfig.setupComplete = prefs.getBool(KEY_SETUP_COMPLETE, false);
    _deviceConfig.wifiEnabled = prefs.getBool(KEY_WIFI_ENABLED, true);
    _deviceConfig.geolocationEnabled = prefs.getBool(KEY_GEO_ENABLED, true);
    _deviceConfig.weatherEnabled = prefs.getBool(KEY_WEATHER_ENABLED, true);
    _deviceConfig.ntpEnabled = prefs.getBool(KEY_NTP_ENABLED, true);
    _deviceConfig.manualTimezoneOffset = prefs.getInt(KEY_MANUAL_TZ, 0);
    _deviceConfig.termsAccepted = prefs.getBool(KEY_TERMS_OK, false);
    _deviceConfig.privacyAccepted = prefs.getBool(KEY_PRIVACY_OK, false);
    _deviceConfig.consentTimestamp = prefs.getUInt(KEY_CONSENT_TIME, 0);

    prefs.end();

    Serial.printf("[WiFi] Config loaded: setup=%d, wifi=%d, geo=%d, wx=%d, ntp=%d\n",
                  _deviceConfig.setupComplete,
                  _deviceConfig.wifiEnabled,
                  _deviceConfig.geolocationEnabled,
                  _deviceConfig.weatherEnabled,
                  _deviceConfig.ntpEnabled);
}

void WiFiManager::saveDeviceConfig(const DeviceConfig& config) {
    _deviceConfig = config;

    Preferences prefs;
    prefs.begin(NVS_CONFIG_NS, false);

    prefs.putBool(KEY_SETUP_COMPLETE, _deviceConfig.setupComplete);
    prefs.putBool(KEY_WIFI_ENABLED, _deviceConfig.wifiEnabled);
    prefs.putBool(KEY_GEO_ENABLED, _deviceConfig.geolocationEnabled);
    prefs.putBool(KEY_WEATHER_ENABLED, _deviceConfig.weatherEnabled);
    prefs.putBool(KEY_NTP_ENABLED, _deviceConfig.ntpEnabled);
    prefs.putInt(KEY_MANUAL_TZ, _deviceConfig.manualTimezoneOffset);
    prefs.putBool(KEY_TERMS_OK, _deviceConfig.termsAccepted);
    prefs.putBool(KEY_PRIVACY_OK, _deviceConfig.privacyAccepted);
    prefs.putUInt(KEY_CONSENT_TIME, _deviceConfig.consentTimestamp);

    prefs.end();

    Serial.println("[WiFi] Device config saved");
}

void WiFiManager::factoryReset() {
    Serial.println("[WiFi] Factory reset - clearing all data");

    // Clear WiFi credentials
    clearCredentials();

    // Clear device config
    Preferences prefs;
    prefs.begin(NVS_CONFIG_NS, false);
    prefs.clear();
    prefs.end();

    // Reset in-memory config
    _deviceConfig = DeviceConfig();

    Serial.println("[WiFi] Factory reset complete - restarting");
    delay(500);
    ESP.restart();
}

void WiFiManager::resetSetupWizard() {
    Serial.println("[WiFi] Resetting setup wizard");

    // Only reset setup complete flag, keep WiFi credentials
    _deviceConfig.setupComplete = false;
    _deviceConfig.termsAccepted = false;
    _deviceConfig.privacyAccepted = false;
    _deviceConfig.consentTimestamp = 0;

    Preferences prefs;
    prefs.begin(NVS_CONFIG_NS, false);
    prefs.putBool(KEY_SETUP_COMPLETE, false);
    prefs.putBool(KEY_TERMS_OK, false);
    prefs.putBool(KEY_PRIVACY_OK, false);
    prefs.putUInt(KEY_CONSENT_TIME, 0);
    prefs.end();

    Serial.println("[WiFi] Setup wizard reset - restarting");
    delay(500);
    ESP.restart();
}
