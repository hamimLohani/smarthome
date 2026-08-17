#ifndef SCHEDULING_H
#define SCHEDULING_H

#include <Arduino.h>

/**
 * Module: scheduling.h / scheduling.cpp
 * Responsibility: Per-plug recurring daily ON/OFF schedules with multi-slot support.
 *
 * IMPORTANT hardware limitation:
 *   Schedule Time requires a valid wall-clock time from NTP. If the device
 *   has never connected to WiFi, or has lost power long enough for
 *   timeIsValid to be false, scheduled events will NOT fire. This is by
 *   design — see time_sync.h.
 *
 * Each plug supports up to MAX_SCHEDULES_PER_PLUG independent slots.
 */

struct ScheduleEntry {
  bool    enabled;
  uint8_t hour;        // 0-23
  uint8_t minute;      // 0-59
  bool    turnOn;      // true = turn plug ON when firing
  bool    repeatDaily; // false = fires once then disables itself
  bool    firedToday;  // internal: prevents double-firing within the same minute
};

/**
 * Initialises schedule slots and loads any persisted schedules from LittleFS.
 */
void initScheduling();

/**
 * Must be called every loop iteration.
 * Evaluates all enabled schedule slots against current time and fires setPlug().
 */
void processSchedules();

/**
 * Saves a schedule entry for a specific slot on a plug.
 * Persists updated slots to LittleFS and publishes to MQTT.
 */
void saveLocalSchedule(uint8_t plugIndex, uint8_t slotIndex, uint8_t h, uint8_t m,
                       bool turnOn, bool repeatDaily);

/**
 * Disables a specific schedule slot on a plug.
 */
void disableLocalSchedule(uint8_t plugIndex, uint8_t slotIndex);

/**
 * Disables all schedule slots for a plug.
 */
void disableAllSchedules(uint8_t plugIndex);

/**
 * Applies schedule entries received via MQTT (JSON payload array from the website).
 * Parses up to MAX_SCHEDULES_PER_PLUG entries, updates local storage, and persists to LittleFS.
 */
void applyMqttSchedule(uint8_t plugIndex, const char* jsonPayload);

/**
 * Returns the current schedule entry for a plug slot (read-only).
 */
const ScheduleEntry& getScheduleEntry(uint8_t plugIndex, uint8_t slotIndex = 0);

#endif // SCHEDULING_H
