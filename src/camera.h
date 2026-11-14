// OV2640 capture on the AI-Thinker ESP32-CAM, with the flash LED.
#pragma once

#include <Arduino.h>

#include "config.h"

struct Photo {
  uint8_t* jpeg = nullptr;  // PSRAM
  size_t size = 0;
  int width = 0;
  int height = 0;
  uint32_t taken_ms = 0;
};

bool cameraBegin(const Config& cfg);
// Applies orientation, exposure and resolution settings to the sensor.
void cameraApply(const Config& cfg);
// Lights the LED, lets the exposure settle and keeps the JPEG, rotated by
// cfg.rotation, as lastPhoto().
bool cameraCapture(const Config& cfg, String& error);
const Photo& lastPhoto();
void ledSet(uint8_t intensity);
