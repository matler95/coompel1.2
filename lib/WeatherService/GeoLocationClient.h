#ifndef GEO_LOCATION_CLIENT_H
#define GEO_LOCATION_CLIENT_H

#include <Arduino.h>

// Geographic location data from IP-based geolocation
struct GeoLocation {
    float latitude = 0.0f;
    float longitude = 0.0f;
    char city[32] = {0};
    char country[3] = {0};  // ISO 3166-1 alpha-2 code
    bool valid = false;
};

// Client for fetching approximate location using IP address
class GeoLocationClient {
public:
    GeoLocationClient();

    // Fetch location based on device's public IP address
    // Returns true on success, false on failure
    // Location data is stored in the provided GeoLocation struct
    bool fetchLocation(GeoLocation& location);

private:
    // Parse JSON response from geolocation API
    bool parseResponse(const String& json, GeoLocation& location);

    // API endpoint (no key required) - using ipwho.is
    // Automatically uses client's public IP when no IP is specified
    static constexpr const char* API_URL = "https://ipwho.is/";

    // HTTP timeout in milliseconds
    static constexpr int TIMEOUT_MS = 10000;
};

#endif // GEO_LOCATION_CLIENT_H
