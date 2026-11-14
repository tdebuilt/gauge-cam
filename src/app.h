// Actions shared by the web server, MQTT and the scheduler (main.cpp).
#pragma once

#include <Arduino.h>

#include "reader.h"

String deviceId();
// Takes a photo with `cfg`, which may be an unsaved draft: the sensor gets the
// saved settings back afterwards.
bool appCapture(const Config& cfg, String& error);
// Optionally takes a new photo, reads it, keeps and publishes the result.
Reading appRead(bool fresh_photo);
// Applies a saved configuration: camera settings, MQTT discovery, schedule.
void appConfigChanged(bool wifi_changed);
void appRestartSoon();
uint32_t appNextReadInS();
