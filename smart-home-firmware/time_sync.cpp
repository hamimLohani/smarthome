/**
 * Module: time_sync.cpp
 * Responsibility: NTP synchronisation and wall-clock validity.
 *
 * Hardware limitation (intentional, documented):
 *   There is no RTC chip on this board.  After a power cycle timeIsValid is
 *   false until at least one NTP sync succeeds.  The millis()-based Delay
 *   timer is not affected.  Schedule-mode timers silently skip firing when
 *   timeIsValid is false, and the on-device menu warns the user before
 *   letting them configure a schedule without a valid time.
 */

#include "time_sync.h"
#include "config.h"
#include "wifi_setup.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>   // PaulStoffregen Time library

static WiFiUDP ntpUDP;
static NTPClient ntpClient(ntpUDP, NTP_SERVER, NTP_OFFSET_SEC, NTP_UPDATE_INTERVAL_MS);

// timeIsValid must start false on every boot — no assumptions about stored time.
static bool timeIsValid = false;
static unsigned long lastSyncAttempt = 0;

void initTimeSync() {
  ntpClient.begin();
  timeIsValid = false;   // Explicit reset — make it obvious this is intentional
  DEBUG_PRINTLN("[TimeSync] Initialized. Time NOT yet valid (no NTP sync).");
}

void processTimeSync() {
  if (!isWiFiConnected()) return;

  unsigned long now = millis();

  // Retry every 10 seconds if time is not yet valid; otherwise sync every hour
  unsigned long interval = timeIsValid ? NTP_UPDATE_INTERVAL_MS : 10000;

  if (lastSyncAttempt != 0 && (now - lastSyncAttempt < interval)) {
    return;
  }

  lastSyncAttempt = now;

  // Use forceUpdate to bypass NTPClient's internal update interval check
  bool updated = ntpClient.forceUpdate();
  if (updated) {
    // Push epoch time into PaulStoffregen TimeLib so hour()/minute() work
    setTime((time_t)ntpClient.getEpochTime());
    timeIsValid = true;
    DEBUG_PRINTF("[TimeSync] NTP sync OK — %02d:%02d:%02d\n",
                 hour(), minute(), second());
  } else {
    DEBUG_PRINTLN("[TimeSync] NTP update failed.");
    // Do NOT flip timeIsValid back to false on a temporary failure;
    // the TimeLib elapsed-time tracking keeps the clock plausible for a while.
  }
}

bool isTimeValid() {
  return timeIsValid;
}

uint8_t getCurrentHour() {
  return (uint8_t)hour();
}

uint8_t getCurrentMinute() {
  return (uint8_t)minute();
}

time_t getCurrentEpoch() {
  return now();
}

void invalidateTime() {
  timeIsValid = false;
}
