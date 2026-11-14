// MQTT client with Home Assistant discovery.
#pragma once

#include <Arduino.h>

#include "config.h"
#include "reader.h"

void mqttBegin(const String& device_id);
// Call after a configuration change: reconnects and republishes discovery.
void mqttReconfigure();
void mqttLoop();
bool mqttConnected();
void mqttPublishReading(const Reading& attempt, const Reading& good);
void mqttPublishPhoto();
// Set when Home Assistant pressed the "read now" button.
bool mqttTakeReadRequest();
