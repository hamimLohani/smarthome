#ifndef RELAYS_H
#define RELAYS_H

#include <Arduino.h>

/**
 * Module: relays.h / relays.cpp
 * Responsibility: Manages all relay GPIO writes and the in-memory plug state array.
 *
 * IMPORTANT: All relay state changes MUST go through setPlug() — never call
 * digitalWrite() on a relay pin directly. This keeps state, MQTT, and display
 * always in sync.
 */

/**
 * Initializes relay GPIO pins to OUTPUT and restores the last saved state
 * from LittleFS so relays survive a WiFi disconnect / reboot gracefully.
 */
void initRelays();

/**
 * The single relay control entrypoint. 
 * - Drives the GPIO respecting RELAY_ACTIVE_LOW
 * - Updates the in-memory state array
 * - Publishes the new state to MQTT (if connected)
 * - Refreshes the OLED display
 *
 * @param plugIndex  0-based plug index (0..NUM_PLUGS-1)
 * @param on         true = ON, false = OFF
 */
void setPlug(uint8_t plugIndex, bool on);

/**
 * Returns the current on/off state of a plug.
 * @param plugIndex  0-based plug index
 */
bool getPlugState(uint8_t plugIndex);

/**
 * Persists all plug states to LittleFS so they survive a reboot.
 * Called automatically by setPlug(); exposed here for explicit saves.
 */
void savePlugStates();

/**
 * Loads persisted plug states from LittleFS and applies them to relays.
 * Called during initRelays().
 */
void loadPlugStates();

#endif // RELAYS_H
