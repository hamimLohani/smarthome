#include "identity.h"
#include "config.h"
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>

String deviceId = "";
String deviceSecret = "";
bool registered = false;
String pairedUid = "";
int lastRegHttpCode = 0;

bool loadIdentity() {
  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("[Identity] Failed to mount LittleFS!");
    return false;
  }

  if (!LittleFS.exists(FILE_IDENTITY)) {
    DEBUG_PRINTLN("[Identity] /identity.json not found. Self-generating "
                  "fallback for dev/testing...");
    generateFallbackIdentity();
    return true;
  }

  File file = LittleFS.open(FILE_IDENTITY, "r");
  if (!file) {
    DEBUG_PRINTLN("[Identity] Failed to open /identity.json for reading!");
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    DEBUG_PRINTLN("[Identity] Failed to parse /identity.json!");
    return false;
  }

  const char *id = doc["device_id"];
  const char *secret = doc["device_secret"];
  const char *pUid = doc["paired_uid"];
  registered = doc["registered"] | false;

  if (!id || !secret) {
    DEBUG_PRINTLN("[Identity] /identity.json is missing required fields!");
    return false;
  }

  deviceId = String(id);
  deviceSecret = String(secret);
  pairedUid = pUid ? String(pUid) : "";

  // Auto-migrate legacy device ID formats (e.g. SP-XXXXXXXX) to new variant
  // format (e.g. SHX-XXXXXXXX)
  String expectedPrefix = "SH" + String(NUM_PLUGS) + "-";
  if (!deviceId.startsWith(expectedPrefix)) {
    DEBUG_PRINTF("[Identity] Legacy/mismatched device ID '%s' found. "
                 "Regenerating variant prefix '%s'...\n",
                 deviceId.c_str(), expectedPrefix.c_str());
    generateFallbackIdentity();
  }

  DEBUG_PRINTF("[Identity] Loaded: ID = %s, Paired UID = %s, Registered = %s\n",
               deviceId.c_str(), pairedUid.c_str(), registered ? "yes" : "no");
  return true;
}

void saveIdentity(const String &id, const String &secret, const String &pUid,
                  bool reg) {
  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("[Identity] Failed to mount LittleFS for saving identity!");
    return;
  }

  StaticJsonDocument<256> doc;
  doc["device_id"] = id;
  doc["device_secret"] = secret;
  doc["paired_uid"] = pUid;
  doc["registered"] = reg;

  File file = LittleFS.open(FILE_IDENTITY, "w");
  if (!file) {
    DEBUG_PRINTLN("[Identity] Failed to open /identity.json for writing!");
    return;
  }

  if (serializeJson(doc, file) == 0) {
    DEBUG_PRINTLN("[Identity] Failed to write JSON to file!");
  } else {
    DEBUG_PRINTLN("[Identity] Saved device identity successfully.");
  }
  file.close();

  deviceId = id;
  deviceSecret = secret;
  pairedUid = pUid;
  registered = reg;
}

bool registerDeviceInFirebase() {
  DEBUG_PRINTLN("[Identity] Device provisioning is handled by backend. Marking registered.");
  saveIdentity(deviceId, deviceSecret, pairedUid, true);
  return true;
}

void generateFallbackIdentity() {
  // Fallback: device_id based on ESP chip ID with plug-count prefix
  uint32_t chipId = ESP.getChipId();
  char idBuf[16];
  snprintf(idBuf, sizeof(idBuf), "SH%d-%08X", NUM_PLUGS, chipId);
  deviceId = String(idBuf);

  // Generate a simple secret based on chip ID and analog read for randomness
  randomSeed(micros() + analogRead(0));
  long randVal1 = random(100000, 999999);
  long randVal2 = random(100000, 999999);
  char secretBuf[32];
  snprintf(secretBuf, sizeof(secretBuf), "sec-%ld%ld", randVal1, randVal2);
  deviceSecret = String(secretBuf);

  DEBUG_PRINTF("[Identity] Self-generated: ID = %s, Secret = %s\n",
               deviceId.c_str(), deviceSecret.c_str());

  // Save the self-generated identity so it persists
  saveIdentity(deviceId, deviceSecret);
}

void performFactoryReset() {
  DEBUG_PRINTLN("[Identity] Performing Factory Reset...");

  // Clear WiFi credentials
  WiFi.disconnect(true);

  // Format LittleFS
  if (LittleFS.begin()) {
    LittleFS.format();
    DEBUG_PRINTLN("[Identity] LittleFS formatted.");
  }

  DEBUG_PRINTLN("[Identity] Factory reset done. Restarting ESP...");
  delay(1000);
  ESP.restart();
}
