#ifndef TIMERS_H
#define TIMERS_H

#include <Arduino.h>

/**
 * Module: timers.h / timers.cpp
 * Responsibility: Per-plug one-off countdown timers (Delay Mode).
 *
 * DESIGN NOTES:
 *   - Implemented with millis()-based countdown; NOT dependent on wall-clock
 *     time or NTP.  Works correctly with no WiFi at all — the recommended
 *     automation option for offline users.
 *   - Only ONE active Delay timer per plug.  Starting a new one cancels the
 *     existing timer on that plug (replacement, not stacking).
 *   - Timer state intentionally does NOT survive a reboot or power loss.
 *     This is a deliberate design choice documented here so it is not mistaken
 *     for a bug:  millis() resets on boot, so any in-progress countdown
 *     cannot be reliably reconstructed.
 *   - On completion, the timer fires through setPlug() — exactly the same
 *     code path as a manual button press or MQTT command — keeping state,
 *     MQTT publication, and display always in sync.
 */

/**
 * Initialises the timer subsystem (clears all slots).
 * Call once from setup().
 */
void initTimers();

/**
 * Must be called every loop iteration.
 * Checks each active timer and fires setPlug() when the countdown expires.
 */
void processTimers();

/**
 * Starts (or replaces) a Delay countdown for the given plug.
 *
 * @param plugIndex      0-based plug index (0..NUM_PLUGS-1)
 * @param durationSec    Countdown duration in seconds
 * @param turnOn         true = turn plug ON when timer fires, false = turn OFF
 */
void startDelayTimer(uint8_t plugIndex, uint32_t durationSec, bool turnOn);

/**
 * Cancels any active Delay timer for the given plug.
 * No-op if no timer is running.
 */
void cancelDelayTimer(uint8_t plugIndex);

/**
 * Returns true if a Delay timer is currently running for this plug.
 */
bool isTimerActive(uint8_t plugIndex);

/**
 * Returns remaining seconds for an active timer, or 0 if none active.
 * Used by MQTT to report timer/state topic.
 */
uint32_t getTimerRemainingSeconds(uint8_t plugIndex);

/**
 * Returns the action that will fire when the timer expires.
 * Undefined if isTimerActive() returns false.
 */
bool getTimerAction(uint8_t plugIndex);

#endif // TIMERS_H
