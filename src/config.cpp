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

int setupIndex(const char* step) {
  for (size_t i = 0; i < kSetupCount; ++i) {
    if (strcmp(kSetupSteps[i], step) == 0) return i;
  }
  return -1;
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

gauge::Tank Config::tank() const {
  gauge::Tank t;
  if (tank_shape == "hcyl") t.shape = gauge::TankShape::HorizontalCylinder;
  if (tank_shape == "vcyl") t.shape = gauge::TankShape::VerticalCylinder;
  if (tank_shape == "table") t.shape = gauge::TankShape::Table;
  t.length_cm = tank_length_cm;
  t.width_cm = tank_width_cm;
  t.height_cm = tank_height_cm;
  t.diameter_cm = tank_diameter_cm;
  t.table = tank_table;
  return t;
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
  out["ntp_server"] = ntp_server;
  out["update_check"] = update_check;
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
  out["exposure_auto"] = exposure_auto;
  out["exposure"] = exposure;
  out["gain"] = gain;
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
  out["tank_shape"] = tank_shape;
  out["tank_length_cm"] = tank_length_cm;
  out["tank_width_cm"] = tank_width_cm;
  out["tank_height_cm"] = tank_height_cm;
  out["tank_diameter_cm"] = tank_diameter_cm;
  JsonArray table = out["tank_table"].to<JsonArray>();
  for (const auto& row : tank_table) {
    JsonArray a = table.add<JsonArray>();
    a.add(row.first);
    a.add(row.second);
  }
  out["level_offset_cm"] = level_offset_cm;
  JsonObject setup = out["setup"].to<JsonObject>();
  for (size_t i = 0; i < kSetupCount; ++i) {
    if (setup_done & (1 << i)) setup[kSetupSteps[i]] = "done";
    else if (setup_later & (1 << i)) setup[kSetupSteps[i]] = "later";
  }
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
  takeString(in, "ntp_server", next.ntp_server);
  take(in, "update_check", next.update_check);
  takeSecret(in, "ui_password", next.ui_password);
  takeSecret(in, "wifi_pass", next.wifi_pass);
  takeSecret(in, "mqtt_pass", next.mqtt_pass);
  takeSecret(in, "ap_password", next.ap_password);
  // The stored MQTT password must not follow the device to another server:
  // whoever can reach this interface could otherwise collect it.
  const bool mqtt_moved = next.mqtt_host != mqtt_host || next.mqtt_port != mqtt_port ||
                          next.mqtt_user != mqtt_user;
  const bool pass_given = in["mqtt_pass"].is<const char*>() && strlen(in["mqtt_pass"].as<const char*>()) > 0;
  if (mqtt_moved && mqtt_pass.length() && !pass_given && !in["mqtt_pass_clear"].as<bool>()) {
    error = "type the MQTT password again to change the server, port or user";
    return false;
  }
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
  take(in, "exposure_auto", next.exposure_auto);
  take(in, "exposure", next.exposure);
  take(in, "gain", next.gain);
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
  takeString(in, "tank_shape", next.tank_shape);
  take(in, "tank_diameter_cm", next.tank_diameter_cm);
  if (in["tank_table"].is<JsonArrayConst>()) {
    next.tank_table.clear();
    for (JsonArrayConst a : in["tank_table"].as<JsonArrayConst>()) {
      if (a.size() != 2) {
        error = "each tank table row must be [level, litres]";
        return false;
      }
      next.tank_table.push_back({a[0].as<float>(), a[1].as<float>()});
    }
  }
  take(in, "tank_length_cm", next.tank_length_cm);
  take(in, "tank_width_cm", next.tank_width_cm);
  take(in, "tank_height_cm", next.tank_height_cm);
  take(in, "level_offset_cm", next.level_offset_cm);
  // Set-up steps: "done", "later" or null (still to do); absent ones keep their state.
  if (in["setup"].is<JsonObjectConst>()) {
    for (JsonPairConst kv : in["setup"].as<JsonObjectConst>()) {
      const int i = setupIndex(kv.key().c_str());
      const char* state = kv.value().as<const char*>();
      const bool done = state && strcmp(state, "done") == 0;
      const bool later = state && strcmp(state, "later") == 0;
      if (i < 0 || (!kv.value().isNull() && !done && !later)) {
        error = "setup: steps are photo, frame, grad, check, tank, ha; states done, later or null";
        return false;
      }
      const uint8_t bit = 1 << i;
      next.setup_done = done ? next.setup_done | bit : next.setup_done & ~bit;
      next.setup_later = later ? next.setup_later | bit : next.setup_later & ~bit;
    }
  }

  bool table_ok = next.tank_table.size() <= 40;
  for (size_t i = 1; i < next.tank_table.size(); ++i) {
    table_ok = table_ok && next.tank_table[i].first > next.tank_table[i - 1].first &&
               next.tank_table[i].second >= next.tank_table[i - 1].second;
  }
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
  } else if (next.exposure > 1200 || next.gain > 30) {
    error = "exposure must be 0 to 1200 and gain 0 to 30";
  } else if (next.tank_shape != "box" && next.tank_shape != "hcyl" && next.tank_shape != "vcyl" &&
             next.tank_shape != "table") {
    error = "tank_shape must be box, hcyl, vcyl or table";
  } else if (next.tank_length_cm < 0 || next.tank_width_cm < 0 || next.tank_height_cm < 0 ||
             next.tank_diameter_cm < 0) {
    error = "tank dimensions cannot be negative";
  } else if (!table_ok) {
    error = "tank table: at most 40 rows, levels increasing, litres never decreasing";
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
  // Saved before the set-up existed by a device that already reads the dial:
  // what works counts as done, the rest waits on the home page.
  if (!doc["setup"].is<JsonObjectConst>() && dialReady()) {
    const bool parts[kSetupCount] = {true, true, true, true, tankReady(), mqtt_host.length() > 0};
    for (size_t i = 0; i < kSetupCount; ++i) (parts[i] ? setup_done : setup_later) |= 1 << i;
  }
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
