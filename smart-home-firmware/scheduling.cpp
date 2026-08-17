/**
 * Module: scheduling.cpp
 * Responsibility: Persistent daily per-plug schedule evaluation with multi-slot support and MQTT sync.
 */

#include "scheduling.h"
#include "config.h"
#include "relays.h"
#include "time_sync.h"
#include "mqtt_client.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

// 2D Array: NUM_PLUGS x MAX_SCHEDULES_PER_PLUG
static ScheduleEntry schedules[NUM_PLUGS][MAX_SCHEDULES_PER_PLUG];

static uint8_t lastCheckedMinute = 255;
static int lastCheckedDay = -1;

// ── LittleFS persistence ──────────────────────────────────────────────────────

static void persistSchedules() {
  if (!LittleFS.begin()) return;

  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (uint8_t p = 0; p < NUM_PLUGS; p++) {
    for (uint8_t s = 0; s < MAX_SCHEDULES_PER_PLUG; s++) {
      if (schedules[p][s].enabled || schedules[p][s].hour != 0 || schedules[p][s].minute != 0) {
        JsonObject obj = arr.createNestedObject();
        obj["plug"]        = p;
        obj["slot"]        = s;
        obj["enabled"]     = schedules[p][s].enabled;
        obj["hour"]        = schedules[p][s].hour;
        obj["minute"]      = schedules[p][s].minute;
        obj["turnOn"]      = schedules[p][s].turnOn;
        obj["repeatDaily"] = schedules[p][s].repeatDaily;
      }
    }
  }

  File file = LittleFS.open(FILE_SCHEDULES, "w");
  if (!file) {
    DEBUG_PRINTLN("[Schedule] Failed to open schedules.json for writing!");
    return;
  }
  serializeJson(doc, file);
  file.close();
  DEBUG_PRINTLN("[Schedule] Schedules persisted to flash.");
}

static void loadSchedules() {
  if (!LittleFS.begin()) return;
  if (!LittleFS.exists(FILE_SCHEDULES)) return;

  File file = LittleFS.open(FILE_SCHEDULES, "r");
  if (!file) return;

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    DEBUG_PRINTLN("[Schedule] Failed to parse schedules.json!");
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject obj : arr) {
    uint8_t plug = obj["plug"] | 0;
    uint8_t slot = obj["slot"] | 0;
    if (plug >= NUM_PLUGS || slot >= MAX_SCHEDULES_PER_PLUG) continue;

    schedules[plug][slot].enabled     = obj["enabled"]     | false;
    schedules[plug][slot].hour        = obj["hour"]        | 0;
    schedules[plug][slot].minute      = obj["minute"]      | 0;
    schedules[plug][slot].turnOn      = obj["turnOn"]      | false;
    schedules[plug][slot].repeatDaily = obj["repeatDaily"] | true;
    schedules[plug][slot].firedToday  = false;

    DEBUG_PRINTF("[Schedule] Plug %u Slot %u: %s at %02u:%02u (%s)\n",
                 plug + 1, slot + 1,
                 schedules[plug][slot].enabled ? "ENABLED" : "disabled",
                 schedules[plug][slot].hour,
                 schedules[plug][slot].minute,
                 schedules[plug][slot].repeatDaily ? "daily" : "once");
  }
}

// ── Public API ────────────────────────────────────────────────────────────────

void initScheduling() {
  for (uint8_t p = 0; p < NUM_PLUGS; p++) {
    for (uint8_t s = 0; s < MAX_SCHEDULES_PER_PLUG; s++) {
      schedules[p][s] = { false, 0, 0, false, true, false };
    }
  }
  loadSchedules();
  DEBUG_PRINTLN("[Schedule] Initialized multi-slot scheduling.");
}

void processSchedules() {
  if (!isTimeValid()) return;

  time_t epoch = getCurrentEpoch();
  int currentDay = day(epoch);

  if (currentDay != lastCheckedDay) {
    lastCheckedDay = currentDay;
    for (uint8_t p = 0; p < NUM_PLUGS; p++) {
      for (uint8_t s = 0; s < MAX_SCHEDULES_PER_PLUG; s++) {
        schedules[p][s].firedToday = false;
      }
    }
  }

  uint8_t h = getCurrentHour();
  uint8_t m = getCurrentMinute();

  if (m == lastCheckedMinute) return;
  lastCheckedMinute = m;

  for (uint8_t p = 0; p < NUM_PLUGS; p++) {
    for (uint8_t s = 0; s < MAX_SCHEDULES_PER_PLUG; s++) {
      ScheduleEntry& entry = schedules[p][s];
      if (!entry.enabled)    continue;
      if (entry.firedToday)  continue;
      if (entry.hour != h)   continue;
      if (entry.minute != m) continue;

      DEBUG_PRINTF("[Schedule] Plug %u Slot %u firing at %02u:%02u -> %s\n",
                   p + 1, s + 1, h, m, entry.turnOn ? "ON" : "OFF");
      setPlug(p, entry.turnOn);
      entry.firedToday = true;

      if (!entry.repeatDaily) {
        entry = { false, 0, 0, false, true, false };
        persistSchedules();
        publishScheduleState(p);
      }
    }
  }
}

void saveLocalSchedule(uint8_t plugIndex, uint8_t slotIndex, uint8_t h, uint8_t m,
                       bool turnOn, bool repeatDaily) {
  if (plugIndex >= NUM_PLUGS || slotIndex >= MAX_SCHEDULES_PER_PLUG) return;

  schedules[plugIndex][slotIndex] = {
    true,        // enabled
    h,
    m,
    turnOn,
    repeatDaily,
    false        // firedToday
  };

  lastCheckedMinute = 255;
  persistSchedules();
  publishScheduleState(plugIndex);
  DEBUG_PRINTF("[Schedule] Plug %u Slot %u saved: %02u:%02u -> %s (%s)\n",
               plugIndex + 1, slotIndex + 1, h, m,
               turnOn ? "ON" : "OFF",
               repeatDaily ? "daily" : "once");
}

void disableLocalSchedule(uint8_t plugIndex, uint8_t slotIndex) {
  if (plugIndex >= NUM_PLUGS || slotIndex >= MAX_SCHEDULES_PER_PLUG) return;
  schedules[plugIndex][slotIndex].enabled = false;
  persistSchedules();
  publishScheduleState(plugIndex);
  DEBUG_PRINTF("[Schedule] Plug %u Slot %u disabled.\n", plugIndex + 1, slotIndex + 1);
}

void disableAllSchedules(uint8_t plugIndex) {
  if (plugIndex >= NUM_PLUGS) return;
  for (uint8_t s = 0; s < MAX_SCHEDULES_PER_PLUG; s++) {
    schedules[plugIndex][s].enabled = false;
  }
  persistSchedules();
  publishScheduleState(plugIndex);
  DEBUG_PRINTF("[Schedule] Plug %u all schedules disabled.\n", plugIndex + 1);
}

void applyMqttSchedule(uint8_t plugIndex, const char* jsonPayload) {
  if (plugIndex >= NUM_PLUGS || !jsonPayload) return;

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, jsonPayload);

  if (err) {
    DEBUG_PRINTF("[Schedule] MQTT payload parse error for Plug %u: %s\n",
                 plugIndex + 1, err.c_str());
    return;
  }

  // Clear existing slots for this plug before applying inbound payload
  for (uint8_t s = 0; s < MAX_SCHEDULES_PER_PLUG; s++) {
    schedules[plugIndex][s] = { false, 0, 0, false, true, false };
  }

  if (doc.is<JsonArray>()) {
    JsonArray arr = doc.as<JsonArray>();
    uint8_t slot = 0;
    for (JsonObject obj : arr) {
      if (slot >= MAX_SCHEDULES_PER_PLUG) break;

      const char* timeStr = obj["t"] | "00:00";
      bool turnOn         = obj["a"] | 0;
      bool repeatDaily    = obj["r"] | 1;
      bool enabled        = obj["e"] | 1;

      uint8_t h = 0, m = 0;
      int ih = 0, im = 0;
      if (sscanf(timeStr, "%d:%d", &ih, &im) == 2) {
        h = (uint8_t)constrain(ih, 0, 23);
        m = (uint8_t)constrain(im, 0, 59);
      }

      schedules[plugIndex][slot] = {
        enabled,
        h, m,
        turnOn,
        repeatDaily,
        false
      };
      slot++;
    }
  } else if (doc.is<JsonObject>()) {
    JsonObject obj = doc.as<JsonObject>();
    const char* timeStr = obj["t"] | "00:00";
    bool turnOn         = obj["a"] | 0;
    bool repeatDaily    = obj["r"] | 1;
    bool enabled        = obj["e"] | 1;

    uint8_t h = 0, m = 0;
    int ih = 0, im = 0;
    if (sscanf(timeStr, "%d:%d", &ih, &im) == 2) {
      h = (uint8_t)constrain(ih, 0, 23);
      m = (uint8_t)constrain(im, 0, 59);
    }

    schedules[plugIndex][0] = {
      enabled,
      h, m,
      turnOn,
      repeatDaily,
      false
    };
  }

  lastCheckedMinute = 255;
  persistSchedules();
  DEBUG_PRINTF("[Schedule] MQTT schedules applied to Plug %u.\n", plugIndex + 1);
}

const ScheduleEntry& getScheduleEntry(uint8_t plugIndex, uint8_t slotIndex) {
  static ScheduleEntry empty = { false, 0, 0, false, true, false };
  if (plugIndex >= NUM_PLUGS || slotIndex >= MAX_SCHEDULES_PER_PLUG) return empty;
  return schedules[plugIndex][slotIndex];
}
