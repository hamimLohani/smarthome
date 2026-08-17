#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <Arduino.h>

extern bool offlineMode;
extern bool inWiFiPortal;
extern int wifiConnectAttempts;

/**
 * Initializes WiFi configuration. Loads offline mode preference,
 * checks for saved credentials, and attempts connection.
 */
void initWiFi();

/**
 * Periodically called in the main loop to manage WiFi reconnection
 * and failure fallback.
 */
void processWiFi();

/**
 * Starts the WiFiManager captive portal AP.
 */
void startWiFiPortal();

/**
 * Clears saved WiFi credentials and marks offlineMode as true.
 */
void forgetWiFiCredentials();

/**
 * Returns true if WiFi is connected.
 */
bool isWiFiConnected();

/**
 * Loads the offline mode preference from FILE_WIFI_PREF.
 */
void loadWiFiPreferences();

/**
 * Saves the offline mode preference to FILE_WIFI_PREF.
 */
void saveWiFiPreferences();

/**
 * Returns the WiFi captive-portal AP name (e.g. "SmartHome-Setup-8C9F").
 * Populated during initWiFi(), valid afterwards.
 */
const char* getApName();

#endif // WIFI_SETUP_H
