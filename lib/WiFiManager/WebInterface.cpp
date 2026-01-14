#include "WebInterface.h"
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// HTML page stored in PROGMEM
const char PROGMEM html_setup[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 WiFi Setup</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .container {
            max-width: 400px;
            width: 100%;
        }
        .card {
            background: white;
            border-radius: 16px;
            padding: 32px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            margin-bottom: 16px;
        }
        h1 {
            font-size: 24px;
            margin-bottom: 8px;
            color: #1a202c;
        }
        .subtitle {
            color: #718096;
            font-size: 14px;
            margin-bottom: 24px;
        }
        h2 {
            font-size: 16px;
            margin: 24px 0 12px 0;
            color: #2d3748;
            font-weight: 600;
        }
        h2:first-of-type { margin-top: 0; }
        input {
            width: 100%;
            padding: 12px 16px;
            margin: 8px 0;
            border: 2px solid #e2e8f0;
            border-radius: 8px;
            font-size: 15px;
            transition: border-color 0.2s;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        button {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-size: 16px;
            font-weight: 600;
            transition: transform 0.1s, box-shadow 0.2s;
            margin-top: 8px;
        }
        button:hover {
            transform: translateY(-1px);
            box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
        }
        button:active {
            transform: translateY(0);
        }
        button:disabled {
            opacity: 0.6;
            cursor: not-allowed;
        }
        .status {
            padding: 12px 16px;
            margin: 12px 0;
            border-radius: 8px;
            font-size: 14px;
            display: none;
            animation: slideIn 0.3s ease;
        }
        @keyframes slideIn {
            from { opacity: 0; transform: translateY(-10px); }
            to { opacity: 1; transform: translateY(0); }
        }
        .status.success {
            background: #c6f6d5;
            color: #22543d;
            border-left: 4px solid #38a169;
        }
        .status.error {
            background: #fed7d7;
            color: #742a2a;
            border-left: 4px solid #e53e3e;
        }
        .status.info {
            background: #bee3f8;
            color: #2c5282;
            border-left: 4px solid #3182ce;
        }
        .network {
            padding: 14px 16px;
            margin: 6px 0;
            border: 2px solid #e2e8f0;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.2s;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .network:hover {
            border-color: #667eea;
            background: #f7fafc;
            transform: translateX(4px);
        }
        .network-name {
            font-weight: 500;
            color: #2d3748;
        }
        .network-info {
            font-size: 12px;
            color: #718096;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        .lock-icon {
            font-size: 14px;
        }
        .signal {
            display: inline-block;
            width: 4px;
            height: 12px;
            background: #cbd5e0;
            margin-left: 2px;
            border-radius: 2px;
        }
        .signal.strong { background: #48bb78; }
        .signal.medium { background: #ed8936; }
        .signal.weak { background: #f56565; }
        .spinner {
            border: 3px solid #f3f3f3;
            border-top: 3px solid #667eea;
            border-radius: 50%;
            width: 24px;
            height: 24px;
            animation: spin 1s linear infinite;
            display: inline-block;
            margin-right: 8px;
            vertical-align: middle;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .footer {
            text-align: center;
            color: white;
            font-size: 13px;
            opacity: 0.9;
        }
        #networks:empty:before {
            content: 'Scanning...';
            display: block;
            text-align: center;
            color: #a0aec0;
            padding: 24px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <h1>WiFi Setup</h1>
            <p class="subtitle">Configure your device connection</p>


            <h2>Available Networks</h2>
            <div id="networks"></div>

            <h2>Enter Credentials</h2>
            <form id="wifiForm">
                <input type="text" id="ssid" placeholder="Network Name (SSID)" required autocomplete="off">
                <input type="password" id="password" placeholder="Password (leave empty for open networks)" autocomplete="off">
                <button type="submit" id="connectBtn">Save</button>
            <div id="status" class="status"></div>
            </form>
        </div>
        <div class="footer">
            ESP32-C3 Device Configuration
        </div>
    </div>

    <script>
        let scanning = false;

        async function scanNetworks() {
            if (scanning) return;
            scanning = true;

            try {
                const response = await fetch('/scan');
                const networks = await response.json();
                const container = document.getElementById('networks');

                if (networks.length === 0) {
                    container.innerHTML = '<div style="text-align:center;color:#a0aec0;padding:24px">No networks found</div>';
                    scanning = false;
                    return;
                }

                // Sort by signal strength
                networks.sort((a, b) => b.rssi - a.rssi);

                container.innerHTML = networks.map(net => {
                    let signalBars = '';
                    const rssi = net.rssi;
                    if (rssi > -60) signalBars = '<span class="signal strong"></span><span class="signal strong"></span><span class="signal strong"></span>';
                    else if (rssi > -70) signalBars = '<span class="signal medium"></span><span class="signal medium"></span><span class="signal"></span>';
                    else signalBars = '<span class="signal weak"></span><span class="signal"></span><span class="signal"></span>';

                    return `<div class="network" onclick="selectNetwork('${net.ssid}', ${net.encrypted})">
                        <span class="network-name">${net.ssid}</span>
                        <span class="network-info">
                            ${net.encrypted ? '<span class="lock-icon">\ud83d\udd12</span>' : ''}
                            ${signalBars}
                        </span>
                    </div>`;
                }).join('');

            } catch (error) {
                console.error('Scan error:', error);
                document.getElementById('networks').innerHTML = '<div style="text-align:center;color:#e53e3e;padding:24px">Scan failed</div>';
            }

            scanning = false;
        }

        function selectNetwork(ssid, encrypted) {
            document.getElementById('ssid').value = ssid;
            if (encrypted) {
                document.getElementById('password').focus();
            } else {
                document.getElementById('password').value = '';
            }
        }

        document.getElementById('wifiForm').onsubmit = async (e) => {
            e.preventDefault();

            const ssid = document.getElementById('ssid').value.trim();
            const password = document.getElementById('password').value;
            const btn = document.getElementById('connectBtn');

            if (!ssid) {
                showStatus('Please enter a network name', 'error');
                return;
            }

            btn.disabled = true;
            btn.innerHTML = 'Saved';
            showStatus('Connecting to ' + ssid + '...', 'info');

            try {
                const response = await fetch('/connect', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({ssid, password})
                });

                const result = await response.json();

                if (result.success) {
                    showStatus('Connected successfully! Device is now online.', 'success');
                    setTimeout(() => {
                        showStatus('You can close this page.', 'success');
                    }, 2000);
                } else {
                    showStatus('Connection failed: ' + (result.message || 'Unknown error'), 'error');
                    btn.disabled = false;
                    btn.innerHTML = 'Save';
                }
            } catch (error) {
                showStatus('Request failed. Please try again.', 'error');
                btn.disabled = false;
                btn.innerHTML = 'Save';
            }
        };

        function showStatus(message, type) {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = 'status ' + type;
            status.style.display = 'block';
        }

        // Auto-scan on load
        scanNetworks();

        // Refresh networks every 15 seconds
        setInterval(scanNetworks, 15000);
    </script>
</body>
</html>
)rawliteral";

WebInterface::WebInterface(WiFiManager* wifiManager)
    : _wifiManager(wifiManager) {
}

void WebInterface::setupRoutes(AsyncWebServer* server) {
    if (server == nullptr) return;

    // Serve main setup page
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleRoot(request);
    });

    // Captive portal detection URLs
    server->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/");
    });

    server->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/");
    });

    server->on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/");
    });

    server->on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/");
    });

    server->on("/hotspot-detect.htm", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/");
    });

    server->on("/connectivity-check.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/");
    });

    // API endpoints
    server->on("/scan", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleScan(request);
    });

    server->on("/connect", HTTP_POST,
        [this](AsyncWebServerRequest *request){
            // This is called after body is received
            handleConnect(request);
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            // Store body data in request temporarily
            if (index == 0) {
                request->_tempObject = new String();
            }
            String* body = (String*)request->_tempObject;
            for (size_t i = 0; i < len; i++) {
                body->concat((char)data[i]);
            }
        });

    server->on("/status", HTTP_GET, [this](AsyncWebServerRequest *request){
        handleStatus(request);
    });

    // Catch-all for other requests
    server->onNotFound([](AsyncWebServerRequest *request){
        request->redirect("/");
    });

    server->begin();
    Serial.println("[WebInterface] Routes configured and server started");
}

void WebInterface::handleRoot(AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", html_setup);
}

void WebInterface::handleScan(AsyncWebServerRequest *request) {
    Serial.println("[WebInterface] Scanning for networks...");

    int numNetworks = WiFi.scanNetworks();

    StaticJsonDocument<2048> doc;
    JsonArray networks = doc.to<JsonArray>();

    for (int i = 0; i < numNetworks && i < 20; i++) {  // Limit to 20 networks
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["encrypted"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);

    Serial.printf("[WebInterface] Found %d networks\n", numNetworks);
}

void WebInterface::handleConnect(AsyncWebServerRequest *request) {
    // Get body data from temporary storage
    if (request->_tempObject == nullptr) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"No data provided\"}");
        return;
    }

    String* bodyPtr = (String*)request->_tempObject;
    String body = *bodyPtr;

    StaticJsonDocument<512> reqDoc;
    DeserializationError error = deserializeJson(reqDoc, body);

    if (error) {
        delete bodyPtr;
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }

    // Manually extract and copy the strings while body is still valid
    const char* ssidPtr = reqDoc["ssid"];
    const char* passwordPtr = reqDoc["password"];

    // Create copies with explicit buffer allocation
    char ssidBuf[33] = {0};
    char passBuf[64] = {0};

    if (ssidPtr != nullptr) {
        strncpy(ssidBuf, ssidPtr, 32);
    }
    if (passwordPtr != nullptr) {
        strncpy(passBuf, passwordPtr, 63);
    }

    // Clean up body before continuing
    delete bodyPtr;
    request->_tempObject = nullptr;

    Serial.printf("[WebInterface] Connection request for: %s\n", ssidBuf);

    if (strlen(ssidBuf) == 0) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"SSID required\"}");
        return;
    }

    // Save credentials and initiate connection
    _wifiManager->saveCredentials(ssidBuf, passBuf);

    // Send simple JSON response without creating document
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Connecting...\"}");
}

void WebInterface::handleStatus(AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;

    if (_wifiManager->isConnected()) {
        doc["connected"] = true;
        doc["ssid"] = _wifiManager->getSSID();
        doc["ip"] = _wifiManager->getIPAddress();
        doc["rssi"] = _wifiManager->getSignalStrength();
    } else {
        doc["connected"] = false;
        doc["state"] = (int)_wifiManager->getState();
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

String WebInterface::getSetupPageHTML() {
    return String(html_setup);
}
