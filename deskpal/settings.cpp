#include "settings.h"

#include <Preferences.h>

Settings settings;

namespace {

const char kNamespace[] = "deskpal";

void loadString(Preferences& prefs, const char* key, char* out, size_t len) {
  if (prefs.isKey(key)) prefs.getString(key, out, len);
}

}  // namespace

void loadSettings() {
  Preferences prefs;
  // Read-only open fails if the portal has never saved anything: keep the defaults.
  if (!prefs.begin(kNamespace, true)) return;
  loadString(prefs, "ssid", settings.wifiSsid, sizeof(settings.wifiSsid));
  loadString(prefs, "pass", settings.wifiPassword, sizeof(settings.wifiPassword));
  loadString(prefs, "ghuser", settings.githubUser, sizeof(settings.githubUser));
  loadString(prefs, "ghtoken", settings.githubToken, sizeof(settings.githubToken));
  if (prefs.isKey("lat")) settings.latitude = prefs.getFloat("lat", NAN);
  if (prefs.isKey("lon")) settings.longitude = prefs.getFloat("lon", NAN);
  prefs.end();
  Serial.println("Loaded settings saved by the setup portal");
}

void saveSettings() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    Serial.println("Could not open settings storage");
    return;
  }
  prefs.putString("ssid", settings.wifiSsid);
  prefs.putString("pass", settings.wifiPassword);
  prefs.putString("ghuser", settings.githubUser);
  prefs.putString("ghtoken", settings.githubToken);
  prefs.putFloat("lat", settings.latitude);
  prefs.putFloat("lon", settings.longitude);
  prefs.end();
}
