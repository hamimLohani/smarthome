#include "wifi_setup.h"
#include "config.h"
#include "identity.h"

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

bool offlineMode = false;
bool inWiFiPortal = false;
int wifiConnectAttempts = 0;

static WiFiManager wm;
static unsigned long lastWiFiCheckTime = 0;
static unsigned long portalStartTime = 0;
static bool wifiConnectedBefore = false;
static char s_apName[32] = "";
static char s_infoHtml[384] = "";
static WiFiManagerParameter* s_custom_info = nullptr;



// Forward declaration of callbacks
void configModeCallback(WiFiManager *myWiFiManager);
void saveConfigCallback();

void loadWiFiPreferences() {
  if (!LittleFS.begin()) return;
  if (!LittleFS.exists(FILE_WIFI_PREF)) {
    offlineMode = false;
    return;
  }

  File file = LittleFS.open(FILE_WIFI_PREF, "r");
  if (!file) return;

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (!err) {
    offlineMode = doc["offline_mode"] | false;
  }
}

void saveWiFiPreferences() {
  if (!LittleFS.begin()) return;
  File file = LittleFS.open(FILE_WIFI_PREF, "w");
  if (!file) return;

  StaticJsonDocument<128> doc;
  doc["offline_mode"] = offlineMode;
  serializeJson(doc, file);
  file.close();
}

void configModeCallback(WiFiManager *myWiFiManager) {
  DEBUG_PRINTLN("[WiFi] Entered Config Portal Mode");
  inWiFiPortal = true;
  portalStartTime = millis();
  
  // Update display to portal/setup instructions

}

void saveConfigCallback() {
  DEBUG_PRINTLN("[WiFi] Settings saved in portal.");
  offlineMode = false;
  saveWiFiPreferences();
}

void bindServerCallback() {
  wm.server->on("/", []() {
    wm.server->sendHeader("Location", "/wifi", true);
    wm.server->send(302, "text/plain", "");
  });
}

void initWiFi() {
  loadWiFiPreferences();

  // Build AP name containing the full device ID
  snprintf(s_apName, sizeof(s_apName), "SmartHome-Setup-%s", deviceId.c_str());

  // Configure WiFiManager
  wm.setAPCallback(configModeCallback);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConfigPortalBlocking(false); // Non-blocking configuration portal
  wm.setConnectTimeout(8); // Timeout for connection attempts in seconds

  // Redirect root to configuration page
  wm.setWebServerCallback(bindServerCallback);

  // Restrict menu to "wifi" (Configure WiFi)
  std::vector<const char *> menu = {"wifi"};
  wm.setMenu(menu);

  snprintf(
    s_infoHtml,
    sizeof(s_infoHtml),
    "<div style='border:1px solid #ddd;border-radius:8px;padding:12px;margin:8px 0 16px'>"
    "<h3 style='margin:0 0 8px'>Smart Home Setup</h3>"
    "<p style='margin:0 0 8px'>Device ID: <b>%s</b></p>"
    "<ol style='margin:0;padding-left:18px'>"
    "<li>Select your home WiFi.</li>"
    "<li>Enter the WiFi password and save.</li>"
    "<li>Pair this Device ID in the app.</li>"
    "</ol>"
    "</div>",
    deviceId.c_str()
  );
  s_custom_info = new WiFiManagerParameter(s_infoHtml);
  wm.addParameter(s_custom_info);

  if (WiFi.SSID() != "") {
    DEBUG_PRINTLN("[WiFi] Saved credentials found. Connecting...");
    WiFi.begin();
    wifiConnectAttempts = 0;
    lastWiFiCheckTime = millis();
    wifiConnectedBefore = false;
  } else {
    DEBUG_PRINTLN("[WiFi] No saved credentials found.");
    // Screenless variant: Automatically start configuration portal AP immediately
    DEBUG_PRINTLN("[WiFi] No display: starting config portal immediately.");
    startWiFiPortal();
  }
}

void processWiFi() {
  // Let WiFiManager process portal tasks
  wm.process();

  if (inWiFiPortal) {
    // If we've successfully connected while in the portal
    if (WiFi.status() == WL_CONNECTED) {
      DEBUG_PRINTLN("[WiFi] Connected from config portal!");
      inWiFiPortal = false;
      offlineMode = false;
      saveWiFiPreferences();
      // Go to pairing code screen for 10 seconds to let the user pair


    }
    return;
  }

  // Periodic check if connection failed / lost
  unsigned long now = millis();
  if (now - lastWiFiCheckTime >= WIFI_RETRY_INTERVAL_MS) {
    lastWiFiCheckTime = now;

    if (WiFi.status() == WL_CONNECTED) {
      if (!wifiConnectedBefore) {
        DEBUG_PRINTLN("[WiFi] Connection established.");
        wifiConnectedBefore = true;
        wifiConnectAttempts = 0;
        // Refresh Firebase on first WiFi connection so a deleted database can
        // be recreated from the device's persisted identity.
        DEBUG_PRINTLN("[WiFi] Refreshing device registration in Firebase...");
        registerDeviceInFirebase();
      }
    } else {
      // WiFi disconnected
      if (WiFi.SSID() != "" && !offlineMode) {
        wifiConnectAttempts++;
        DEBUG_PRINTF("[WiFi] Connection attempt %d / %d failed\n", wifiConnectAttempts, WIFI_MAX_RETRIES);
        
        if (wifiConnectAttempts >= WIFI_MAX_RETRIES) {
          DEBUG_PRINTLN("[WiFi] Reached max retries. Dropping to captive portal fallback.");
          startWiFiPortal();
        }
      }
    }
  }
}

void startWiFiPortal() {
  if (inWiFiPortal) return;
  DEBUG_PRINTLN("[WiFi] Launching captive portal AP...");
  wm.startConfigPortal(s_apName);
}

void forgetWiFiCredentials() {
  DEBUG_PRINTLN("[WiFi] Forgetting credentials.");
  wm.resetSettings(); // Clears WiFi credentials
  offlineMode = true;
  inWiFiPortal = false;
  saveWiFiPreferences();
  WiFi.disconnect(true);
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

const char* getApName() {
  return s_apName;
}
