// Improv Wi-Fi over serial: lets the browser flasher (ESP Web Tools) send
// the Wi-Fi credentials right after flashing. https://www.improv-wifi.com/serial/
#pragma once

#include <Arduino.h>

// `connect` tries the credentials and returns true once connected; it is
// expected to save them. `url` returns the device page to offer afterwards.
void improvBegin(const String& firmware, const String& version, const String& device_name,
                 bool (*connect)(const String& ssid, const String& pass),
                 String (*url)());
void improvLoop();
