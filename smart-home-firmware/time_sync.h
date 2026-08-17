#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>

/**
 * Module: time_sync.h / time_sync.cpp
 * Responsibility: NTP time synchronisation and wall-clock validity tracking.
 *
 * IMPORTANT hardware limitation:
 *   This board has NO battery-backed RTC chip. Wall-clock time is only valid
 *   after a successful NTP sync over WiFi. On every power-up timeIsValid is
 *   false until the first successful sync. Code that depends on wall-clock time
 *   (scheduling.cpp) MUST check timeIsValid before acting.
 *
 *   Delay-mode timers (timers.cpp) use millis() and are UNAFFECTED by this
 *   limitation — they work correctly even without WiFi.
 */

/**
 * Initialises the NTP client.  Does NOT attempt a sync immediately; call
 * processTimeSync() in the main loop.
 */
void initTimeSync();

/**
 * Must be called in the main loop.  Performs a re-sync every
 * NTP_UPDATE_INTERVAL_MS while WiFi is connected.
 */
void processTimeSync();

/**
 * Returns true if a successful NTP sync has occurred since the last boot.
 * Always false after a cold start until WiFi connects and NTP responds.
 */
bool isTimeValid();

/**
 * Returns current hour (0-23) from the time library.
 * Only meaningful when isTimeValid() == true.
 */
uint8_t getCurrentHour();

/**
 * Returns current minute (0-59).
 * Only meaningful when isTimeValid() == true.
 */
uint8_t getCurrentMinute();

/**
 * Returns epoch seconds (time_t).
 * Only meaningful when isTimeValid() == true.
 */
time_t getCurrentEpoch();

/**
 * Resets timeIsValid to false (called on boot; need not be called elsewhere).
 */
void invalidateTime();

#endif // TIME_SYNC_H
