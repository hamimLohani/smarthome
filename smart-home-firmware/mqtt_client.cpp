/**
 * Module: mqtt_client.cpp
 * Responsibility: PubSubClient lifecycle, topic routing, exponential back-off.
 *
 * Wildcard subscribe pattern lets the device respond to commands before the
 * user UID is known.  The UID is extracted from the first inbound topic and
 * cached so retained state publishes use the correct full path.
 */

#include "mqtt_client.h"
#include "config.h"
#include "identity.h"
#include "relays.h"
#include "timers.h"
#include "scheduling.h"
#include "wifi_setup.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClientSecure espClient;
static PubSubClient mqttClient(espClient);

// Reconnection back-off state
static unsigned long lastReconnectAttempt = 0;
static unsigned long reconnectInterval    = MQTT_RECONNECT_INTERVAL_MS;
static const unsigned long MAX_BACKOFF_MS = 60000UL; // cap at 60 s

static void publishCurrentState();

// ── Topic helpers ─────────────────────────────────────────────────────────────

// Build base topic using pairedUid (or a placeholder if not yet paired)
static String baseTopic() {
  String uid = (pairedUid.length() > 0) ? pairedUid : "unknown";
  return "users/" + uid + "/devices/" + deviceId;
}

// ── Incoming message handler ──────────────────────────────────────────────────

static void onMqttMessage(const char* topic, byte* payload, unsigned int length) {
  // Safety-cap payload length to avoid stack overflow on malformed messages
  const unsigned int MAX_PAYLOAD = 512;
  if (length > MAX_PAYLOAD) {
    DEBUG_PRINTLN("[MQTT] Payload too large, ignoring.");
    return;
  }

  char msg[MAX_PAYLOAD + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  DEBUG_PRINTF("[MQTT] Received on [%s]: %s\n", topic, msg);

  // ── Parse topic to extract uid and plug number ───────────────────────────
  // Expected format: users/{uid}/devices/{deviceId}/plug/{n}/...
  String topicStr(topic);

  if (topicStr.endsWith("/unpair")) {
    if (strcmp(msg, "unpair") == 0) {
      DEBUG_PRINTLN("[MQTT] Unpair command received.");
      unpairDeviceMqtt();
    }
    return;
  }

  if (topicStr.endsWith("/sync")) {
    // Pair/link only through the explicit web sync handshake. Retained plug,
    // timer, or schedule topics must not silently re-link after local unpair.
    int usersIdx = topicStr.indexOf("users/");
    int nextSlash = topicStr.indexOf('/', usersIdx + 6);
    if (usersIdx >= 0 && nextSlash > usersIdx) {
      String extractedUid = topicStr.substring(usersIdx + 6, nextSlash);
      if (extractedUid != "unknown" && extractedUid.length() > 0) {
        setPairedUid(extractedUid);
      }
    }
    publishCurrentState();
    return;
  }

  // Extract plug number (segment after "/plug/")
  int plugIdx = topicStr.indexOf("/plug/");
  if (plugIdx < 0) return;
  int plugNumStart = plugIdx + 6;
  int plugNumEnd   = topicStr.indexOf('/', plugNumStart);
  if (plugNumEnd < 0) return;

  int plugNum = topicStr.substring(plugNumStart, plugNumEnd).toInt();
  if (plugNum < 1 || plugNum > NUM_PLUGS) return;
  uint8_t plugIndex = (uint8_t)(plugNum - 1); // convert 1-based to 0-based

  String suffix = topicStr.substring(plugNumEnd + 1);

  // ── plug/{n}/set ─────────────────────────────────────────────────────────
  if (suffix == "set") {
    bool turnOn = (strcmp(msg, "1") == 0 || strcmp(msg, "ON") == 0 ||
                   strcmp(msg, "true") == 0);
    setPlug(plugIndex, turnOn);
    return;
  }

  // ── plug/{n}/timer/set ───────────────────────────────────────────────────
  if (suffix == "timer/set") {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg) != DeserializationError::Ok) {
      DEBUG_PRINTLN("[MQTT] timer/set parse error");
      return;
    }
    if (doc.containsKey("cancel") && doc["cancel"].as<bool>()) {
      cancelDelayTimer(plugIndex);
    } else {
      uint32_t durSec = doc["durationSeconds"] | 0;
      const char* act = doc["action"] | "OFF";
      bool on = (strcmp(act, "ON") == 0);
      if (durSec > 0) startDelayTimer(plugIndex, durSec, on);
    }
    return;
  }

  // ── plug/{n}/schedule ────────────────────────────────────────────────────
  if (suffix == "schedule") {
    applyMqttSchedule(plugIndex, msg);
    return;
  }
}

// ── Subscription helper ───────────────────────────────────────────────────────

static void subscribeAll() {
  // Use '+' wildcard for UID so commands arrive before pairing is complete
  String sub = "users/+/devices/" + deviceId + "/plug/+/set";
  mqttClient.subscribe(sub.c_str(), 1);

  sub = "users/+/devices/" + deviceId + "/plug/+/timer/set";
  mqttClient.subscribe(sub.c_str(), 1);

  sub = "users/+/devices/" + deviceId + "/plug/+/schedule";
  mqttClient.subscribe(sub.c_str(), 1);

  // Sync topic to pick up owner's UID automatically on boot/pairing
  sub = "users/+/devices/" + deviceId + "/sync";
  mqttClient.subscribe(sub.c_str(), 1);

  // Unpair topic to listen for unpairing from the web app
  sub = "users/+/devices/" + deviceId + "/unpair";
  mqttClient.subscribe(sub.c_str(), 1);

  DEBUG_PRINTLN("[MQTT] Subscriptions registered.");
}

static void publishCurrentState() {
  if (!mqttClient.connected()) return;

  String statusTopic = baseTopic() + "/status";
  mqttClient.publish(statusTopic.c_str(), "online", true);

  for (uint8_t i = 0; i < NUM_PLUGS; i++) {
    publishPlugState(i, getPlugState(i));
  }

  for (uint8_t i = 0; i < NUM_PLUGS; i++) {
    publishTimerState(i, false, 0, false);
  }
}

// ── Connection / reconnection ─────────────────────────────────────────────────

static void connectMqtt() {
  if (deviceId.length() == 0) {
    DEBUG_PRINTLN("[MQTT] Device ID not loaded yet — skipping connect attempt.");
    return;
  }

  // LWT: broker publishes "offline" if connection drops unexpectedly
  String lwtTopic   = baseTopic() + "/status";
  String clientId   = deviceId;

  const char* mqttUser = (strlen(MQTT_DEV_USERNAME) > 0) ? MQTT_DEV_USERNAME : deviceId.c_str();
  const char* mqttPass = (strlen(MQTT_DEV_PASSWORD) > 0) ? MQTT_DEV_PASSWORD : deviceSecret.c_str();

  DEBUG_PRINTF("[MQTT] Connecting as %s (user: %s) ...\n", clientId.c_str(), mqttUser);

  bool ok = mqttClient.connect(
    clientId.c_str(),
    mqttUser,
    mqttPass,
    lwtTopic.c_str(),
    1,     // LWT QoS
    true,  // LWT retain
    "offline"
  );

  if (ok) {
    DEBUG_PRINTLN("[MQTT] Connected!");
    reconnectInterval = MQTT_RECONNECT_INTERVAL_MS; // reset back-off

    // Subscribe to command topics
    subscribeAll();

    publishCurrentState();
  } else {
    DEBUG_PRINTF("[MQTT] Connect failed, rc=%d. Next retry in %lu ms\n",
                 mqttClient.state(), reconnectInterval);
    // Exponential back-off, capped at MAX_BACKOFF_MS
    reconnectInterval = min(reconnectInterval * 2, MAX_BACKOFF_MS);
  }
}

// ── Public API ────────────────────────────────────────────────────────────────

void initMqtt() {
  espClient.setInsecure(); // Disable TLS certificate chain validation to save RAM
  espClient.setBufferSizes(2048, 1024); // Limit TLS buffer sizes to prevent heap OOM crash
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(onMqttMessage);
  mqttClient.setBufferSize(512); // Increase from default 256 for schedule payloads
  DEBUG_PRINTLN("[MQTT] Initialized.");
}

void processMqtt() {
  if (!isWiFiConnected()) return; // No WiFi → nothing to do

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= reconnectInterval) {
      lastReconnectAttempt = now;
      connectMqtt();
    }
    return;
  }

  // Pump the MQTT library — processes incoming messages and keep-alive
  mqttClient.loop();
}

void publishPlugState(uint8_t plugIndex, bool on) {
  if (!mqttClient.connected()) return;
  if (plugIndex >= NUM_PLUGS) return;

  String topic = baseTopic() + "/plug/" + String(plugIndex + 1) + "/state";
  const char* payload = on ? "1" : "0";
  mqttClient.publish(topic.c_str(), payload, true); // retain = true
  DEBUG_PRINTF("[MQTT] Published plug %u state = %s\n", plugIndex + 1, payload);
}

void publishTimerState(uint8_t plugIndex, bool active,
                       uint32_t remainingSec, bool action) {
  if (plugIndex >= NUM_PLUGS) return;

  // Throttle: only publish once per second to avoid flooding the broker
  static unsigned long lastTimerPublish[4] = { 0, 0, 0, 0 };
  unsigned long now = millis();

  if (!active || now - lastTimerPublish[plugIndex] < 1000UL) {
    // Always publish when cancelling (active == false)
    if (active) return;
  }
  lastTimerPublish[plugIndex] = now;

  if (!mqttClient.connected()) return;

  StaticJsonDocument<128> doc;
  doc["active"]           = active;
  doc["remainingSeconds"] = remainingSec;
  doc["action"]           = action ? "ON" : "OFF";

  char buf[128];
  serializeJson(doc, buf);

  String topic = baseTopic() + "/plug/" + String(plugIndex + 1) + "/timer/state";
  mqttClient.publish(topic.c_str(), buf, true);
}

void publishScheduleState(uint8_t plugIndex) {
  if (!mqttClient.connected()) return;
  if (plugIndex >= NUM_PLUGS) return;

  String topic = baseTopic() + "/plug/" + String(plugIndex + 1) + "/schedule";

  StaticJsonDocument<512> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (uint8_t s = 0; s < MAX_SCHEDULES_PER_PLUG; s++) {
    const ScheduleEntry& entry = getScheduleEntry(plugIndex, s);
    if (entry.enabled || entry.hour != 0 || entry.minute != 0 || entry.turnOn) {
      JsonObject obj = arr.createNestedObject();
      char timeBuf[6];
      snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", entry.hour, entry.minute);
      obj["t"] = timeBuf;
      obj["a"] = entry.turnOn ? 1 : 0;
      obj["r"] = entry.repeatDaily ? 1 : 0;
      obj["e"] = entry.enabled ? 1 : 0;
    }
  }

  char buf[512];
  serializeJson(doc, buf);
  mqttClient.publish(topic.c_str(), buf, true);
  DEBUG_PRINTF("[MQTT] Published plug %u schedule array = %s\n", plugIndex + 1, buf);
}

bool isMqttConnected() {
  return mqttClient.connected();
}

void setPairedUid(const String& uid) {
  if (uid != pairedUid) {
    pairedUid = uid;
    DEBUG_PRINTF("[MQTT] Paired UID set to: %s. Saving to flash...\n", pairedUid.c_str());
    // FIX: Pass the current `registered` state so it is not silently reset to
    // false (the default) each time a new UID is extracted from an incoming
    // MQTT topic. If registered was true before pairing, it must stay true.
    saveIdentity(deviceId, deviceSecret, pairedUid, registered);
  }
}

void unpairDeviceMqtt() {
  String uid = pairedUid;
  pairedUid = "";

  if (mqttClient.connected()) {
    // Device-scoped event works even when pairedUid was lost or not yet synced.
    mqttClient.publish((String("devices/") + deviceId + "/unpair_event").c_str(), "unpair", true);
  }

  if (uid.length() > 0) {
    DEBUG_PRINTLN("[MQTT] Clearing retained messages and notifying unpair...");
    
    // Publish retained event so the web app can clean Firebase even if it was closed.
    mqttClient.publish((String("users/") + uid + "/devices/" + deviceId + "/unpair_event").c_str(), "unpair", true);
    
    // Clear any retained unpair message so the ESP doesn't re-trigger unpair upon next connection
    mqttClient.publish((String("users/") + uid + "/devices/" + deviceId + "/unpair").c_str(), "", true);

    // Clear status
    mqttClient.publish((String("users/") + uid + "/devices/" + deviceId + "/status").c_str(), "", true);
    // Clear sync
    mqttClient.publish((String("users/") + uid + "/devices/" + deviceId + "/sync").c_str(), "", true);

    // Clear plug states, timers, and schedules
    for (int i = 1; i <= NUM_PLUGS; i++) {
      mqttClient.publish((String("users/") + uid + "/devices/" + deviceId + "/plug/" + String(i) + "/state").c_str(), "", true);
      mqttClient.publish((String("users/") + uid + "/devices/" + deviceId + "/plug/" + String(i) + "/timer/state").c_str(), "", true);
      mqttClient.publish((String("users/") + uid + "/devices/" + deviceId + "/plug/" + String(i) + "/schedule").c_str(), "", true);
    }
    
    // Ensure all messages are processed by client & broker before disconnecting
    for (int i = 0; i < 30; i++) {
      mqttClient.loop();
      delay(15);
    }
    
    // Disconnect so the broker processes the tombstone publishes and stops wildcard commands
    mqttClient.disconnect();
  }

  // Clear Firebase Database claim (resets claimed_by to null)
  registerDeviceInFirebase();

  // Reset local pairedUid and save identity
  saveIdentity(deviceId, deviceSecret, "", registered);
}
