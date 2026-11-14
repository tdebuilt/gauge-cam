// Wi-Fi station with a fallback access point and captive portal.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

void netBegin(const String& hostname);
void netLoop();
// Blocking attempt (about 20 s); saves the credentials on success.
bool netConnect(const String& ssid, const String& pass);
bool netConnected();
bool netApActive();
String netUrl();
void netToJson(JsonObject out);
