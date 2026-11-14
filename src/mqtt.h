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
// Publishes the status and the diagnostics; the values only when `new_good`
// (or an earlier new reading has not reached the broker yet).
void mqttPublishReading(const Reading& attempt, const Reading& good, bool new_good);
// State of the Home Assistant firmware update entity.
void mqttPublishUpdate();
void mqttPublishPhoto();
// True once connected since boot.
bool mqttEverConnected();
// Connects once with `c`'s server settings, then disconnects; on failure
// `error` holds the broker's answer. Blocks up to about 10 s.
bool mqttTest(const Config& c, String& error);
// Set when Home Assistant pressed the "read now" button.
bool mqttTakeReadRequest();
