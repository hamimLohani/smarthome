#ifndef IDENTITY_H
#define IDENTITY_H

#include <Arduino.h>

extern String deviceId;
extern String deviceSecret;
extern String pairedUid;
extern bool registered;
extern int lastRegHttpCode;

/**
 * Mounts LittleFS and attempts to load the device identity (ID and secret)
 * from FILE_IDENTITY. If missing and fallback is allowed, self-generates one.
 * Returns true if identity is loaded (either from file or generated).
 */
bool loadIdentity();

/**
 * Saves the provided device identity to LittleFS.
 */
void saveIdentity(const String& id, const String& secret, const String& pUid = "", bool reg = false);

/**
 * Registers the device credentials in Firebase Realtime Database.
 * Returns true if registration succeeded or already registered.
 */
bool registerDeviceInFirebase();

/**
 * Generates a fallback identity based on the ESP8266 Chip ID.
 */
void generateFallbackIdentity();

/**
 * Performs a factory reset by clearing LittleFS and formatting.
 */
void performFactoryReset();

#endif // IDENTITY_H
