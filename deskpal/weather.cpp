#include "weather.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"

bool lookupLocation(Location& out) {
  // ip-api.com is free without a key, but only over plain HTTP (HTTPS is for
  // paying customers). The request carries nothing but the IP address the
  // server sees anyway; the answer is a city-level guess, fine for weather.
  WiFiClient client;
  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, "http://ip-api.com/json/?fields=status,city,lat,lon")) {
    Serial.println("[location] bad URL");
    return false;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("[location] HTTP error %d\n", status);
    http.end();
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err || strcmp(doc["status"] | "", "success") != 0 || !doc["lat"].is<float>()) {
    Serial.printf("[location] bad reply: %s\n", err ? err.c_str() : "lookup failed");
    return false;
  }

  out.latitude = doc["lat"];
  out.longitude = doc["lon"];
  strlcpy(out.place, doc["city"] | "", sizeof(out.place));
  out.known = true;
  Serial.printf("[location] %s (%.2f, %.2f) from IP address\n", out.place, out.latitude,
                out.longitude);
  return true;
}

bool fetchWeather(const Location& where, WeatherData& data) {
  // timezone=auto makes Open-Meteo add utc_offset_seconds for the location,
  // which sets the clock when no TIMEZONE is configured.
  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,wind_speed_10m&wind_speed_unit=kmh"
           "&timezone=auto",
           (double)where.latitude, (double)where.longitude);

  // The client must outlive the HTTPClient that uses it, so it is declared first.
  WiFiClientSecure client;
  // Skip HTTPS certificate checks. The connection is still encrypted, but not
  // verified. Fine for public weather data; see the README for details.
  client.setInsecure();

  HTTPClient http;
  // HTTP/1.0 stops the server from sending a "chunked" reply, so we can feed
  // the raw stream straight into ArduinoJson without buffering it in RAM.
  http.useHTTP10(true);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    Serial.println("[weather] bad URL");
    data.lastFetchFailed = true;
    return false;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("[weather] HTTP error %d\n", status);
    http.end();
    data.lastFetchFailed = true;
    return false;
  }

  // The filter tells ArduinoJson which fields to keep; everything else in the
  // reply is skipped while reading, which keeps memory use tiny.
  JsonDocument filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["wind_speed_10m"] = true;
  filter["utc_offset_seconds"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  JsonObject current = doc["current"];
  if (err || current.isNull() || !current["temperature_2m"].is<float>()) {
    Serial.printf("[weather] bad reply: %s\n", err ? err.c_str() : "missing fields");
    data.lastFetchFailed = true;
    return false;
  }

  data.tempC = current["temperature_2m"];
  data.code = current["weather_code"] | 0;
  data.windKmh = current["wind_speed_10m"] | 0.0f;
  data.utcOffsetKnown = doc["utc_offset_seconds"].is<int32_t>();
  data.utcOffsetSeconds = doc["utc_offset_seconds"] | 0;
  strlcpy(data.place, where.place, sizeof(data.place));
  data.valid = true;
  data.lastFetchFailed = false;
  data.updatedAtMs = millis();
  Serial.printf("[weather] %.1f C, code %d, wind %.1f km/h\n", data.tempC, data.code, data.windKmh);
  return true;
}

// Codes come from the WMO table in the Open-Meteo docs.
const char* weatherText(int code) {
  switch (code) {
    case 0: return "Clear sky";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45:
    case 48: return "Fog";
    case 51:
    case 53:
    case 55: return "Drizzle";
    case 56:
    case 57: return "Icy drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66:
    case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80:
    case 81:
    case 82: return "Rain showers";
    case 85:
    case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96:
    case 99: return "Storm and hail";
    default: return "Unknown";
  }
}

WeatherIcon weatherIcon(int code) {
  if (code <= 1) return WeatherIcon::Sun;
  if (code == 2) return WeatherIcon::PartCloud;  // partly cloudy
  if (code <= 48) return WeatherIcon::Cloud;     // overcast and fog
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WeatherIcon::Snow;
  return WeatherIcon::Rain;                   // drizzle, rain, showers, storms
}
