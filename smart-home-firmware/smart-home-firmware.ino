/**
 * smart-multiplug-firmware.ino
 *
 * Entry point for the Smart Home ESP8266 firmware.
 *
 * Responsibilities of this file:
 *   - Call each module's init function once in setup()
 *   - Call each module's process/update function in loop()
 *   - Keep setup() and loop() as thin orchestration only —
 *     no business logic lives here.
 *
 * Extension point:
 *   checkForOTAUpdate() is called once per loop as a placeholder for future
 *   OTA (over-the-air) firmware updates.  It does nothing in this version.
 *
 * Board settings (Arduino IDE):
 *   Board:            NodeMCU 1.0 (ESP-12E) or Generic ESP8266 Module
 *   Flash size:       4MB (FS:2MB, OTA:~1MB) — required for LittleFS partition
 *   CPU frequency:    80 MHz
 *   Upload speed:     115200
 *   LittleFS:         enabled (Tools > Flash Size: 4MB (FS:2MB))
 *
 * Required libraries (install via Arduino Library Manager or PlatformIO):
 *   - ESP8266WiFi          (bundled with esp8266 board package)
 *   - WiFiManager          by tzapu  (v2.0+)
 *   - PubSubClient         by Nick O'Leary
 *   - ArduinoJson          by Benoit Blanchon (v6+)
 *   - LittleFS             (bundled with esp8266 board package)
 *   - NTPClient            by Arduino Libraries / Fabrice Weinberg
 *   - Time                 by PaulStoffregen
 *   - DHT sensor library   by Adafruit  (only required if ENABLE_DHT11 = true)
 */

#include <Arduino.h>

// All module headers
#include "config.h"
#include "identity.h"
#include "wifi_setup.h"
#include "mqtt_client.h"
#include "relays.h"
#include "time_sync.h"
#include "timers.h"
#include "scheduling.h"
#include "sensors.h"

// ── OTA placeholder ──────────────────────────────────────────────────────────
/**
 * Placeholder for future OTA update logic.
 * Called once per loop.  Does nothing in this firmware version.
 * Replace this body when adding ArduinoOTA or a custom HTTP OTA mechanism.
 */
static void checkForOTAUpdate() {
  // TODO: implement OTA when needed
}


// ── setup() ──────────────────────────────────────────────────────────────────
void setup() {
#if DEBUG_SERIAL
  Serial.begin(DEBUG_BAUD);
  delay(200); // Let the serial port settle
  Serial.println("\n\n[Boot] Smart Home firmware starting...");
#endif


  // 2. Load device identity from LittleFS
  //    Halts with an error if identity is missing and not in dev mode
  if (!loadIdentity()) {
    // Show error on screen and halt (watchdog will eventually reset the device)
    DEBUG_PRINTLN("[Boot] FATAL: Could not load device identity!");
    while (true) {
      delay(1000);
    }
  }

  // 3. Initialize hardware — relays restore last-known state from flash
  initRelays();

  // 4. WiFi — non-blocking; may open captive portal if no credentials saved
  initWiFi();

  // 5. Time sync — begins NTP polling once WiFi connects
  initTimeSync();

  // 6. Scheduling — loads persisted schedules from LittleFS
  initScheduling();

  // 7. Delay timers — clear all slots (timers do not survive a reboot)
  initTimers();

  // 8. Sensors — initialize the optional DHT11, Gas, and Flame sensors
  initSensors();

  // 9. MQTT — connects once WiFi is available (handled in processMqtt)
  initMqtt();

  DEBUG_PRINTLN("[Boot] Setup complete.");
  

}

// ── loop() ───────────────────────────────────────────────────────────────────
void loop() {
  // ── WiFi management (reconnect / captive portal) ─────────────────────────
  processWiFi();

  // ── MQTT keep-alive and incoming message pump ─────────────────────────────
  processMqtt();

  // ── NTP time sync (re-sync every NTP_UPDATE_INTERVAL_MS while online) ────
  processTimeSync();

  // ── Scheduling: evaluate daily ON/OFF entries against current wall-clock ──
  processSchedules();

  // ── Delay timers: check countdown expiry and fire setPlug() when done ─────
  processTimers();

  // ── Sensors: read sensors periodically and publish if values change ───────
  processSensors();


  // ── OTA update check (placeholder — does nothing in this version) ─────────
  checkForOTAUpdate();

  // ── Watchdog-safe: yield() lets the ESP8266 background tasks run ──────────
  yield();
}
