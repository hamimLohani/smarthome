/**
 * Module: relays.cpp
 * Responsibility: Single point of truth for relay GPIO control.
 *
 * Relay writes always go through setPlug() which simultaneously:
 *   1. Drives the GPIO (respecting RELAY_ACTIVE_LOW)
 *   2. Updates the in-memory plugStates[] array
 *   3. Publishes the new retained MQTT state message
 *   4. Persists state to LittleFS (survives power-loss / WiFi reboot)
 *   5. Syncs state
 */

#include "relays.h"
#include "config.h"
#include "mqtt_client.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

// In-memory plug state array — index 0..NUM_PLUGS-1
static bool plugStates[NUM_PLUGS] = { false };

// File used to persist relay states across reboots
#define FILE_PLUG_STATES "/plug_states.json"

// ── Low-level GPIO helper ─────────────────────────────────────────────────────
static void driveRelay(uint8_t plugIndex, bool on) {
  if (plugIndex >= NUM_PLUGS) return;
  uint8_t pin = RELAY_PINS[plugIndex];
#if RELAY_ACTIVE_LOW
  digitalWrite(pin, on ? LOW : HIGH);
#else
  digitalWrite(pin, on ? HIGH : LOW);
#endif
}

// ── Public API ────────────────────────────────────────────────────────────────

void initRelays() {
  for (uint8_t i = 0; i < NUM_PLUGS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    // Default safe state: all relays OFF before loading persisted state
    driveRelay(i, false);
  }
  // Restore last known states from flash
  loadPlugStates();
  DEBUG_PRINTLN("[Relays] Initialized.");
}

void setPlug(uint8_t plugIndex, bool on) {
  if (plugIndex >= NUM_PLUGS) return;

  plugStates[plugIndex] = on;
  driveRelay(plugIndex, on);

  DEBUG_PRINTF("[Relays] Plug %u set to %s\n", plugIndex + 1, on ? "ON" : "OFF");

  // Publish state to MQTT (retained so broker remembers it)
  publishPlugState(plugIndex, on);

  // Persist to flash so state survives a reboot
  savePlugStates();

  // State reflected

}

bool getPlugState(uint8_t plugIndex) {
  if (plugIndex >= NUM_PLUGS) return false;
  return plugStates[plugIndex];
}

void savePlugStates() {
  if (!LittleFS.begin()) return;

  StaticJsonDocument<128> doc;
  for (uint8_t i = 0; i < NUM_PLUGS; i++) {
    doc[String(i)] = plugStates[i];
  }

  File file = LittleFS.open(FILE_PLUG_STATES, "w");
  if (!file) {
    DEBUG_PRINTLN("[Relays] Failed to open plug_states.json for writing!");
    return;
  }
  serializeJson(doc, file);
  file.close();
}

void loadPlugStates() {
  if (!LittleFS.begin()) return;
  if (!LittleFS.exists(FILE_PLUG_STATES)) return;

  File file = LittleFS.open(FILE_PLUG_STATES, "r");
  if (!file) return;

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    DEBUG_PRINTLN("[Relays] Failed to parse plug_states.json!");
    return;
  }

  for (uint8_t i = 0; i < NUM_PLUGS; i++) {
    bool state = doc[String(i)] | false;
    plugStates[i] = state;
    driveRelay(i, state);
    DEBUG_PRINTF("[Relays] Restored Plug %u -> %s\n", i + 1, state ? "ON" : "OFF");
  }
}
