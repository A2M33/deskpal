#include "weather.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config.h"

bool fetchWeather(WeatherData& data) {
  char url[220];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,wind_speed_10m&wind_speed_unit=kmh",
           (double)WEATHER_LATITUDE, (double)WEATHER_LONGITUDE);

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
  if (code <= 48) return WeatherIcon::Cloud;  // cloudy and fog
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WeatherIcon::Snow;
  return WeatherIcon::Rain;                   // drizzle, rain, showers, storms
}
