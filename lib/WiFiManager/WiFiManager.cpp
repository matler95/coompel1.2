#include "WiFiManager.h"
#include "WebInterface.h"

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

// Constants
static constexpr unsigned long WIFI_CONNECT_TIMEOUT = 30000;   // 30s
static constexpr unsigned long WIFI_RECONNECT_DELAY = 30000;   // 30s
static constexpr uint8_t WIFI_MAX_RETRIES = 3;

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

    if (_config.configured && strlen(_config.ssid) > 0) {
        Serial.printf("[WiFi] Stored SSID found: %s\n", _config.ssid);
        connect();
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
