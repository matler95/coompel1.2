#include <pgmspace.h>

// TLS CA certificates for GeoLocation and Weather APIs.
//
// IMPORTANT:
// - Replace the placeholder PEM strings below with the real CA certificates
//   that sign the production endpoints you use.
// - Keep them as root or intermediate CA certs, not leaf certs, unless you
//   intentionally want strict pinning.
// - PEM must include the BEGIN/END CERTIFICATE lines.
//
// Example process to obtain CA PEM (on a PC):
//   - Use a browser or `openssl s_client -showcerts` against the API host
//   - Identify the root/intermediate CA in the chain
//   - Export that certificate as PEM and paste it below.
//
// NOTE: The firmware expects these symbols when GEO_TLS_USE_CA_CERT==1
//       and/or WEATHER_TLS_USE_CA_CERT==1.

// GeoLocation APIs (ipwho.is, ipapi.co)
// Replace this placeholder with the CA cert that signs https://ipwho.is
// and https://ipapi.co/json/ (usually a widely used public CA).
const char GEO_ROOT_CA_PEM[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
REPLACE_WITH_REAL_GEO_CA_CERT_PEM
-----END CERTIFICATE-----
)PEM";

// Weather API (MET Norway locationforecast)
// Replace this placeholder with the CA cert that signs
// https://api.met.no/weatherapi/locationforecast/2.0/compact
const char WEATHER_ROOT_CA_PEM[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
REPLACE_WITH_REAL_WEATHER_CA_CERT_PEM
-----END CERTIFICATE-----
)PEM";
