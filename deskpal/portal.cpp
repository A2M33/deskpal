#include "portal.h"

#include <WiFiManager.h>

#include "config.h"
#include "settings.h"

namespace {

// Created on first use, so the web server costs no RAM on normal days.
WiFiManager* manager = nullptr;
WiFiManagerParameter* userParam = nullptr;
WiFiManagerParameter* tokenParam = nullptr;
WiFiManagerParameter* latParam = nullptr;
WiFiManagerParameter* lonParam = nullptr;

bool active = false;
bool saveRequested = false;
unsigned long openedAtMs = 0;

// Empty text means "not set" (NAN), so the location comes from the IP address again.
float parseCoordinate(const char* text) {
  while (*text == ' ') text++;
  if (*text == '\0') return NAN;
  char* end = nullptr;
  const float value = strtof(text, &end);
  return end == text ? NAN : value;
}

void formatCoordinate(float value, char* out, size_t len) {
  if (isnan(value)) out[0] = '\0';
  else snprintf(out, len, "%.4f", value);
}

void applyAndRestart() {
  // The Wi-Fi page fills in the ESP32's station config when you press Save;
  // read it back from there. An empty SSID means the form kept the old network.
  const String ssid = manager->getWiFiSSID();
  if (ssid.length() > 0) {
    strlcpy(settings.wifiSsid, ssid.c_str(), sizeof(settings.wifiSsid));
    strlcpy(settings.wifiPassword, manager->getWiFiPass().c_str(), sizeof(settings.wifiPassword));
  }

  strlcpy(settings.githubUser, userParam->getValue(), sizeof(settings.githubUser));
  // The token field starts empty so the saved token is never shown on the page.
  // Empty keeps the old token, a single "-" removes it.
  const char* token = tokenParam->getValue();
  if (strcmp(token, "-") == 0) settings.githubToken[0] = '\0';
  else if (token[0] != '\0') strlcpy(settings.githubToken, token, sizeof(settings.githubToken));
  settings.latitude = parseCoordinate(latParam->getValue());
  settings.longitude = parseCoordinate(lonParam->getValue());
  if (isnan(settings.latitude) || isnan(settings.longitude)) {
    settings.latitude = settings.longitude = NAN;  // half a coordinate is no coordinate
  }

  saveSettings();
  Serial.println("[portal] settings saved, restarting");
  delay(500);  // let the browser get its reply
  ESP.restart();
}

}  // namespace

void startPortal() {
  if (active) return;

  if (manager == nullptr) {
    static char lat[16];
    static char lon[16];
    formatCoordinate(settings.latitude, lat, sizeof(lat));
    formatCoordinate(settings.longitude, lon, sizeof(lon));

    manager = new WiFiManager();
    userParam = new WiFiManagerParameter("ghuser", "GitHub user", settings.githubUser, 39);
    tokenParam = new WiFiManagerParameter(
        "ghtoken", "GitHub token (optional; empty keeps the saved one, - removes it)", "", 127);
    latParam = new WiFiManagerParameter("lat", "Latitude (empty = find from IP address)", lat, 15);
    lonParam = new WiFiManagerParameter("lon", "Longitude", lon, 15);
    manager->addParameter(userParam);
    manager->addParameter(tokenParam);
    manager->addParameter(latParam);
    manager->addParameter(lonParam);

    manager->setTitle("deskpal setup");
    manager->setConfigPortalBlocking(false);
    // Save the GitHub fields even if the new Wi-Fi does not connect right away.
    manager->setBreakAfterConfig(true);
    manager->setSaveConfigCallback([] { saveRequested = true; });
    manager->setSaveParamsCallback([] { saveRequested = true; });
  }

  Serial.printf("[portal] open: join Wi-Fi \"%s\" and go to 192.168.4.1\n", PORTAL_AP_NAME);
  manager->startConfigPortal(PORTAL_AP_NAME);
  active = true;
  saveRequested = false;
  openedAtMs = millis();
}

bool portalActive() { return active; }

void updatePortal() {
  if (!active) return;
  manager->process();
  if (saveRequested) applyAndRestart();

  // WiFiManager has no timeout in non-blocking mode. A restart is the
  // simplest clean way back: it reconnects with the saved settings.
  if (millis() - openedAtMs >= PORTAL_TIMEOUT_MS) {
    Serial.println("[portal] nothing saved, restarting");
    ESP.restart();
  }
}
