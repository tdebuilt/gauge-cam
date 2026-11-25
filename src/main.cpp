// gauge-cam: reads a needle gauge (oil tank level) with an ESP32-CAM.
#include <Arduino.h>

#include <esp_task_wdt.h>

#include <algorithm>
#include <vector>

#include "app.h"
#include "camera.h"
#include "config.h"
#include "diag.h"
#include "generated/version.h"
#include "improv.h"
#include "mqtt.h"
#include "net.h"
#include "ota.h"
#include "reader.h"
#include "web.h"

namespace {

String device_id;
bool camera_ok = false;
uint32_t next_read_ms = 0;
uint32_t restart_at_ms = 0;
gauge::JumpFilter jump_filter;
constexpr int kConfirmations = 2;  // readings that must agree with a jump
constexpr uint32_t kRecheckMs = 120000;  // next reading after a suspect one

// Self-healing: the device sits alone in a cellar.
constexpr uint32_t kWatchdogS = 60;  // the loop must come back within this
constexpr int kCameraRetries = 3;  // driver restarts before restarting the device
constexpr uint32_t kCameraRetryMs = 30000;
constexpr int kCaptureFailures = 3;  // captures in a row without a frame
constexpr uint8_t kCameraRestarts = 3;  // then stay up to show the error
constexpr uint32_t kOfflineRestartMs = 3600UL * 1000;  // without Wi-Fi or MQTT
constexpr uint32_t kNewFirmwareWifiMs = 5UL * 60 * 1000;  // or back to the previous one
int camera_retries = 0;
uint32_t last_camera_retry = 0;
bool camera_gave_up = false;
uint32_t wifi_down_since = 0;
uint32_t mqtt_down_since = 0;
uint32_t wifi_up_since = 0;

void schedule(uint32_t in_ms) { next_read_ms = millis() + in_ms; }

// The first reading after boot waits for NTP (2 min at most), so that Home
// Assistant gets its time instead of an unknown "last reading".
constexpr uint32_t kClockWaitMs = 120000;
bool clock_wait = true;

// Restarts for a camera fault, at most kCameraRestarts times since power on
// so that a dead camera does not hide the web interface behind restarts.
void cameraRestart(const String& why) {
  if (cameraRestartCount() >= kCameraRestarts) {
    if (!camera_gave_up) logMsg("Camera still failing after %u restarts: %s", kCameraRestarts, why.c_str());
    camera_gave_up = true;
    return;
  }
  setCameraRestartCount(cameraRestartCount() + 1);
  diagRestart(why);
}

void healthLoop() {
  esp_task_wdt_reset();
  const uint32_t now = millis();

  if (!camera_ok && psramFound() && camera_retries < kCameraRetries && now - last_camera_retry > kCameraRetryMs) {
    last_camera_retry = now;
    ++camera_retries;
    camera_ok = cameraRetry(config);
    logMsg("Camera start, attempt %d: %s", camera_retries + 1, camera_ok ? "ok" : "failed");
  }
  if (!camera_ok && psramFound() && camera_retries >= kCameraRetries) {
    cameraRestart("the camera does not start");
  }
  if (camera_ok && cameraFailuresInARow() >= kCaptureFailures) {
    cameraRestart("the camera stopped delivering photos");
  }

  const bool wifi = netConnected();
  if (wifi) {
    if (!wifi_up_since) wifi_up_since = now;
    wifi_down_since = 0;
  } else {
    wifi_up_since = 0;
    if (!wifi_down_since) wifi_down_since = now;
  }
  if (!wifi && config.wifi_verified && !netApInUse() && now - wifi_down_since > kOfflineRestartMs) {
    diagRestart("no Wi-Fi for 1 h");
  }
  if (wifi && config.mqtt_host.length() && mqttEverConnected() && !mqttConnected()) {
    if (!mqtt_down_since) mqtt_down_since = now;
    if (now - mqtt_down_since > kOfflineRestartMs) diagRestart("MQTT server unreachable for 1 h");
  } else {
    mqtt_down_since = 0;
  }

  // A new firmware is kept once it has run on Wi-Fi for a minute; if it
  // cannot join Wi-Fi, restarting brings the previous firmware back.
  if (otaPendingVerify()) {
    if (wifi && now - wifi_up_since > 60000) otaConfirm("Wi-Fi works");
    if (!wifi && config.wifi_verified && now > kNewFirmwareWifiMs) {
      diagRestart("the new firmware cannot join Wi-Fi, back to the previous one");
    }
  } else if (now > 60000) {
    otaConfirm("running");
  }
}

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

// Takes config.samples photos and keeps the reading of median level; a
// majority of the photos must be readable.
Reading readSamples() {
  std::vector<Reading> ok;
  Reading failure;
  for (int i = 0; i < config.samples; ++i) {
    String error;
    if (!appCapture(config, error)) {
      failure = Reading();
      failure.at_ms = millis();
      failure.at_epoch = epochNow();
      failure.error = error;
      if (!camera_ok) break;
      continue;
    }
    Reading r = readPhoto(config);
    if (r.ok) {
      ok.push_back(r);
    } else {
      failure = r;
    }
  }
  if (ok.size() < static_cast<size_t>(config.samples / 2 + 1)) {
    if (config.samples > 1 && failure.error.length()) {
      failure.error += " (" + String(ok.size()) + "/" + String(config.samples) + " photos read)";
    }
    return failure;
  }
  std::sort(ok.begin(), ok.end(), [](const Reading& a, const Reading& b) { return a.level < b.level; });
  return ok[ok.size() / 2];
}

// Holds back a level that jumped since the last accepted one until the
// following readings confirm it, and checks again soon.
void applyJumpFilter(Reading& r) {
  if (!r.ok) return;
  const float before = jump_filter.reference();
  if (jump_filter.feed(r.level, config.max_jump, kConfirmations) != gauge::JumpFilter::Verdict::Suspect) {
    return;
  }
  r.ok = false;
  r.suspect = true;
  r.error = "implausible jump from " + String(before, 1) + " to " + String(r.level, 1) + " " +
            config.unit + ", checking again (" + String(jump_filter.pending()) + "/" +
            String(kConfirmations + 1) + ")";
  schedule(kRecheckMs);
}

Reading appRead(bool fresh_photo) {
  const uint32_t start = millis();
  Reading r = fresh_photo ? readSamples() : readPhoto(config);
  applyJumpFilter(r);
  storeReading(r);
  if (r.ok && camera_ok) setCameraRestartCount(0);
  if (!r.ok && config.dialReady()) {
    const Photo& p = lastPhoto();
    const bool photo_of_reading = p.size && (!fresh_photo || p.taken_ms >= start);
    keepFailure(r, photo_of_reading ? p.jpeg : nullptr, photo_of_reading ? p.size : 0);
  }
  if (r.ok) {
    logMsg("Reading: %.1f %s", r.level, config.unit.c_str());
  } else {
    logMsg("Reading refused: %s", r.error.c_str());
  }
  mqttPublishReading(lastAttempt(), lastGood(), r.ok);
  if (fresh_photo) mqttPublishPhoto();
  return r;
}

void appConfigChanged(bool wifi_changed) {
  if (camera_ok) cameraApply(config);
  jump_filter.reset();  // the calibration or the offset may have moved the level
  clockBegin(config.ntp_server);
  mqttReconfigure();
  schedule(5000);
  if (wifi_changed) appRestartSoon();
}

void appRestartSoon() {
  // Asked for by the user: the running firmware is fine, keep it.
  otaConfirm("restart requested");
  restart_at_ms = millis() + 1500;
}

uint32_t appNextReadInS() {
  const int32_t left = static_cast<int32_t>(next_read_ms - millis());
  return left > 0 ? left / 1000 : 0;
}

void setup() {
  Serial.begin(115200);
  diagBegin();
  esp_task_wdt_init(kWatchdogS, true);
  esp_task_wdt_add(nullptr);
  const uint64_t mac = ESP.getEfuseMac();
  char id[20];
  snprintf(id, sizeof id, "gauge-cam-%06x", static_cast<unsigned>(mac >> 24) & 0xFFFFFF);
  device_id = id;
  if (!config.load()) logMsg("No saved configuration, using defaults");
  logMsg("gauge-cam %s (%s), started by: %s", GAUGECAM_VERSION, device_id.c_str(), resetReasonId());
  if (previousRestartNote().length()) logMsg("Previous restart: %s", previousRestartNote().c_str());
  otaBegin();

  if (!psramFound()) logMsg("No PSRAM: this board cannot take photos");
  camera_ok = psramFound() && cameraBegin(config);
  if (!camera_ok) logMsg("Camera start failed");

  improvBegin("gauge-cam", GAUGECAM_VERSION, device_id, improvConnect, netUrl);
  netBegin(device_id);
  mqttBegin(device_id);
  webBegin();
  schedule(10000);  // first reading once the network had time to come up
}

void loop() {
  healthLoop();
  improvLoop();
  netLoop();
  webLoop();
  mqttLoop();
  otaLoop();

  if (mqttTakeReadRequest()) next_read_ms = millis();
  const bool due = static_cast<int32_t>(millis() - next_read_ms) >= 0;
  if (due) clock_wait = clock_wait && !epochNow() && config.ntp_server.length() && millis() < kClockWaitMs;
  if (due && !clock_wait) {
    schedule(config.interval_s * 1000UL);
    if (camera_ok && config.dialReady()) {
      appRead(true);
    } else if (camera_ok) {
      String error;
      if (appCapture(config, error)) mqttPublishPhoto();  // helps the first set-up
    }
  }
  if (restart_at_ms && static_cast<int32_t>(millis() - restart_at_ms) >= 0) diagRestart("asked from the interface");
  delay(2);
}
