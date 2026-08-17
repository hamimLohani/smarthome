#include "sensors.h"
#include "config.h"
#include "mqtt_client.h"

#if ENABLE_DHT11
#include <DHT.h>
DHT dht(PIN_DHT11, DHT11);
#endif

static unsigned long lastSensorPoll = 0;
static const unsigned long SENSOR_POLL_INTERVAL = 2000; // Poll every 2 seconds

static float lastTemp = NAN;
static float lastHum = NAN;
static int lastGas = -1;
static int lastFlame = -1;
static bool wasMqttConnected = false;

void initSensors() {
#if ENABLE_DHT11
  dht.begin();
#endif

#if ENABLE_GAS
  pinMode(PIN_GAS, INPUT_PULLUP);
#endif

#if ENABLE_FLAME
  pinMode(PIN_FLAME, INPUT_PULLUP);
#endif
  
  DEBUG_PRINTLN("[Sensors] Initialized.");
}

void processSensors() {
  unsigned long now = millis();
  
  // Throttle reads
  if (now - lastSensorPoll < SENSOR_POLL_INTERVAL) {
    return;
  }
  lastSensorPoll = now;

  bool changed = false;
  bool mqttConnectedNow = isMqttConnected();
  
  // Force a publish if we just connected to the MQTT broker
  if (mqttConnectedNow && !wasMqttConnected) {
    changed = true;
  }
  wasMqttConnected = mqttConnectedNow;

  float currentTemp = NAN;
  float currentHum = NAN;
  int currentGas = -1;
  int currentFlame = -1;

#if ENABLE_DHT11
  currentTemp = dht.readTemperature();
  currentHum = dht.readHumidity();
  
  if (!isnan(currentTemp) && !isnan(currentHum)) {
    // Only trigger a publish if temperature changes by 0.5C or humidity by 1%
    if (isnan(lastTemp) || abs(currentTemp - lastTemp) >= 0.5 || abs(currentHum - lastHum) >= 1.0) {
      changed = true;
      lastTemp = currentTemp;
      lastHum = currentHum;
    }
  }
#endif

#if ENABLE_GAS
  currentGas = digitalRead(PIN_GAS);
  if (currentGas != lastGas) {
    changed = true;
    lastGas = currentGas;
  }
#endif

#if ENABLE_FLAME
  currentFlame = digitalRead(PIN_FLAME);
  if (currentFlame != lastFlame) {
    changed = true;
    lastFlame = currentFlame;
  }
#endif

  // If state changed and we are online, publish the current state
  if (changed && mqttConnectedNow) {
    publishSensors(lastTemp, lastHum, lastGas, lastFlame);
  }
}
