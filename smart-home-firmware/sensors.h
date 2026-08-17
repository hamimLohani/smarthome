#pragma once

#include <Arduino.h>

/**
 * Initializes the configured sensors (DHT11, Gas, Flame)
 */
void initSensors();

/**
 * Periodically reads the sensors and publishes to MQTT if connected and values changed
 */
void processSensors();
