#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>

/**
 * Module: mqtt_client.h / mqtt_client.cpp
 * Responsibility: MQTT broker connection, subscriptions, publishing.
 *
 * Connection details:
 *   Host/port are compile-time constants from config.h (MQTT_HOST / MQTT_PORT).
 *   This is intentional — the broker address is identical for every unit.
 *   Per-device credentials (client ID, username, password) come from
 *   the identity loaded by identity.cpp.
 *
 * Topic scheme (must match the website exactly):
 *   Subscribe:
 *     users/+/devices/{deviceId}/plug/+/set          — plug on/off command
 *     users/+/devices/{deviceId}/plug/+/timer/set    — start/cancel delay timer
 *     users/+/devices/{deviceId}/plug/+/schedule     — recurring schedule config
 *
 *   Publish (retained):
 *     users/{uid}/devices/{deviceId}/plug/{n}/state        — current on/off
 *     users/{uid}/devices/{deviceId}/plug/{n}/timer/state  — countdown state
 *     users/{uid}/devices/{deviceId}/status               — "online" / LWT "offline"
 *
 * Pairing:
 *   The firmware does not know the user's UID until after pairing.  It uses
 *   a wildcard '+' on inbound subscribe topics so it can respond before
 *   pairing.  On the first successful command received, the UID is extracted
 *   from the topic and cached so retained publishes use the correct path.
 */

/**
 * Initialises the PubSubClient and sets up the MQTT broker connection.
 * Does NOT connect immediately; connection is established in processMqtt().
 */
void initMqtt();

/**
 * Must be called every loop iteration.
 * Handles keep-alive, incoming messages, and reconnection with exponential back-off.
 */
void processMqtt();

/**
 * Publishes the retained on/off state for one plug.
 * Called by relays.cpp via setPlug().
 */
void publishPlugState(uint8_t plugIndex, bool on);

/**
 * Publishes the retained timer/state for one plug.
 * Called by timers.cpp.
 *
 * @param active          true if countdown is running
 * @param remainingSec    seconds left (0 if !active)
 * @param action          the plug action that will fire (ON/OFF)
 */
void publishTimerState(uint8_t plugIndex, bool active,
                       uint32_t remainingSec, bool action);

/**
 * Publishes retained schedule configuration array for one plug to MQTT.
 */
void publishScheduleState(uint8_t plugIndex);

/**
 * Returns true if the MQTT client is currently connected to the broker.
 */
bool isMqttConnected();

/**
 * Stores the paired user UID so retained publishes use the full topic path.
 * Called the first time a command topic is received that contains a UID.
 */
void setPairedUid(const String& uid);

/**
 * Performs unpairing steps: clears retained messages on MQTT under the current UID,
 * resets the ownership claim in Firebase database, and sets local paired UID to empty.
 */
void unpairDeviceMqtt();

#endif // MQTT_CLIENT_H
