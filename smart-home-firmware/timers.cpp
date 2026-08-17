/**
 * Module: timers.cpp
 * Responsibility: millis()-based one-off per-plug countdown timers (Delay Mode).
 *
 * Timer state does NOT survive a reboot — intentional, documented.
 * See timers.h for full design rationale.
 */

#include "timers.h"
#include "config.h"
#include "relays.h"
#include "mqtt_client.h"

struct DelayTimer {
  bool     active;
  bool     turnOn;           // Action when timer fires
  uint32_t durationSec;      // Original duration (stored for MQTT reporting)
  unsigned long startMillis; // millis() snapshot when timer was started
};

// One timer slot per plug — index 0..NUM_PLUGS-1
static DelayTimer timers[NUM_PLUGS];

void initTimers() {
  for (uint8_t i = 0; i < NUM_PLUGS; i++) {
    timers[i] = { false, false, 0, 0 };
  }
  DEBUG_PRINTLN("[Timers] Initialized (all cleared — timers do not survive reboot).");
}

void processTimers() {
  unsigned long now = millis();

  for (uint8_t i = 0; i < NUM_PLUGS; i++) {
    if (!timers[i].active) continue;

    // Calculate elapsed time safely (handles millis() overflow after ~49 days)
    unsigned long elapsed = now - timers[i].startMillis;
    unsigned long durationMs = (unsigned long)timers[i].durationSec * 1000UL;

    if (elapsed >= durationMs) {
      // Timer expired — fire the plug action through setPlug()
      DEBUG_PRINTF("[Timers] Plug %u timer fired -> %s\n",
                   i + 1, timers[i].turnOn ? "ON" : "OFF");

      timers[i].active = false;  // Clear before setPlug to avoid re-entry

      // Fire through the single control function, keeping state/MQTT/display in sync
      setPlug(i, timers[i].turnOn);

      // Notify MQTT that the timer is no longer active
      publishTimerState(i, false, 0, timers[i].turnOn);
    } else {
      // Timer still running — publish remaining time for live countdown in web UI
      uint32_t remaining = (uint32_t)((durationMs - elapsed) / 1000UL);
      publishTimerState(i, true, remaining, timers[i].turnOn);
    }
  }
}

void startDelayTimer(uint8_t plugIndex, uint32_t durationSec, bool turnOn) {
  if (plugIndex >= NUM_PLUGS) return;

  if (timers[plugIndex].active) {
    DEBUG_PRINTF("[Timers] Replacing existing timer on Plug %u\n", plugIndex + 1);
  }

  timers[plugIndex] = {
    true,
    turnOn,
    durationSec,
    millis()
  };

  DEBUG_PRINTF("[Timers] Plug %u: countdown %u sec -> will turn %s\n",
               plugIndex + 1, durationSec, turnOn ? "ON" : "OFF");

  // Immediate MQTT publish so web UI shows the new countdown
  publishTimerState(plugIndex, true, durationSec, turnOn);
}

void cancelDelayTimer(uint8_t plugIndex) {
  if (plugIndex >= NUM_PLUGS) return;
  if (!timers[plugIndex].active) return;

  timers[plugIndex].active = false;
  DEBUG_PRINTF("[Timers] Plug %u timer cancelled.\n", plugIndex + 1);

  publishTimerState(plugIndex, false, 0, timers[plugIndex].turnOn);
}

bool isTimerActive(uint8_t plugIndex) {
  if (plugIndex >= NUM_PLUGS) return false;
  return timers[plugIndex].active;
}

uint32_t getTimerRemainingSeconds(uint8_t plugIndex) {
  if (plugIndex >= NUM_PLUGS || !timers[plugIndex].active) return 0;

  unsigned long elapsed = millis() - timers[plugIndex].startMillis;
  unsigned long durationMs = (unsigned long)timers[plugIndex].durationSec * 1000UL;

  if (elapsed >= durationMs) return 0;
  return (uint32_t)((durationMs - elapsed) / 1000UL);
}

bool getTimerAction(uint8_t plugIndex) {
  if (plugIndex >= NUM_PLUGS) return false;
  return timers[plugIndex].turnOn;
}
