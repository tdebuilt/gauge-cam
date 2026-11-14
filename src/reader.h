// Turns the last photo into a level, a volume and a percentage.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "gauge.h"

struct Reading {
  bool ok = false;
  String error;
  gauge::Result gauge;
  float level = NAN;  // gauge value plus offset, in the configured unit
  float volume_l = NAN;  // rectangular tank only
  float percent = NAN;
  uint32_t at_ms = 0;
};

// Reads the last photo with the given configuration.
Reading readPhoto(const Config& cfg);
void readingToJson(const Reading& r, const Config& cfg, JsonObject out);

// Last attempt (may be a failure) and last successful reading.
const Reading& lastAttempt();
const Reading& lastGood();
void storeReading(const Reading& r);
