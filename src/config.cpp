#include "config.h"

#include <Preferences.h>

Config config;

namespace {

constexpr const char* kNamespace = "gaugecam";
constexpr const char* kKey = "config";

template <typename T>
void take(JsonObjectConst in, const char* key, T& field) {
  if (in[key].is<T>()) field = in[key].as<T>();
}

void takeString(JsonObjectConst in, const char* key, String& field) {
  if (in[key].is<const char*>()) field = in[key].as<const char*>();
}

// Secrets: absent or empty keeps the stored value; "<key>_clear": true clears it.
void takeSecret(JsonObjectConst in, const char* key, String& field) {
  if (in[String(key) + "_clear"].as<bool>()) {
    field = "";
  } else if (in[key].is<const char*>() && strlen(in[key].as<const char*>()) > 0) {
    field = in[key].as<const char*>();
  }
}

}  // namespace

gauge::Settings Config::gaugeSettings() const {
  gauge::Settings s;
  s.pivot_mode = pivot_mode == "fixed" ? gauge::PivotMode::Fixed : gauge::PivotMode::Hub;
  s.pivot_x = pivot_x;
  s.pivot_y = pivot_y;
  s.points = points;
  return s;
}

void Config::toJson(JsonObject out, bool with_secrets) const {
  out["name"] = name;
  out["lang"] = lang;
  out["wifi_ssid"] = wifi_ssid;
  out["mqtt_host"] = mqtt_host;
  out["mqtt_port"] = mqtt_port;
  out["mqtt_user"] = mqtt_user;
  out["mqtt_prefix"] = mqtt_prefix;
  out["discovery_prefix"] = discovery_prefix;
  out["ha_discovery"] = ha_discovery;
  if (with_secrets) {
    out["wifi_verified"] = wifi_verified;  // device state, stored but not editable
    out["ui_password"] = ui_password;
    out["wifi_pass"] = wifi_pass;
    out["mqtt_pass"] = mqtt_pass;
    out["ap_password"] = ap_password;
  } else {
    out["ui_password_set"] = ui_password.length() > 0;
    out["wifi_pass_set"] = wifi_pass.length() > 0;
    out["mqtt_pass_set"] = mqtt_pass.length() > 0;
    out["ap_password_set"] = ap_password.length() > 0;
  }
  out["interval_s"] = interval_s;
  out["framesize"] = framesize;
  out["vflip"] = vflip;
  out["hmirror"] = hmirror;
  out["rotation"] = rotation;
  out["led_intensity"] = led_intensity;
  out["led_ms"] = led_ms;
  out["cam_brightness"] = cam_brightness;
  out["cam_contrast"] = cam_contrast;
  out["jpeg_quality"] = jpeg_quality;
  JsonArray r = out["roi"].to<JsonArray>();
  r.add(roi.x);
  r.add(roi.y);
  r.add(roi.w);
  r.add(roi.h);
  out["pivot_mode"] = pivot_mode;
  out["pivot_x"] = pivot_x;
  out["pivot_y"] = pivot_y;
  JsonArray pts = out["points"].to<JsonArray>();
  for (const gauge::Point& p : points) {
    JsonArray a = pts.add<JsonArray>();
    a.add(p.x);
    a.add(p.y);
    a.add(p.value);
  }
  out["unit"] = unit;
  out["min_contrast"] = min_contrast;
  out["samples"] = samples;
  out["max_jump"] = max_jump;
  out["tank_length_cm"] = tank_length_cm;
  out["tank_width_cm"] = tank_width_cm;
  out["tank_height_cm"] = tank_height_cm;
  out["level_offset_cm"] = level_offset_cm;
}

bool Config::fromJson(JsonObjectConst in, String& error) {
  Config next = *this;  // validate everything before applying
  takeString(in, "name", next.name);
  takeString(in, "lang", next.lang);
  takeString(in, "wifi_ssid", next.wifi_ssid);
  takeString(in, "mqtt_host", next.mqtt_host);
  take(in, "mqtt_port", next.mqtt_port);
  takeString(in, "mqtt_user", next.mqtt_user);
  takeString(in, "mqtt_prefix", next.mqtt_prefix);
  takeString(in, "discovery_prefix", next.discovery_prefix);
  take(in, "ha_discovery", next.ha_discovery);
  takeSecret(in, "ui_password", next.ui_password);
  takeSecret(in, "wifi_pass", next.wifi_pass);
  takeSecret(in, "mqtt_pass", next.mqtt_pass);
  takeSecret(in, "ap_password", next.ap_password);
  // New Wi-Fi credentials have not been tried yet.
  if (next.wifi_ssid != wifi_ssid || next.wifi_pass != wifi_pass) next.wifi_verified = false;
  take(in, "interval_s", next.interval_s);
  takeString(in, "framesize", next.framesize);
  take(in, "vflip", next.vflip);
  take(in, "hmirror", next.hmirror);
  take(in, "rotation", next.rotation);
  take(in, "led_intensity", next.led_intensity);
  take(in, "led_ms", next.led_ms);
  take(in, "cam_brightness", next.cam_brightness);
  take(in, "cam_contrast", next.cam_contrast);
  take(in, "jpeg_quality", next.jpeg_quality);
  if (in["roi"].is<JsonArrayConst>()) {
    JsonArrayConst r = in["roi"];
    if (r.size() != 4) {
      error = "roi must be [x, y, w, h]";
      return false;
    }
    next.roi = {r[0].as<int>(), r[1].as<int>(), r[2].as<int>(), r[3].as<int>()};
  }
  takeString(in, "pivot_mode", next.pivot_mode);
  take(in, "pivot_x", next.pivot_x);
  take(in, "pivot_y", next.pivot_y);
  if (in["points"].is<JsonArrayConst>()) {
    next.points.clear();
    for (JsonArrayConst a : in["points"].as<JsonArrayConst>()) {
      if (a.size() != 3) {
        error = "each point must be [x, y, value]";
        return false;
      }
      next.points.push_back({a[0].as<float>(), a[1].as<float>(), a[2].as<float>()});
    }
  }
  takeString(in, "unit", next.unit);
  take(in, "min_contrast", next.min_contrast);
  take(in, "samples", next.samples);
  take(in, "max_jump", next.max_jump);
  take(in, "tank_length_cm", next.tank_length_cm);
  take(in, "tank_width_cm", next.tank_width_cm);
  take(in, "tank_height_cm", next.tank_height_cm);
  take(in, "level_offset_cm", next.level_offset_cm);

  if (next.lang != "fr" && next.lang != "en") {
    error = "lang must be fr or en";
  } else if (next.pivot_mode != "hub" && next.pivot_mode != "fixed") {
    error = "pivot_mode must be hub or fixed";
  } else if (next.framesize != "QVGA" && next.framesize != "VGA" && next.framesize != "SVGA") {
    error = "framesize must be QVGA, VGA or SVGA";
  } else if (next.rotation % 90 != 0 || next.rotation > 270) {
    error = "rotation must be 0, 90, 180 or 270";
  } else if (next.interval_s < 60) {
    error = "interval_s must be at least 60";
  } else if (next.jpeg_quality < 4 || next.jpeg_quality > 63) {
    error = "jpeg_quality must be between 4 and 63";
  } else if (next.cam_brightness < -2 || next.cam_brightness > 2 ||
             next.cam_contrast < -2 || next.cam_contrast > 2) {
    error = "camera brightness and contrast must be between -2 and 2";
  } else if (next.samples < 1 || next.samples > 5) {
    error = "samples must be between 1 and 5";
  } else if (next.max_jump < 0) {
    error = "max_jump must be 0 (off) or more";
  } else if (next.ap_password.length() && (next.ap_password.length() < 8 || next.ap_password.length() > 63)) {
    error = "the access point password needs 8 to 63 characters";
  } else if (next.discovery_prefix.length() == 0 || next.discovery_prefix.indexOf('#') >= 0 ||
             next.discovery_prefix.indexOf('+') >= 0) {
    error = "invalid discovery prefix";
  } else if (next.mqtt_prefix.length() == 0 || next.mqtt_prefix.indexOf('#') >= 0 ||
             next.mqtt_prefix.indexOf('+') >= 0) {
    error = "invalid MQTT prefix";
  }
  if (error.length()) return false;
  *this = next;
  return true;
}

bool Config::load() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false;
  const size_t len = prefs.getBytesLength(kKey);
  if (len == 0) {
    prefs.end();
    return false;
  }
  std::vector<char> buf(len);
  prefs.getBytes(kKey, buf.data(), len);
  prefs.end();
  JsonDocument doc;
  if (deserializeJson(doc, buf.data(), len)) return false;
  String error;
  if (!fromJson(doc.as<JsonObjectConst>(), error)) return false;
  wifi_verified = doc["wifi_verified"] | false;
  return true;
}

bool Config::save() const {
  JsonDocument doc;
  toJson(doc.to<JsonObject>(), true);
  String text;
  serializeJson(doc, text);
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.putBytes(kKey, text.c_str(), text.length()) == text.length();
  prefs.end();
  return ok;
}
