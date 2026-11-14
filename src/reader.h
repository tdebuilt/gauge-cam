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
  float volume_l = NAN;  // when the tank is described
  float percent = NAN;
  bool suspect = false;  // read fine but held back by the jump filter
  uint32_t at_ms = 0;
  uint32_t at_epoch = 0;  // 0 while the clock is not set
};

// A refused reading kept with its photo, to see what went wrong.
struct Failure {
  uint32_t id = 0;  // 0: empty slot
  Reading reading;
  uint8_t* jpeg = nullptr;  // PSRAM, may be null (no photo)
  size_t size = 0;
};

// Reads the last photo with the given configuration.
Reading readPhoto(const Config& cfg);
void readingToJson(const Reading& r, const Config& cfg, JsonObject out);

// Last attempt (may be a failure) and last successful reading.
const Reading& lastAttempt();
const Reading& lastGood();
void storeReading(const Reading& r);

// Keeps the last few refused readings, with a copy of the photo if given.
void keepFailure(const Reading& r, const uint8_t* jpeg, size_t size);
// Newest first.
void failuresToJson(const Config& cfg, JsonArray out);
const Failure* findFailure(uint32_t id);
uint32_t lastFailureId();
