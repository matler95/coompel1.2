#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>

class WiFiManager;
class AsyncWebServer;
class AsyncWebServerRequest;

class WebInterface {
public:
    WebInterface(WiFiManager* wifiManager);

    // Setup all web server routes
    void setupRoutes(AsyncWebServer* server);

private:
    WiFiManager* _wifiManager;

    // HTML page generation
    String getSetupPageHTML();

    // API handlers
    void handleRoot(AsyncWebServerRequest *request);
    void handleScan(AsyncWebServerRequest *request);
    void handleConnect(AsyncWebServerRequest *request);
    void handleStatus(AsyncWebServerRequest *request);
    void handleCaptivePortal(AsyncWebServerRequest *request);

    // Configuration wizard endpoints
    void handleGetConfig(AsyncWebServerRequest *request);
    void handleSaveConfig(AsyncWebServerRequest *request);
    void handleTerms(AsyncWebServerRequest *request);
    void handlePrivacy(AsyncWebServerRequest *request);
};

#endif // WEB_INTERFACE_H
