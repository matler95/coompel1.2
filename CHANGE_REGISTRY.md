# Firmware Change Registry

This document tracks incremental hardening steps applied to the firmware.

---

## [2026-01-16] Step A – Idle-based Display Power Management

**Files touched:**
- `src/main.cpp`

**Changes:**
- Added global idle tracking variables:
  - `lastUserActivity` and `displaySleeping`.
- Introduced `onUserActivity()` helper:
  - Updates `lastUserActivity` on any user interaction.
  - Wakes the OLED (calls `display.setPower(true)` and reapplies brightness) if it was previously turned off due to inactivity.
- Hooked `onUserActivity()` into:
  - Button events (`onButtonEvent`).
  - Touch events (`onTouchEvent`, when `TOUCH_ENABLED`).
  - Motion events (`onMotionEvent`).
  - Menu activity (`resetMenuTimeout`).
- Initialized `lastUserActivity` during `setup()` after core subsystems are ready.
- Added idle check in `loop()`:
  - If the device has been idle for at least `SLEEP_TIMEOUT_MS` (from `config.h`) and the display is not already sleeping, it logs a `[POWER]` message, turns the display off via `display.setPower(false)`, and sets `displaySleeping = true`.

**Behavioral impact:**
- During normal interaction, the display behavior is unchanged.
- If the device is left untouched for `SLEEP_TIMEOUT_MS` (default 5 minutes), the OLED is powered off to reduce burn-in and power use.
- Any subsequent user activity (button, touch, motion, menu navigation) wakes the display and restores brightness.

**Notes:**
- `DEEP_SLEEP_TIMEOUT_MS` is not yet used; deep-sleep integration will be considered in a later step once wake sources and UX are defined.

---

## [2026-01-16] Step B – Networking/TLS Structure Hardening (Geo & Weather)

**Files touched:**
- `lib/WeatherService/GeoLocationClient.cpp`
- `lib/WeatherService/WeatherClient.cpp`

**Changes:**
- Removed forced Google DNS override in `GeoLocationClient::fetchLocation`:
  - Deleted the `WiFi.config(..., 8.8.8.8, 8.8.4.4)` block that previously changed DNS on every geolocation request.
  - The device now uses whatever DNS servers are provided by the network (DHCP or static config).
- Introduced small internal TLS configuration helpers:
  - In `GeoLocationClient.cpp`:
    - Added an anonymous-namespace function `configureSecureClient(WiFiClientSecure& client)` and replaced direct `client.setInsecure()` with a call to this helper.
  - In `WeatherClient.cpp`:
    - Added the same-style `configureSecureClient(WiFiClientSecure& client)` helper and replaced the inline `client.setInsecure()` call.

**Behavioral impact:**
- DNS behavior:
  - Geo-location no longer forces public Google DNS; DNS resolution now respects the network’s own DNS configuration, which is safer and more compatible with enterprise/hotspot environments.
- TLS behavior (current state):
  - TLS verification is still disabled internally (the helpers currently call `client.setInsecure()`), so behavior with respect to certificate checking is unchanged.
  - However, all TLS configuration for Geo and Weather is now funneled through a single helper in each translation unit, making it straightforward to switch to CA-based validation or certificate pinning in a future change without touching the HTTP logic.

**Notes / Next steps:**
- To fully harden TLS, the `configureSecureClient` helpers should be updated to use either:
  - `client.setCACert(<PEM>)` with a pinned CA/certificate for the specific hosts, or
  - Fingerprint-based pinning using known-good certificate hashes.
- That change will require embedding or provisioning certificates; the current step focuses on removing the DNS override and centralizing TLS configuration.

### [2026-01-16] Step B2 – TLS CA Hook for Geo/Weather Clients

**Files updated:**
- `lib/WeatherService/GeoLocationClient.cpp`
- `lib/WeatherService/WeatherClient.cpp`

**Changes:**
- Made TLS behavior for external HTTPS APIs configurable via compile-time flags:
  - In `GeoLocationClient.cpp`:
    - `configureSecureClient(WiFiClientSecure& client)` now:
      - Uses `client.setCACert(GEO_ROOT_CA_PEM)` when `GEO_TLS_USE_CA_CERT==1` is defined at build time and an external `GEO_ROOT_CA_PEM` symbol (PEM-encoded CA/cert) is provided.
      - Falls back to `client.setInsecure()` otherwise (preserving existing default behavior).
  - In `WeatherClient.cpp`:
    - `configureSecureClient(WiFiClientSecure& client)` now:
      - Uses `client.setCACert(WEATHER_ROOT_CA_PEM)` when `WEATHER_TLS_USE_CA_CERT==1` is defined and an external `WEATHER_ROOT_CA_PEM` symbol is provided.
      - Falls back to `client.setInsecure()` otherwise.

**Behavioral impact:**
- **Default build (no new macros defined):**
  - Behavior is unchanged: both GeoLocation and Weather clients still call `setInsecure()` and skip certificate verification.
- **Hardened build (once configured by integrator):**
  - By defining `GEO_TLS_USE_CA_CERT=1` and/or `WEATHER_TLS_USE_CA_CERT=1` at compile time and supplying appropriate PEM-encoded CA/cert data (`GEO_ROOT_CA_PEM`, `WEATHER_ROOT_CA_PEM`), the firmware will:
    - Perform real TLS certificate verification for the respective HTTPS endpoints using the provided CA/cert.
    - Reject connections with invalid or untrusted certificates instead of silently accepting them.

**Notes:**
- This step intentionally does **not** ship embedded CA material, as that depends on deployment choices (which CAs to trust, whether to use full-chain CA vs. leaf cert pinning, etc.).
- To fully harden TLS in a concrete product, you should:
  - Decide which endpoints and CAs to trust.
  - Add the corresponding PEM strings in flash (e.g., in a dedicated header or source file) to back `GEO_ROOT_CA_PEM` and `WEATHER_ROOT_CA_PEM`.
  - Enable the appropriate `*_TLS_USE_CA_CERT` macros in your build configuration.

---

## [2026-01-16] Step C – WiFi AP Auto-Shutdown Behavior

**Files touched:**
- `lib/WiFiManager/WiFiManager.cpp`

**Changes:**
- Introduced a default AP auto-shutdown interval:
  - Added `DEFAULT_AP_AUTO_SHUTDOWN_MS` (15 minutes) in `WiFiManager.cpp`.
  - In `WiFiManager::init()`, if `_apAutoShutdownMs` is still `0` (no explicit configuration), it is set to this default.
- Wired `_apStartTime` to AP lifecycle:
  - In `startCaptivePortal()`, `_apStartTime` is set to `millis()` after the AP is started.
- Implemented AP auto-shutdown in `handleAPMode()`:
  - While in AP mode, if `_apAutoShutdownMs > 0` and `_apStartTime > 0`, the code checks whether the elapsed time since `_apStartTime` exceeds `_apAutoShutdownMs`.
  - When the timeout is reached:
    - Logs a `[WiFi] AP auto-shutdown timeout reached, stopping captive portal` message.
    - Calls `stopCaptivePortal()` to stop the AP and DNS server.
    - Calls `freeWebServerMemory()` to release the AsyncWebServer/DNSServer/WebInterface heap.
    - Sets `_state` to `WiFiState::IDLE` and triggers `WiFiEvent::DISCONNECTED`.
    - Resets `_apStartTime` to `0`.

**Behavioral impact:**
- If the setup wizard/captive portal is left running with no user completing configuration:
  - After the configured timeout (default 15 minutes if not overridden by `setAPAutoShutdownMs()`), the AP and captive portal will automatically shut down and the device will transition to an idle/offline WiFi state.
  - This reduces unnecessary AP broadcasting and frees heap used by the web server.
- If you want the old behavior (AP never times out), you can explicitly call `wifi.setAPAutoShutdownMs(0)` somewhere in your setup code.

**Notes / Next steps:**
- Additional WiFi behavior hardening (e.g., long-term offline/backoff mode for repeated STA reconnect failures) can be implemented later using the existing state machine and timing fields, but is not yet changed in this step.

---

## [2026-01-16] Step D – Central Weather/WiFi Status & UI Feedback

**Files touched:**
- `src/main.cpp`

**Changes:**
- Introduced a lightweight `SystemStatus` struct in `main.cpp` to aggregate key runtime state for UI:
  - Tracks `WiFiState` and whether WiFi is currently connected.
  - Tracks `WeatherState`, `lastWeatherError` (from `WeatherService`), and `lastWeatherUpdateTs` (seconds since boot of last successful weather update).
- Hooked `SystemStatus` up to existing callbacks:
  - Updated `onWiFiEvent(WiFiEvent event)` to keep `systemStatus.wifiState` and `systemStatus.wifiConnected` in sync with `WiFiManager`.
  - Registered a new weather event callback via `weatherService.setEventCallback(onWeatherEvent)` in `setup()` and implemented `onWeatherEvent(WeatherEvent event)` to:
    - Mirror the current `WeatherState` and `WeatherError` into `systemStatus`.
    - Record `lastWeatherUpdateTs` whenever `WeatherEvent::WEATHER_UPDATED` is received.
- Enhanced the weather view error path in `updateWeatherViewMode()`:
  - When in `WeatherState::ERROR`, the screen already shows `getErrorString()` and retry count.
  - Added handling for `WeatherState::CACHED` or `WeatherState::STALE` without valid data:
    - Shows "No fresh data" and, if available, a simple "Last ok: <seconds>s" line using `systemStatus.lastWeatherUpdateTs`.
- Kept WiFi info view structurally the same but now backed by up-to-date `WiFiState` from `WiFiManager` (via `systemStatus` and `wifi.getState()`), providing a clearer separation between "configured but offline" and actual connection state.

**Behavioral impact:**
- The WiFi info screen continues to show connection details when connected and a clear textual state (Idle / AP Mode / Connecting / Disconnected / Failed) when only credentials are stored but there is no active connection.
- The weather view now provides slightly richer context in non-happy-path situations:
  - Explicitly distinguishes between transient errors (`ERROR` with retry info) and stale-but-previously-valid data ("No fresh data" with last-known-good timestamp when available).
- This central status aggregation prepares the ground for any future on-device diagnostics or additional status views, without changing the public APIs of `WiFiManager` or `WeatherService`.

---

## [2026-01-16] Step E – Part 1: Extract Weather Modes

**Files added:**
- `include/SystemStatus.h`
- `lib/ModeWeather/ModeWeather.h`
- `lib/ModeWeather/ModeWeather.cpp`

**Files updated:**
- `src/main.cpp`

**Changes:**
- Introduced a shared `SystemStatus` definition in `include/SystemStatus.h` so that both `main.cpp` and new mode modules can use the same status struct type.
- In `main.cpp`:
  - Replaced the local `SystemStatus` struct definition with an instance `SystemStatus systemStatus;` using the shared header type.
  - Included the new headers `SystemStatus.h` and `ModeWeather/ModeWeather.h`.
  - Removed the in-file implementations of:
    - `updateWeatherViewMode()`
    - `updateWeatherAboutMode()`
    - `updateWeatherPrivacyMode()`
    which are now provided by `lib/ModeWeather/ModeWeather.cpp`.
- Created `ModeWeather` module:
  - `ModeWeather.h` declares the three weather-related update functions.
  - `ModeWeather.cpp` contains the exact logic previously in `main.cpp` for:
    - Weather forecast overview and detail pages.
    - Weather attribution (about) screen.
    - Weather privacy information screen.
  - Uses `extern` declarations to access the existing global singletons and state (`display`, `wifi`, `weatherService`, `systemStatus`, `weatherViewPage`) so behavior remains identical.

**Behavioral impact:**
- No user-visible behavior change is intended in this step:
  - `loop()` and the app mode state machine still call the same `updateWeather*` functions.
  - Screen content, timing, and interactions for all weather-related modes remain the same.
- This refactor is purely structural and prepares for further mode extractions (clock, pomodoro, sensors, animations) without touching business logic.

---

## [2026-01-16] Step B3 – TLS Mode Decision (Option A: CA-Based TLS for Geo + Weather)

**Files added:**
- `src/tls_certs.cpp`

**Files updated:**
- `platformio.ini`

**Changes:**
- Introduced a dedicated source file `src/tls_certs.cpp` that defines the CA certificate symbols used by the Geo and Weather TLS helpers:
  - `const char GEO_ROOT_CA_PEM[] PROGMEM`
  - `const char WEATHER_ROOT_CA_PEM[] PROGMEM`
  These are initialized with placeholder PEM blocks and are intended to be replaced by the integrator with the actual root or intermediate CA certificates that sign the production HTTPS endpoints:
  - Geo-location: `https://ipwho.is/`, `https://ipapi.co/json/`
  - Weather: `https://api.met.no/weatherapi/locationforecast/2.0/compact`
- Enabled CA-based TLS verification for the **release** build configuration:
  - In `[env:esp32c3_release]` of `platformio.ini`, added build flags:
    - `-DGEO_TLS_USE_CA_CERT=1`
    - `-DWEATHER_TLS_USE_CA_CERT=1`
  - These flags cause the existing `configureSecureClient` helpers to call `setCACert(GEO_ROOT_CA_PEM)` and `setCACert(WEATHER_ROOT_CA_PEM)` respectively instead of `setInsecure()`.

**Behavioral impact:**
- **Debug build (`env:esp32c3_dev`):**
  - Behavior is unchanged; TLS for Geo and Weather still uses `setInsecure()` and does not validate certificates. This is convenient for local development and experimentation, even with self-signed or intercepted traffic.
- **Release build (`env:esp32c3_release`):**
  - TLS for Geo and Weather now performs real certificate validation using the CA PEMs defined in `tls_certs.cpp`.
  - If `GEO_ROOT_CA_PEM` / `WEATHER_ROOT_CA_PEM` do not match the actual certificate chain presented by the servers (or if the placeholders are not replaced), HTTPS connections will fail with TLS errors and Geo/Weather will not update.

**Integrator actions required to complete hardening:**
- Replace the placeholder PEM blocks in `src/tls_certs.cpp` with the correct CA material **before** shipping a release build:
  - On a development machine, use a browser or `openssl s_client -showcerts` against each endpoint to obtain the certificate chain.
  - Identify the appropriate CA certificate (usually the issuing intermediate or root) and export it in PEM format.
  - Paste the full PEM, including `-----BEGIN CERTIFICATE-----` / `-----END CERTIFICATE-----` lines, into the corresponding string literal.
- Build and flash the `esp32c3_release` environment, then run live tests:
  - Verify that GeoLocation lookups succeed and that Weather updates work under normal conditions.
  - Temporarily point DNS to a test MITM/intercept proxy (or otherwise break TLS) to confirm that the firmware properly rejects invalid certificates and surfaces errors via the existing status reporting and diagnostics screen.

**Notes:**
- This step represents a concrete policy decision for production firmware: **release builds use CA-validated TLS for both Geo and Weather**. If future deployment scenarios require different trust anchors (e.g., private CAs), only `tls_certs.cpp` and possibly the build flags need to be adjusted.
- Because all TLS behavior is still funneled through the existing `configureSecureClient` helpers, it remains possible to add more advanced pinning strategies later (e.g., multiple CA certs, backup chains, or certificate fingerprint checks) without touching higher-level HTTP logic.
