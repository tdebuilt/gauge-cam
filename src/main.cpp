// gauge-cam: reads a needle gauge (oil tank level) with an ESP32-CAM.
#include <Arduino.h>

#include "app.h"
#include "camera.h"
#include "config.h"
#include "generated/version.h"
#include "improv.h"
#include "mqtt.h"
#include "net.h"
#include "reader.h"
#include "web.h"

namespace {

String device_id;
bool camera_ok = false;
uint32_t next_read_ms = 0;
uint32_t restart_at_ms = 0;

void schedule(uint32_t in_ms) { next_read_ms = millis() + in_ms; }

bool improvConnect(const String& ssid, const String& pass) { return netConnect(ssid, pass); }

}  // namespace

String deviceId() { return device_id; }

bool appCapture(const Config& cfg, String& error) {
  if (!camera_ok) {
    error = "camera not initialised";
    return false;
  }
  if (&cfg == &config) return cameraCapture(config, error);
  cameraApply(cfg);
  const bool ok = cameraCapture(cfg, error);
  cameraApply(config);
  return ok;
}

Reading appRead(bool fresh_photo) {
  Reading r;
  String error;
  if (fresh_photo && !appCapture(config, error)) {
    r.at_ms = millis();
    r.error = error;
  } else {
    r = readPhoto(config);
  }
  storeReading(r);
  if (r.ok) {
    Serial.printf("Reading: %.1f %s\n", r.level, config.unit.c_str());
  } else {
    Serial.printf("Reading failed: %s\n", r.error.c_str());
  }
  mqttPublishReading(lastAttempt(), lastGood());
  if (fresh_photo) mqttPublishPhoto();
  return r;
}

void appConfigChanged(bool wifi_changed) {
  if (camera_ok) cameraApply(config);
  mqttReconfigure();
  schedule(5000);
  if (wifi_changed) appRestartSoon();
}

void appRestartSoon() { restart_at_ms = millis() + 1500; }

uint32_t appNextReadInS() {
  const int32_t left = static_cast<int32_t>(next_read_ms - millis());
  return left > 0 ? left / 1000 : 0;
}

void setup() {
  Serial.begin(115200);
  const uint64_t mac = ESP.getEfuseMac();
  char id[20];
  snprintf(id, sizeof id, "gauge-cam-%06x", static_cast<unsigned>(mac >> 24) & 0xFFFFFF);
  device_id = id;
  Serial.printf("\ngauge-cam %s (%s)\n", GAUGECAM_VERSION, device_id.c_str());

  if (!config.load()) Serial.println("No saved configuration, using defaults");
  camera_ok = psramFound() && cameraBegin(config);
  if (!camera_ok) Serial.println("Camera or PSRAM initialisation failed");

  improvBegin("gauge-cam", GAUGECAM_VERSION, device_id, improvConnect, netUrl);
  netBegin(device_id);
  mqttBegin(device_id);
  webBegin();
  schedule(10000);  // first reading once the network had time to come up
}

void loop() {
  improvLoop();
  netLoop();
  webLoop();
  mqttLoop();

  if (mqttTakeReadRequest()) next_read_ms = millis();
  if (static_cast<int32_t>(millis() - next_read_ms) >= 0) {
    schedule(config.interval_s * 1000UL);
    if (camera_ok && config.dialReady()) {
      appRead(true);
    } else if (camera_ok) {
      String error;
      if (appCapture(config, error)) mqttPublishPhoto();  // helps the first set-up
    }
  }
  if (restart_at_ms && static_cast<int32_t>(millis() - restart_at_ms) >= 0) ESP.restart();
  delay(2);
}
