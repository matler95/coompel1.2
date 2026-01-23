# Firmware Audit – ESP32-C3 Interactive Device (coompel1.2)

**Date:** 2026-01-16  
**Target platform:** ESP32-C3, Arduino framework + FreeRTOS  
**Intended use:** 24/7 commercial device, minimal user intervention

---

## 1. Executive Summary

### 1.1 Overall Maturity Level

Overall assessment: **Near-commercial**, with several **High** and a few **Critical** risks that should be addressed before release.

Positive aspects:
- **Clear modular design:** Display, input, sensors, motion, WiFi, weather, menu, and animation are well-separated.
- **Non-blocking main loop:** Core loop is cooperative, with short per-iteration work and a small `delay(10)` to yield.
- **Explicit state machines:** WiFi and weather services maintain explicit states and use timers rather than busy loops.
- **NVS-backed configuration:** WiFi credentials, device configuration, and weather cache are persisted with `Preferences` using fixed keys.
- **Captive portal UX:** Multi-step, privacy-aware setup wizard with explicit consent handling and feature toggles.

Concerns for 24/7/commercial use:
- **TLS & HTTP clients use `setInsecure()`**, no pinning or CA validation (security risk, but common on constrained IoT).
- **Multiple heap-allocating components** (AsyncWebServer, DNS, weather/geo clients, JSON) without centralized memory budget or fragmentation monitoring.
- **Long-running background task (WeatherService) + main loop** both depend on heap; failure modes are mostly logged but not surfaced to UI.
- **Factory reset / setup reset call `ESP.restart()` after short `delay(500)`** without explicit quiescing of background activity.
- **Power management constants exist but are not enforced**; device will not enter sleep/deep-sleep despite config.h suggestions.

### 1.2 Top 5–10 Highest-Risk Issues

1. **Insecure TLS usage for external APIs**  
   - `GeoLocationClient` and `WeatherClient` use `WiFiClientSecure` with `setInsecure()`.  
   - External endpoints: `https://ipwho.is/`, `https://ipapi.co/json/`, `https://api.met.no/weatherapi/locationforecast/2.0/compact`.  
   - This exposes traffic to MITM and spoofing, and terms-of-service for some APIs expect sane client behavior.

2. **HTTP/JSON heavy operations with large dynamic allocations and no hard failure strategy**  
   - `WeatherClient::parseResponse` uses `DynamicJsonDocument` with capacity `50000` plus STL `std::map` and `std::vector`.  
   - On a small heap, repeated weather updates plus AsyncWebServer and WiFi stacks can cause fragmentation or allocation failures.  
   - Errors are logged but not propagated to UI in a way that guides the user; 24/7 operation under low memory is uncertain.

3. **WiFi reconfiguration of DNS on every geolocation fetch**  
   - `GeoLocationClient::fetchLocation` calls `WiFi.config(..., 8.8.8.8, 8.8.4.4)` on each call.  
   - This may override user/router DHCP DNS expectations and could interact poorly with other network features (e.g., captive portals or local services).

4. **AsyncWebServer / DNSServer lifecycle and heap usage**  
   - Captive portal starts `DNSServer` + `AsyncWebServer` with a substantial HTML wizard.  
   - `WiFiManager::freeWebServerMemory()` cleans up, but it is only called on successful WiFi connection; in failure states or partial flows, web server may persist longer than needed.

5. **Limited resilience around repeated WiFi failures**  
   - WiFi state machine retries `WIFI_MAX_RETRIES` and then re-enters AP mode. For unstable networks, this can create oscillations between AP and STA without any backoff strategy beyond fixed delays.

6. **WeatherService FreeRTOS task creation without global coordination**  
   - `xTaskCreate` of `WeatherService::fetchTask` uses 8 KB stack; if heap is low, task creation fails and sets state to `ERROR`, but there is no central monitor to stop repeated attempts or surface a clear UI warning.

7. **No explicit handling for RTC / time drift beyond NTP**  
   - NTP enable flag exists and is wired through device config, but full NTP implementation and long-term drift handling are not shown in this codebase (may exist elsewhere).  
   - Assumes NTP sync will “just work”; no backoff or fallback if it doesn’t.

8. **Power management not enforced**  
   - `SLEEP_TIMEOUT_MS` and `DEEP_SLEEP_TIMEOUT_MS` in `config.h` are not used in `main.cpp`.  
   - Device will run display and sensors continuously, increasing power draw and potential OLED burn-in in 24/7 use.

9. **Minor UX inconsistencies between modes**  
   - Some modes (e.g., sensors, WiFi info, weather screens) rely on long-press to escape back to menu, but behavior can differ when shake and touch interactions occur at the same time.

### 1.3 Commercial Release Readiness

**Verdict: Not yet safe for commercial release.**

Reasons:
- **Security posture is prototype-grade** (`setInsecure()` TLS, forced DNS, no OTA security model visible in this repo).  
- **Memory behavior under stress** (low heap, long runtimes) is not clearly bounded; heap-heavy weather/geo code and AsyncWebServer may degrade over weeks.
- **Power/thermal/display aging** not actively managed; constants exist but are unused.

With targeted hardening (see improvement plan), this can be raised to commercial-grade.

---

## 2. Detailed Findings

### 2.1 Critical Bugs / Undefined Behavior

**Finding C1 – TLS disabled for all HTTPS requests**  
- **Files:**  
  - `lib/WeatherService/GeoLocationClient.cpp`  
  - `lib/WeatherService/WeatherClient.cpp`
- **Details:**  
  - `WiFiClientSecure client; client.setInsecure();` disables certificate verification.  
  - All communication to external services is vulnerable to MITM and spoofing.
- **Real-world scenario:**  
  - An attacker on the same WiFi could impersonate `ipwho.is` or `api.met.no` and return bogus location/forecast or exploit JSON parsing.  
  - For a commercial product advertising online services, this is unacceptable.
- **Severity:** **Critical**

**Finding C2 – Potential use-after-free / stale pointers around web interface (theoretical)**  
- **Files:**  
  - `lib/WiFiManager/WiFiManager.cpp`  
  - `lib/WiFiManager/WebInterface.cpp/.h`
- **Details:**  
  - `WiFiManager::freeWebServerMemory()` deletes `_webInterface`, `_webServer`, `_dnsServer` and sets them to `nullptr`.  
  - `WebInterface` only holds a raw pointer to `WiFiManager`; AsyncWebServer callbacks capture `this` for the lifetime of the server.  
  - Current logic only deletes server when WiFi is connected (and AP is no longer used). There is no evidence of server deletion while requests are still in flight, so this is **likely safe** in current flows, but fragile if future modifications move `freeWebServerMemory()` earlier.
- **Real-world scenario:**  
  - If future changes call `freeWebServerMemory()` from another context while requests are running, callbacks may fire on a deleted `WebInterface`.  
  - Today, this is not triggered by visible code, but the pattern is fragile.
- **Severity:** **Medium** (latent risk, not currently an active bug)

No clear undefined behavior (UB) like out-of-bounds array access or null dereference was observed in the inspected modules.

---

### 2.2 Reliability & Stability Risks

**Finding R1 – Heap-heavy operations in WeatherClient**  
- **File:** `lib/WeatherService/WeatherClient.cpp`  
- **Why it’s a problem:**  
  - `DynamicJsonDocument doc(50000);` plus `std::map` and `std::vector` for up to ~4 days of data.  
  - Combined with AsyncWebServer, MQTT (if added later), and WiFi stacks, this can lead to fragmentation and allocation failures on 24/7 devices.
- **Scenario:**  
  - After many hours/days of operation, free heap drops or fragments; weather update fails to allocate doc; parse fails repeatedly; user sees no weather updates and only serial logs.  
  - In worst case, memory corruption from library code can reboot the device.
- **Severity:** **High**

**Finding R2 – No global rate limiting or backoff for repeated weather/geolocation failures**  
- **Files:**  
  - `lib/WeatherService/WeatherService.cpp`  
  - `lib/WeatherService/GeoLocationClient.cpp`  
- **Details:**  
  - WeatherService uses `MAX_RETRIES = 3` and `RETRY_DELAY_SECS = 30`, but does not distinguish between server-side (429) and client-side / connectivity errors — despite WeatherClient exposing `wasRateLimited()`.  
  - Geo client will repeatedly call external services on every scheduled update if enabled and WiFi is flapping.
- **Scenario:**  
  - In a poor WiFi environment, device hammers APIs with retries every ~30 seconds, potentially hitting rate limits and being blacklisted.  
  - No exponential backoff or long-term cool-down present.
- **Severity:** **Medium–High**

**Finding R3 – WiFi reconnection oscillation between AP and STA**  
- **File:** `lib/WiFiManager/WiFiManager.cpp`  
- **Details:**  
  - On boot, if configured network is not reachable, `WiFiManager` tries `WIFI_MAX_RETRIES` (3) with 30s timeout, then goes to `FAILED` and calls `startCaptivePortal()`.  
  - In `DISCONNECTED`, if `now - _lastConnectionAttempt > WIFI_RECONNECT_DELAY (30s)`, it calls `connect()` again.  
  - There is no notion of “remember that this network has been permanently failing for hours; stop flipping into AP unless user intervenes.”
- **Scenario:**  
  - User moves device out of WiFi coverage; device alternates between AP and STA states, broadcasting SSID and consuming more power.  
  - Over days, this is annoying and may exhaust some routers.
- **Severity:** **Medium**

**Finding R4 – No watchdog integration or loop-time guard rails**  
- **File:** `src/main.cpp`  
- **Details:**  
  - Main loop measures `maxLoopWorkUs` and logs every 10 s, which is good instrumentation.  
  - However, no action is taken if work time spikes; there is no software watchdog resetting subsystems or soft reboot policy.
- **Scenario:**  
  - A future feature accidentally adds blocking code (e.g., long HTTP GET) in the loop; ESP32 WDT may still kick in, but you have no graceful diagnostic or recovery path at application level.
- **Severity:** **Medium**

**Finding R5 – `ESP.restart()` used after short delay in factory reset / setup reset**  
- **File:** `lib/WiFiManager/WiFiManager.cpp`  
- **Details:**  
  - `factoryReset()` and `resetSetupWizard()` perform NVS operations and then `delay(500); ESP.restart();`.  
  - If a FreeRTOS task (WeatherService) or AsyncWebServer is mid-HTTP/JSON, restart could happen while they are still using NVS or heap.
- **Scenario:**  
  - User triggers factory reset while weather fetch task is running; in practice ESP32 ROM handles reset robustly, but this is a rough edge; potential NVS inconsistency if writes were mid-flight.
- **Severity:** **Low–Medium**

---

### 2.3 Security & Networking Concerns

**Finding S1 – TLS not verified (`setInsecure()`)**  
- **Files:** `GeoLocationClient.cpp`, `WeatherClient.cpp`  
- **Severity:** **Critical** (already described as C1).

**Finding S2 – Forced DNS servers in GeoLocationClient**  
- **File:** `lib/WeatherService/GeoLocationClient.cpp`  
- **Details:**  
  - `WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), 8.8.8.8, 8.8.4.4);` executed before each geolocation request.  
  - Overrides DHCP-provided DNS without user awareness; might break corporate or hotspot environments.
- **Scenario:**  
  - Device is deployed in enterprise WiFi requiring internal DNS; forced 8.8.8.8/8.8.4.4 may fail, breaking all outbound HTTP and possibly violating policy.
- **Severity:** **High**

**Finding S3 – No credential encryption in NVS**  
- **Files:** `WiFiManager.cpp` (`loadCredentials`, `storeCredentials`)  
- **Details:**  
  - WiFi SSID/password stored in plain text in NVS.  
  - On ESP32 this is common, but for a commercial product, secure boot & flash encryption should be considered.
- **Scenario:**  
  - Physical attacker with access to flash could read credentials.  
  - For consumer device this may be acceptable, but needs explicit product decision.
- **Severity:** **Medium** (product/market dependent)

**Finding S4 – No visible OTA or update authenticity mechanism**  
- **Files:** none in this repo  
- **Details:**  
  - No OTA implementation in the code inspected. If OTA will exist, it must enforce signed firmware / authenticity.
- **Scenario:**  
  - An insecure OTA path later added on top of this stack would be high risk if not designed carefully.
- **Severity:** **N/A yet**, but highlight for future work.

---

### 2.4 Performance & Resource Issues

**Finding P1 – Heavy JSON processing and C++ containers on constrained heap**  
- **Files:** `WeatherClient.cpp`, `GeoLocationClient.cpp`, `WeatherService.cpp`  
- **Details:**  
  - Large `DynamicJsonDocument`, `std::map`, `std::vector`, plus multiple string copies.  
  - Suitable for prototypes but should be tested under minimum-heap worst-case (AsyncWebServer active, large NVS usage, WiFi tasks).
- **Severity:** **High** (for long-term uptime under resource pressure).

**Finding P2 – Continuous display activity, no dimming or sleep strategy**  
- **File:** `src/main.cpp`, `DisplayManager`  
- **Details:**  
  - Display is always powered; brightness is user-controlled but there is no auto-dim or sleep in idle states.  
  - OLED lifetime and power consumption will suffer.
- **Scenario:**  
  - 24/7 operation at high brightness may cause ghosting/burn-in and reduce panel life.
- **Severity:** **Medium**

**Finding P3 – `delay(10)` in loop without dynamic adjustment**  
- **File:** `src/main.cpp`  
- **Details:**  
  - 10 ms sleep ensures CPU isn’t saturated, but may be longer than necessary when animations or sensor sampling would benefit from tighter timing.  
  - Health logs show `maxLoopWorkUs` but there is no adaptation.
- **Severity:** **Low** (UX trade-off rather than bug).

---

### 2.5 UX / UI Problems

**Finding U1 – Mode-exit behavior not fully unified across input types**  
- **Files:** `src/main.cpp` (`onButtonEvent`, `onTouchEvent`, `onMotionEvent`)  
- **Details:**  
  - Long-press button from most modes returns to menu; touch long-press does similar from animations; shake from menu either exits to animations or navigates back.  
  - Some modes (WiFi setup/info, weather views, sensors) rely exclusively on long-press/back path; if input is mis-detected (e.g., noisy encoder), user may feel “stuck” temporarily.
- **Severity:** **Low–Medium**

**Finding U2 – No explicit on-device feedback for network/privacy failures**  
- **Files:** `main.cpp`, `WeatherService`, `WiFiManager`, `WebInterface`  
- **Details:**  
  - Setup wizard explains privacy/consent well, but once device leaves captive portal, on-device UI (OLED) only shows status icons / views driven by you (not fully visible in snippet).  
  - If weather or location fail (rate limit, TLS, DNS), user has no obvious explanation without serial log.
- **Severity:** **Medium** (for polished product UX)

---

### 2.6 Architectural & Maintainability Issues

**Finding A1 – Global singletons for all subsystems**  
- **File:** `src/main.cpp`  
- **Details:**  
  - `DisplayManager display; InputManager input; MotionSensor motion; SensorHub sensors; WiFiManager wifi; WeatherService weatherService;` are globals.  
  - This simplifies Arduino-style code but couples modules tightly and makes unit testing or alternate compositions difficult.
- **Severity:** **Medium** (architectural, not a bug)

**Finding A2 – Mixed responsibility in `main.cpp`**  
- **File:** `src/main.cpp`  
- **Details:**  
  - `main.cpp` owns: application modes; menu wiring; interactions with WiFi/weather; Pomodoro logic; random animations; input/touch/motion event policies.  
  - Over 1600 lines; hard to reason about interactions and maintain over time.
- **Severity:** **Medium**

**Finding A3 – No central error/state reporting layer**  
- **Files:** Many modules use `Serial.println` for diagnostics.  
- **Details:**  
  - Errors in WiFi, Weather, Geo, Sensors, Motion are logged to serial only; no structured error bus to drive UI or remote diagnostics.
- **Severity:** **Medium** (limits debuggability in field).

---

## 3. Assumption Validation

Below are key assumptions visible in the code, with their safety and failure impact.

1. **WiFi always reconnects eventually**  
   - **Assumption locations:** `WiFiManager::update`, `WeatherService::update`, Geo/Weather clients.  
   - **Safety:** Partially safe; there is logic for retries and AP mode, but no long backoff or user-configurable policy.  
   - **If violated:** Frequent retries + AP toggling; weather and NTP remain offline; user may never see weather/clock features but device otherwise functions.

2. **DNS and external APIs remain reachable and stable**  
   - **Assumption locations:** Geo/Weather clients, `WiFi.config` forced DNS.  
   - **Safety:** Not entirely safe; enterprise/hotspot/filtered networks may block 8.8.8.8 or these APIs.  
   - **If violated:** Repeated timeouts, rate-limit errors; features silently broken apart from serial logs.

3. **Heap is sufficient and not badly fragmented**  
   - **Assumption locations:** Weather JSON parsing, AsyncWebServer, Dynamic allocations in Display/Input/WiFi/Weather.  
   - **Safety:** At risk for long uptimes with added features.  
   - **If violated:** Task creation or JSON parsing fails, forcing weather into extended `ERROR` state; worst case random crashes from library internals.

4. **Loop execution time stays well below WDT limits**  
   - **Assumption locations:** `loop()` uses `delay(10)` and instrumentation only.  
   - **Safety:** Currently safe given non-blocking design.  
   - **If violated (future changes):** WDT resets occur; health logs may show high `maxLoopWorkUs` but no auto-mitigation.

5. **WeatherService background task does not interfere with main loop or other tasks**  
   - **Assumption:** FreeRTOS scheduler plus low task priority keep main loop responsive.  
   - **Safety:** Likely safe as long as memory is available and HTTP operations finish within timeouts.  
   - **If violated:** Scheduler may be starved during blocking WiFi/TLS operations; UI may stutter during long HTTP requests.

6. **NVS operations are atomic enough under concurrent tasks**  
   - **Assumption:** `Preferences` library handles flash writes correctly even if other tasks are running.  
   - **Safety:** Generally safe but not explicitly coordinated.  
   - **If violated:** NVS corruption or inconsistent cached config; worst-case boot into inconsistent WiFi/device state.

7. **Touch and motion are optional**  
   - **Assumption:** `TOUCH_ENABLED` may be false; motion sensor may fail to init (`[WARN] Motion sensor not found`).  
   - **Safety:** Well-handled; code degrades gracefully.

8. **User will finish captive portal setup or re-run it if needed**  
   - **Assumption:** If `setupComplete` is false, user connects to AP and completes wizard.  
   - **Safety:** Reasonable, but if user never completes wizard and walks away, device remains as AP indefinitely.

---

## 4. Modular Improvement Plan

Each step is **independent** and can be shipped separately while keeping firmware stable.

### Step 1 – Harden Networking & TLS

- **Goal:** Move from `setInsecure()` to a minimally secure, production-appropriate TLS posture and sane DNS handling.
- **Files/modules:**  
  - `GeoLocationClient.*`  
  - `WeatherClient.*`  
  - `WiFiManager.*`
- **Actions:**  
  - Replace `client.setInsecure()` with either:  
    - CA bundle-based validation for the specific hosts, or  
    - Certificate pinning (store SHA-256 fingerprint or PEM in flash).  
  - Remove or make optional the forced `WiFi.config(..., 8.8.8.8, 8.8.4.4)` step. Only override DNS if config explicitly allows it.  
  - Add robust error classification (timeout, TLS failure, 4xx, 5xx) and expose via `WeatherError` / a new `GeoError` enum.
- **Expected benefit:**  
  - Stronger security, better behavior in various network environments, fewer surprises in the field.
- **Risk level:** **Medium** (network regressions possible; test thoroughly).  
- **How to verify:**  
  - Run against known CA-signed endpoints; test with wrong certificates to ensure connection is rejected.  
  - Test in networks with non-Google DNS, captive portals, and offline conditions.

### Step 2 – Bound and Optimize Memory Usage for Weather/Geo

- **Goal:** Ensure weather/geolocation features behave gracefully under low heap and do not destabilize the system.
- **Files/modules:**  
  - `WeatherClient.*`  
  - `GeoLocationClient.*`  
  - `WeatherService.*`
- **Actions:**  
  - Replace `std::map`/`std::vector` with fixed-size C-style arrays or small containers with hard caps (e.g., max N hourly samples per day).  
  - Reduce `DynamicJsonDocument` size and use filtered deserialization more aggressively (already using filter, but capacity may be over-provisioned).  
  - Add explicit heap checks (`ESP.getFreeHeap()`) before starting fetch; if below threshold (e.g., < 50KB), skip update and set an error state.  
  - Implement exponential backoff when weather/geo repeatedly fail (respect HTTP 429 via `wasRateLimited()`).
- **Expected benefit:**  
  - Predictable memory footprint; lower fragmentation; improved long-term reliability.
- **Risk level:** **Medium–High** (changes algorithmic structure).  
- **How to verify:**  
  - Run long-duration tests with health logs; monitor `freeHeap`, `largestFree`, and weather success rate.  
  - Use synthetic low-heap conditions (e.g., allocate large buffer in test build) to ensure graceful degradation.

### Step 3 – Improve WiFi State Machine & AP Behavior

- **Goal:** Avoid oscillation between AP and STA, minimize unnecessary AP broadcast, and improve UX around connectivity problems.
- **Files/modules:**  
  - `WiFiManager.*`  
  - `WebInterface.*`  
  - `main.cpp` (WiFi-related views)
- **Actions:**  
  - Add explicit "offline" mode: after a configurable number of failed reconnections over a long window, stop trying until the user triggers WiFi menu or setup wizard again.  
  - Implement an AP auto-shutdown timeout using existing `_apAutoShutdownMs`; after e.g. 10 minutes without wizard completion, stop AP and indicate offline state on OLED.  
  - Surface WiFi error reasons to UI: add simple icons or messages on WiFi info screen, wired to `WiFiState` and last error.
- **Expected benefit:**  
  - Better behavior in unstable/offline environments; reduced power and RF noise; clearer UX.
- **Risk level:** **Medium**.  
- **How to verify:**  
  - Test with no WiFi available; confirm AP timeout and offline indication.  
  - Simulate wrong credentials and intermittent networks.

### Step 4 – Power Management & Display Lifespan

- **Goal:** Introduce basic power management to protect the OLED and reduce consumption.
- **Files/modules:**  
  - `src/main.cpp`  
  - `DisplayManager.*`  
  - `config.h`
- **Actions:**  
  - Implement an **idle timer** at app level: after N minutes without user input/motion, dim display; after M minutes, turn display off or enter light sleep.  
  - Use existing `SLEEP_TIMEOUT_MS` and `DEEP_SLEEP_TIMEOUT_MS` or new names to match behavior.  
  - On wake (button/touch/motion), restore brightness and resume animations.
- **Expected benefit:**  
  - Longer OLED life, better 24/7 thermal and power profile.
- **Risk level:** **Low–Medium**.  
- **How to verify:**  
  - Run long idle tests; confirm transitions and wake behavior from button, motion, or touch.

### Step 5 – Central Error/State Reporting & UI Feedback

- **Goal:** Give non-technical users clear visibility into network/weather issues without serial logs.
- **Files/modules:**  
  - `main.cpp` (weather view, WiFi info view)  
  - `WeatherService.*`  
  - `WiFiManager.*`  
  - `DisplayManager.*` (optional icon helpers)
- **Actions:**  
  - Define a small global “system status” struct (current WiFi state, last weather error, last NTP error).  
  - Update it from WiFiManager, WeatherService, and future NTP client.  
  - Render human-readable messages in WiFi info and weather view screens (e.g., `"Weather: ERROR (rate limited)"`).
- **Expected benefit:**  
  - Significantly better UX and supportability; easier remote debugging.
- **Risk level:** **Low–Medium**.  
- **How to verify:**  
  - Induce network/API failures and confirm appropriate messages appear.

### Step 6 – Refactor `main.cpp` and Reduce Global Coupling

- **Goal:** Improve maintainability and extensibility for future features (OTA, logging, new modes).
- **Files/modules:**  
  - `src/main.cpp` (split); potential new `AppController`, `UiController`, `NetworkController` modules.
- **Actions:**  
  - Extract **mode-specific logic** into separate compilation units: e.g., `ModeAnimations`, `ModeSensors`, `ModeWeather`, `ModeClock`, `ModePomodoro`.  
  - Replace raw globals with a small `AppContext` struct passed into these modules (pointers to display, wifi, weatherService, etc.).  
  - Keep `setup()` and `loop()` as thin orchestrators.
- **Expected benefit:**  
  - Easier to reason about interactions; simpler to add OTA or remote logging; cleaner tests.
- **Risk level:** **Medium–High** (large refactor).  
- **How to verify:**  
  - Comprehensive regression testing of all modes, menu navigation, and interactions.

---

## 5. Final Verdict

### 5.1 Commercial Robustness

Current status: **Near-commercial but not yet sufficient for a security-conscious, 24/7 commercial device.**

- Functional behavior, UI, and modular structure are strong.  
- The main gaps are **security (TLS, DNS), long-term memory behavior, and power management**.

### 5.2 Minimum Required Work Before Shipping

At minimum, before commercial release:

1. **Secure external HTTPS usage (Step 1).**  
2. **Bound and test memory usage for weather/geolocation (Step 2).**  
3. **Improve WiFi state handling to avoid pathological reconnect/AP oscillations (Step 3).**  
4. **Add at least basic OLED power management (dim/off on long idle) (Step 4).**  
5. **Expose error states to UI so users understand when online features are degraded (Step 5).**

With these in place and validated on hardware over multi-week burn-in tests, the firmware can reasonably be considered commercial-grade.

### 5.3 Optional but Recommended Improvements

- **Refactor main orchestration** (Step 6) for long-term evolution.  
- **Add OTA with signed firmware and rollback** once security posture is solid.  
- **Introduce a lightweight system watchdog layer** on top of ESP32 WDT, with controlled soft-restart and reason logging.  
- **Persist simple diagnostic counters in NVS** (boot count, last reset reason, last weather error) for field troubleshooting.

---

**End of Firmware Audit Report**
