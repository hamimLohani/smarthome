#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define FIRMWARE_VERSION "v1.6.0"

// ── Debug Configuration ──────────────────────────────────────────────────────
#define DEBUG_SERIAL false
#define DEBUG_BAUD   115200

#if DEBUG_SERIAL
  #define DEBUG_PRINT(x)     Serial.print(x)
  #define DEBUG_PRINTLN(x)   Serial.println(x)
  #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif


// ── Hardware Variant Definitions ─────────────────────────────────────────────
#define VARIANT_2_PORT 1
#define VARIANT_3_PORT 2
#define VARIANT_4_PORT 3

// SELECT ACTIVE HARDWARE VARIANT HERE:
#define ACTIVE_VARIANT VARIANT_4_PORT

// ── Automatic Variant Pin Configuration ──────────────────────────────────────
#define RELAY_ACTIVE_LOW true

#if (ACTIVE_VARIANT == VARIANT_2_PORT)
  #define NUM_PLUGS 2
  const uint8_t RELAY_PINS[NUM_PLUGS] = { 14, 13 }; // Relays 1, 2
#elif (ACTIVE_VARIANT == VARIANT_3_PORT)
  #define NUM_PLUGS 3
  const uint8_t RELAY_PINS[NUM_PLUGS] = { 14, 13, 12 }; // Relays 1, 2, 3
#elif (ACTIVE_VARIANT == VARIANT_4_PORT)
  #define NUM_PLUGS 4
  const uint8_t RELAY_PINS[NUM_PLUGS] = { 14, 13, 12, 16 }; // Relays 1, 2, 3, 4
#endif


// ── WiFi & Recovery Configuration ────────────────────────────────────────────
#define WIFI_MAX_RETRIES 3
#define WIFI_RETRY_INTERVAL_MS 10000

// ── MQTT Configuration ───────────────────────────────────────────────────────
#define MQTT_HOST "c617578413a54eb186e280d7c7917aa9.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883 // HiveMQ Cloud secure port (TLS)
#define MQTT_RECONNECT_INTERVAL_MS 5000

// Optional: Set these to override device-specific credentials in development/testing.
// If left as empty strings (""), the device uses its deviceId and deviceSecret (production-mode).
#define MQTT_DEV_USERNAME "smartmultiplug"
#define MQTT_DEV_PASSWORD "smartmultiplug"

// ── Time & NTP Configuration ─────────────────────────────────────────────────
#define NTP_SERVER "pool.ntp.org"
#define NTP_OFFSET_SEC 21600 // default offset, e.g. +6 hours (21600 seconds)
#define NTP_UPDATE_INTERVAL_MS 3600000 // 1 hour

// ── Persistent Storage Config Files ─────────────────────────────────────────
#define FILE_IDENTITY "/identity.json"
#define FIREBASE_DB_URL "https://smart-multi-plug-b8bf0-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FILE_SCHEDULES "/schedules.json"
#define FILE_WIFI_PREF "/wifi_pref.json"
#define MAX_SCHEDULES_PER_PLUG 4

// ── Optional Sensors ─────────────────────────────────────────────────────────
// Set to true to enable compiling in sensor code
#define ENABLE_DHT11 false
#define ENABLE_GAS   false
#define ENABLE_FLAME false

#if ENABLE_DHT11
  #define PIN_DHT11 5 // D1
#endif
#if ENABLE_GAS
  #define PIN_GAS 4 // D2
#endif
#if ENABLE_FLAME
  #define PIN_FLAME 2 // D4
#endif

#endif // CONFIG_H
