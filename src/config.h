// Persistent device configuration (JSON in NVS).
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

#include "gauge.h"

// Steps of the first set-up, in order.
constexpr const char* kSetupSteps[] = {"photo", "frame", "grad", "check", "tank", "ha"};
constexpr size_t kSetupCount = sizeof(kSetupSteps) / sizeof(kSetupSteps[0]);

struct Config {
  // Device
  String name = "Gauge Cam";
  String lang = "fr";  // language of the Home Assistant entity names: fr, en
  String ui_password;  // optional HTTP basic auth (user "admin")

  // Network
  String wifi_ssid;
  String wifi_pass;
  // Set once the device joined wifi_ssid; until then the fallback access
  // point stays open so that a typo in the password can be fixed.
  bool wifi_verified = false;
  // WPA2 password of the fallback access point (8 to 63 characters). Empty:
  // no access point once Wi-Fi works; change Wi-Fi over USB instead.
  String ap_password;
  String mqtt_host;
  uint16_t mqtt_port = 1883;
  String mqtt_user;
  String mqtt_pass;
  String mqtt_prefix = "gauge-cam";
  String discovery_prefix = "homeassistant";
  bool ha_discovery = true;
  String ntp_server = "pool.ntp.org";  // empty: no clock (no timestamps)
  bool update_check = true;  // daily look for a new release
  // Where releases are looked for: empty = GitHub; otherwise the address of a
  // local mirror serving latest.json (see README), for devices without Internet.
  String update_url;

  // Capture
  uint32_t interval_s = 3600;
  String framesize = "VGA";  // QVGA, VGA, SVGA
  bool vflip = false;
  bool hmirror = false;
  uint16_t rotation = 0;  // clockwise degrees (0, 90, 180, 270), after the flips
  uint8_t led_intensity = 128;  // 0 = off
  uint16_t led_ms = 400;
  int8_t cam_brightness = 0;  // -2..2
  int8_t cam_contrast = 0;  // -2..2
  // Automatic exposure and gain, or fixed values for identical photos.
  bool exposure_auto = true;
  uint16_t exposure = 300;  // 0..1200 (sensor AEC value)
  uint8_t gain = 0;  // 0..30
  uint8_t jpeg_quality = 10;  // 4 (best) .. 63

  // Dial, in full-image pixel coordinates
  gauge::Rect roi;
  String pivot_mode = "hub";  // hub or fixed
  float pivot_x = -1;
  float pivot_y = -1;
  std::vector<gauge::Point> points;  // dial order
  String unit = "cm";
  float min_contrast = 20;
  uint8_t samples = 3;  // photos per scheduled reading, the median is kept
  float max_jump = 5;  // larger level change must be confirmed (0 = off)

  // Tank: box, hcyl (horizontal cylinder), vcyl (vertical cylinder) or
  // table (level -> litres); inner dimensions in cm
  String tank_shape = "box";
  float tank_length_cm = 0;
  float tank_width_cm = 0;
  float tank_height_cm = 0;
  float tank_diameter_cm = 0;
  std::vector<std::pair<float, float>> tank_table;
  float level_offset_cm = 0;  // level when the gauge reads 0

  // First set-up in the interface: bit i of each mask is step kSetupSteps[i],
  // done or put off ("later"). A step in neither is still to do.
  uint8_t setup_done = 0;
  uint8_t setup_later = 0;

  bool dialReady() const { return pivot_x >= 0 && points.size() >= 2; }
  gauge::Tank tank() const;
  bool tankReady() const { return tank().ready(); }
  gauge::Settings gaugeSettings() const;

  // Secrets are never sent back: the UI only learns whether they are set.
  void toJson(JsonObject out, bool with_secrets) const;
  // Applies the keys present in `in`; an empty secret keeps the stored one,
  // "<secret>_clear": true erases it.
  // Returns false and sets `error` when a value is invalid.
  bool fromJson(JsonObjectConst in, String& error);

  // Applies a configuration stored by save() over this one (the defaults at
  // boot), secrets and device state included.
  bool fromStored(const char* text, size_t len);
  bool load();
  bool save() const;
};

extern Config config;

// Changes from `before` to `after`, by top-level key: {"key": {"before": ...,
// "after": ...}}; a secret only shows as "changed", never its value.
void configDiff(const Config& before, const Config& after, JsonObject out);

// The settings before the last change, kept for /api/config/undo.
bool configKeepPrevious(const Config& before);
bool configLoadPrevious(Config& out);
// When the settings last changed (epoch, 0: unknown or never).
uint32_t configChangedAt();
void configSetChangedAt(uint32_t epoch);
