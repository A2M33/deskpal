#pragma once

// weather.cpp includes WiFi.h for the plain-HTTP WiFiClient used by the
// location lookup. Nothing on the host ever connects; see HTTPClient.h.

class WiFiClient {};
