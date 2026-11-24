#include "ota.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <mbedtls/sha256.h>

#include "config.h"
#include "diag.h"
#include "generated/version.h"
#include "mqtt.h"
#include "web.h"

// Mozilla root certificates, built into the ESP-IDF mbedTLS library.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");

namespace {

constexpr const char* kAsset = "gauge-cam.bin";
constexpr uint32_t kFirstCheckMs = 120000;  // after Wi-Fi is up
constexpr uint32_t kCheckEveryMs = 24UL * 3600 * 1000;
constexpr uint32_t kRetryMs = 3600UL * 1000;  // after a failed check

String latest;  // tag of the latest release, "" when none or unknown
String release_url;
String summary;
String asset_url;
int asset_size = 0;
String asset_sha256;  // 64 lowercase hex digits, from the digest GitHub publishes
String check_error;
String install_error;
uint32_t checked_ms = 0;  // 0: never
uint32_t next_check_ms = 0;
uint32_t wifi_up_ms = 0;
bool check_requested = false;
bool install_requested = false;
bool busy = false;
int progress = -1;  // percent while installing
bool pending_verify = false;
bool confirmed = false;

// A build made after the tag ("v1.2.0-3-gabc") is newer than that release.
bool isAvailable() {
  const String installed = GAUGECAM_VERSION;
  return latest.length() && asset_url.length() && latest != installed &&
         !installed.startsWith(latest + "-");
}

void secure(WiFiClientSecure& tls) {
  tls.setCACertBundle(x509_crt_bundle_start);
}

// mbedTLS takes its buffers (about 40 KB, one block of 17 KB) from the
// internal RAM, not from the PSRAM.
bool enoughMemory(String& error) {
  const size_t block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (ESP.getFreeHeap() >= 45000 && block >= 20000) return true;
  error = "not enough memory for HTTPS (" + String(ESP.getFreeHeap() / 1024) + " KB free)";
  return false;
}

void check() {
  checked_ms = millis();
  if (!enoughMemory(check_error)) return;
  WiFiClientSecure tls;
  secure(tls);
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding: the body streams straight into the parser
  http.setUserAgent("gauge-cam/" GAUGECAM_VERSION);
  http.setTimeout(20000);
  if (!http.begin(tls, "https://api.github.com/repos/" GAUGECAM_REPO "/releases/latest")) {
    check_error = "cannot reach GitHub";
    return;
  }
  http.addHeader("Accept", "application/vnd.github+json");
  const int code = http.GET();
  if (code == 404) {  // no release published yet
    latest = asset_url = asset_sha256 = release_url = summary = "";
    check_error = "";
    http.end();
    return;
  }
  if (code != 200) {
    check_error = code < 0 ? "GitHub unreachable: " + HTTPClient::errorToString(code) : "GitHub answered " + String(code);
    http.end();
    return;
  }
  JsonDocument filter;
  filter["tag_name"] = true;
  filter["html_url"] = true;
  filter["body"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  filter["assets"][0]["size"] = true;
  filter["assets"][0]["digest"] = true;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    check_error = String("unreadable answer from GitHub: ") + err.c_str();
    return;
  }
  latest = doc["tag_name"] | "";
  release_url = doc["html_url"] | "";
  summary = String(doc["body"] | "").substring(0, 250);
  asset_url = "";
  asset_size = 0;
  asset_sha256 = "";
  for (JsonObjectConst a : doc["assets"].as<JsonArrayConst>()) {
    if (strcmp(a["name"] | "", kAsset) == 0) {
      asset_url = a["browser_download_url"] | "";
      asset_size = a["size"] | 0;
      const String digest = a["digest"] | "";
      if (digest.startsWith("sha256:") && digest.length() == 7 + 64) {
        asset_sha256 = digest.substring(7);
        asset_sha256.toLowerCase();
      }
    }
  }
  check_error = "";
  logMsg("Latest release: %s%s", latest.length() ? latest.c_str() : "none",
         isAvailable() ? " (update available)" : "");
}

// Keeps the network services alive while the download blocks the loop.
void serviceNetwork() {
  esp_task_wdt_reset();
  mqttLoop();
  webLoop();
}

void setProgress(int percent) {
  if (percent == progress) return;
  progress = percent;
  mqttPublishUpdate();
}

bool install() {
  if (!isAvailable()) {
    install_error = "no newer release to install";
    return false;
  }
  // Without the digest GitHub publishes, the file could not be checked.
  if (asset_sha256.length() != 64) {
    install_error = "the release gives no SHA-256 digest for " + String(kAsset);
    return false;
  }
  if (!enoughMemory(install_error)) return false;
  logMsg("Installing %s", latest.c_str());
  WiFiClientSecure tls;
  secure(tls);
  HTTPClient http;
  http.useHTTP10(true);
  http.setUserAgent("gauge-cam/" GAUGECAM_VERSION);
  http.setTimeout(20000);
  // The asset URL redirects to GitHub's download host, checked the same way.
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(tls, asset_url)) {
    install_error = "cannot reach GitHub";
    return false;
  }
  const int code = http.GET();
  if (code != 200) {
    install_error = "download failed: " + (code < 0 ? HTTPClient::errorToString(code) : String(code));
    http.end();
    return false;
  }
  const int size = http.getSize();
  if (size <= 0 || (asset_size && size != asset_size)) {
    install_error = "unexpected file size";
    http.end();
    return false;
  }
  if (!Update.begin(size, U_FLASH)) {
    install_error = Update.errorString();
    http.end();
    return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts_ret(&sha, 0);
  constexpr size_t kChunk = 4096;
  uint8_t* buf = static_cast<uint8_t*>(malloc(kChunk));
  int done = 0;
  uint32_t last_data = millis(), last_service = 0;
  while (buf && done < size && millis() - last_data < 20000) {
    const size_t avail = stream->available();
    if (avail) {
      const int n = stream->readBytes(buf, min(avail, min(kChunk, static_cast<size_t>(size - done))));
      if (n > 0) {
        if (Update.write(buf, n) != static_cast<size_t>(n)) break;
        mbedtls_sha256_update_ret(&sha, buf, n);
        done += n;
        last_data = millis();
      }
    } else if (!stream->connected()) {
      break;
    } else {
      delay(5);
    }
    if (millis() - last_service > 500) {
      last_service = millis();
      setProgress(static_cast<int>(100LL * done / size));
      serviceNetwork();
    }
  }
  free(buf);
  http.end();
  uint8_t hash[32];
  mbedtls_sha256_finish_ret(&sha, hash);
  mbedtls_sha256_free(&sha);
  if (done != size) {
    Update.abort();
    install_error = Update.hasError() ? String(Update.errorString()) : "download interrupted";
    return false;
  }
  // Checked before Update.end(): a mismatching image is never marked bootable.
  char hex[65];
  for (int i = 0; i < 32; ++i) snprintf(hex + 2 * i, 3, "%02x", hash[i]);
  if (asset_sha256 != hex) {
    Update.abort();
    install_error = "SHA-256 mismatch: the downloaded file is not the published one";
    logMsg("Update refused: SHA-256 %s, expected %s", hex, asset_sha256.c_str());
    return false;
  }
  if (!Update.end(true)) {
    install_error = Update.errorString();
    return false;
  }
  return true;
}

}  // namespace

extern "C" bool verifyRollbackLater() { return true; }  // see otaConfirm

void otaBegin() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  pending_verify = esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
  if (pending_verify) logMsg("New firmware %s, confirmed once Wi-Fi works", GAUGECAM_VERSION);
  if (esp_ota_get_last_invalid_partition()) {
    logMsg("An earlier update failed to start: the previous firmware was kept");
  }
}

void otaConfirm(const char* why) {
  if (confirmed) return;
  confirmed = true;
  if (!pending_verify) return;
  esp_ota_mark_app_valid_cancel_rollback();
  pending_verify = false;
  logMsg("Firmware %s confirmed (%s)", GAUGECAM_VERSION, why);
}

bool otaPendingVerify() { return pending_verify; }

void otaRequestCheck() { check_requested = true; }

void otaRequestInstall() {
  if (!busy) install_requested = true;
}

bool otaBusy() { return busy || install_requested; }

void otaLoop() {
  if (WiFi.status() != WL_CONNECTED) {
    wifi_up_ms = 0;
    return;
  }
  const uint32_t now = millis();
  if (!wifi_up_ms) {
    wifi_up_ms = now;
    if (!checked_ms) next_check_ms = now + kFirstCheckMs;
  }
  const bool due = config.update_check && static_cast<int32_t>(now - next_check_ms) >= 0;
  if (check_requested || due || (install_requested && !checked_ms)) {
    check_requested = false;
    busy = true;
    check();
    busy = false;
    next_check_ms = millis() + (check_error.length() ? kRetryMs : kCheckEveryMs);
    mqttPublishUpdate();
  }
  if (install_requested) {
    busy = true;
    install_error = "";
    setProgress(0);
    const bool ok = install();
    install_requested = false;
    if (ok) {
      setProgress(100);
      diagRestart("update to " + latest + " installed");
    }
    logMsg("Update failed: %s", install_error.c_str());
    busy = false;
    progress = -1;
    mqttPublishUpdate();
  }
}

void otaToJson(JsonObject out) {
  out["installed"] = GAUGECAM_VERSION;
  out["latest"] = latest;
  out["available"] = isAvailable();
  out["checked_s"] = checked_ms ? static_cast<int32_t>((millis() - checked_ms) / 1000) : -1;
  if (check_error.length()) out["error"] = check_error;
  if (install_error.length()) out["install_error"] = install_error;
  out["busy"] = otaBusy();
  if (progress >= 0) out["progress"] = progress;
  out["release_url"] = release_url;
  out["pending_verify"] = pending_verify;
}

void otaToHomeAssistant(JsonObject out) {
  out["installed_version"] = GAUGECAM_VERSION;
  // Same version when nothing newer: Home Assistant then shows "up to date".
  out["latest_version"] = isAvailable() ? latest : String(GAUGECAM_VERSION);
  out["title"] = "Gauge Cam";
  if (release_url.length()) out["release_url"] = release_url;
  if (summary.length()) out["release_summary"] = summary;
  out["in_progress"] = otaBusy();
  if (progress >= 0) {
    out["update_percentage"] = progress;
  } else {
    out["update_percentage"] = nullptr;
  }
}
