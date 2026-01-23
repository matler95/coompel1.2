#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

// Forward declarations (no heavy includes in header)
class DNSServer;
class AsyncWebServer;
class WebInterface;

// WiFi connection states
enum class WiFiState {
    IDLE,              // WiFi off, not configured
    AP_MODE,           // Access Point active, waiting for config
    CONNECTING,        // Attempting to connect to configured network
    CONNECTED,         // Successfully connected
    DISCONNECTED,      // Was connected, now lost connection
    FAILED             // Connection attempt failed
};

// WiFi events
enum class WiFiEvent {
    AP_STARTED,        // Captive portal active
    CLIENT_CONNECTED,  // Device connected to AP
    CREDENTIALS_SAVED, // User submitted credentials
    CONNECTED,         // WiFi connection established
    DISCONNECTED,      // Lost connection
    FAILED             // Connection failed
};

// WiFi configuration structure
struct WiFiConfig {
    char ssid[33];                 // 32 chars + null
    char password[64];             // 63 chars + null
    bool configured = false;
    uint32_t lastConnectAttempt = 0;
    uint8_t retryCount = 0;
};

// Device configuration (setup wizard settings)
struct DeviceConfig {
    bool setupComplete = false;       // Has wizard been completed?
    bool wifiEnabled = true;          // Allow WiFi connection
    bool geolocationEnabled = true;   // Allow IP geolocation
    bool weatherEnabled = true;       // Allow weather fetching
    bool ntpEnabled = true;           // Allow NTP time sync
    int32_t manualTimezoneOffset = 0; // Manual timezone (seconds from UTC)
    bool termsAccepted = false;       // Legal terms accepted
    bool privacyAccepted = false;     // Privacy policy accepted
    uint32_t consentTimestamp = 0;    // When consent was given (epoch)
};

// Callback type for WiFi events
using WiFiEventCallback = void (*)(WiFiEvent event);

class WiFiManager {
public:
    WiFiManager();
    ~WiFiManager();

    // Initialization
    bool init();

    // Update loop (call frequently from main loop)
    void update();

    // State management
    WiFiState getState() const { return _state; }
    bool isConnected() const { return _state == WiFiState::CONNECTED; }
    bool isAPActive() const { return _state == WiFiState::AP_MODE; }

    // Configuration
    void startCaptivePortal();
    void stopCaptivePortal();
    void saveCredentials(const char* ssid, const char* password);
    void clearCredentials();
    bool hasCredentials() const { return _config.configured; }

    // Connection management
    void connect();
    void disconnect();
    int8_t getSignalStrength() const;
    const char* getSSID() const;
    const char* getConfiguredSSID() const { return _config.ssid; }
    String getIPAddress() const;
    String getAPName() const { return _apName; }

    // Device configuration (setup wizard)
    bool isSetupComplete() const { return _deviceConfig.setupComplete; }
    const DeviceConfig& getDeviceConfig() const { return _deviceConfig; }
    void saveDeviceConfig(const DeviceConfig& config);
    void loadDeviceConfig();
    void factoryReset();
    void resetSetupWizard();

    // Event callback
    void setEventCallback(WiFiEventCallback callback);

    // Web server lifecycle
    void freeWebServerMemory();
    bool isWebServerActive() const { return _webServerActive; }

    // Web server access (used by WebInterface only)
    AsyncWebServer* getWebServer() const { return _webServer; }

    // Scheduling and AP management
    void scheduleRestart(unsigned long delayMs = 500);
    void setAPAutoShutdownMs(uint32_t ms) { _apAutoShutdownMs = ms; }
    uint32_t getAPAutoShutdownMs() const { return _apAutoShutdownMs; }

private:
    // Core state
    WiFiState _state = WiFiState::IDLE;
    WiFiConfig _config;
    DeviceConfig _deviceConfig;
    WiFiEventCallback _eventCallback = nullptr;

    // Captive portal components (owned)
    DNSServer* _dnsServer = nullptr;
    AsyncWebServer* _webServer = nullptr;
    WebInterface* _webInterface = nullptr;
    bool _webServerActive = false;

    // AP configuration
    String _apName;
    IPAddress _apIP{192, 168, 4, 1};
    IPAddress _apGateway{192, 168, 4, 1};
    IPAddress _apSubnet{255, 255, 255, 0};

    // Connection management
    unsigned long _lastConnectionAttempt = 0;
    unsigned long _connectionTimeout = 0;
    uint8_t _maxRetries = 0;
    unsigned long _lastUpdateTime = 0;

    // Deferred connection after form submit
    unsigned long _pendingConnectionTime = 0;
    bool _pendingConnection = false;

    // AP auto-shutdown and restart scheduling
    uint32_t _apAutoShutdownMs = 0;      // 0 = no auto-shutdown
    unsigned long _apStartTime = 0;      // millis() when AP started
    bool _restartScheduled = false;      // scheduled system restart flag
    unsigned long _restartAtMs = 0;      // when to perform restart

    // Internal helpers
    void loadCredentials();
    void storeCredentials();
    void handleConnectionState();
    void handleAPMode();
    void triggerEvent(WiFiEvent event);
    bool validateCredentials(const char* ssid, const char* password);
    void generateAPName();
    void setupWebInterface();
};

#endif // WIFI_MANAGER_H
