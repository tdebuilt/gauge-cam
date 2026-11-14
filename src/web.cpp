#include "web.h"

#include <ArduinoJson.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "app.h"
#include "camera.h"
#include "config.h"
#include "diag.h"
#include "generated/version.h"
#include "generated/web_assets.h"
#include "mqtt.h"
#include "net.h"
#include "ota.h"
#include "reader.h"

namespace {

WebServer server(80);
String upload_error;

bool authorized() {
  if (config.ui_password.isEmpty()) return true;
  if (server.authenticate("admin", config.ui_password.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "gauge-cam");
  return false;
}

void sendJson(int code, JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void sendError(int code, const String& message) {
  JsonDocument doc;
  doc["error"] = message;
  sendJson(code, doc);
}

bool parseBody(JsonDocument& doc) {
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) {
    sendError(400, "invalid JSON body");
    return false;
  }
  return true;
}

// Photos and readings wait while a firmware update downloads.
bool refuseWhileUpdating() {
  if (!otaBusy()) return false;
  sendError(503, "firmware update in progress");
  return true;
}

void serveAsset(const WebAsset& a) {
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, a.type, reinterpret_cast<const char*>(a.data), a.size);
}

void handleStatus() {
  if (!authorized()) return;
  JsonDocument doc;
  doc["name"] = config.name;
  doc["id"] = deviceId();
  doc["version"] = GAUGECAM_VERSION;
  doc["uptime_s"] = millis() / 1000;
  doc["heap_free"] = ESP.getFreeHeap();
  doc["heap_min"] = ESP.getMinFreeHeap();
  doc["psram_free"] = ESP.getFreePsram();
  doc["time"] = epochNow();
  doc["reset_reason"] = resetReasonId();
  if (previousRestartNote().length()) doc["restart_note"] = previousRestartNote();
  otaToJson(doc["update"].to<JsonObject>());
  netToJson(doc["wifi"].to<JsonObject>());
  doc["mqtt"]["configured"] = config.mqtt_host.length() > 0;
  doc["mqtt"]["connected"] = mqttConnected();
  const Photo& p = lastPhoto();
  if (p.size) {
    doc["photo"]["width"] = p.width;
    doc["photo"]["height"] = p.height;
    doc["photo"]["bytes"] = p.size;
    doc["photo"]["age_s"] = (millis() - p.taken_ms) / 1000;
    doc["photo"]["taken_ms"] = p.taken_ms;  // changes with every photo
  }
  doc["dial_ready"] = config.dialReady();
  doc["last_failure"] = lastFailureId();
  doc["next_read_s"] = appNextReadInS();
  readingToJson(lastAttempt(), config, doc["attempt"].to<JsonObject>());
  readingToJson(lastGood(), config, doc["good"].to<JsonObject>());
  sendJson(200, doc);
}

void handleGetConfig() {
  if (!authorized()) return;
  JsonDocument doc;
  config.toJson(doc.to<JsonObject>(), false);
  sendJson(200, doc);
}

void handlePostConfig() {
  if (!authorized()) return;
  JsonDocument body;
  if (!parseBody(body)) return;
  const String ssid = config.wifi_ssid, pass = config.wifi_pass;
  String error;
  if (!config.fromJson(body.as<JsonObjectConst>(), error)) return sendError(400, error);
  if (!config.save()) return sendError(500, "cannot save the configuration");
  const bool wifi_changed = ssid != config.wifi_ssid || pass != config.wifi_pass;
  JsonDocument doc;
  doc["saved"] = true;
  doc["restart"] = wifi_changed;
  sendJson(200, doc);
  appConfigChanged(wifi_changed);
}

// Takes a photo. A JSON body holds unsaved capture settings (LED, rotation,
// flips, resolution...) to try them before saving.
void handleCapture() {
  if (!authorized() || refuseWhileUpdating()) return;
  Config draft = config;
  String error;
  const bool has_draft = server.arg("plain").length() > 0;
  if (has_draft) {
    JsonDocument body;
    if (!parseBody(body)) return;
    if (!draft.fromJson(body.as<JsonObjectConst>(), error)) return sendError(400, error);
  }
  if (!appCapture(has_draft ? draft : config, error)) return sendError(500, error);
  JsonDocument doc;
  doc["width"] = lastPhoto().width;
  doc["height"] = lastPhoto().height;
  sendJson(200, doc);
}

void handleRead() {
  if (!authorized() || refuseWhileUpdating()) return;
  const bool fresh = server.arg("photo") != "0";
  JsonDocument doc;
  readingToJson(appRead(fresh), config, doc.to<JsonObject>());
  sendJson(200, doc);
}

// Reads the last photo with a draft dial configuration, without saving it.
void handleTest() {
  if (!authorized()) return;
  JsonDocument body;
  if (!parseBody(body)) return;
  Config draft = config;
  String error;
  if (!draft.fromJson(body.as<JsonObjectConst>(), error)) return sendError(400, error);
  JsonDocument doc;
  readingToJson(readPhoto(draft), draft, doc.to<JsonObject>());
  sendJson(200, doc);
}

void handleImage() {
  if (!authorized()) return;
  const Photo& p = lastPhoto();
  if (!p.size) return sendError(404, "no photo yet");
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", reinterpret_cast<const char*>(p.jpeg), p.size);
}

void handleFailures() {
  if (!authorized()) return;
  JsonDocument doc;
  failuresToJson(config, doc.to<JsonArray>());
  sendJson(200, doc);
}

void handleFailureImage() {
  if (!authorized()) return;
  const Failure* f = findFailure(server.arg("id").toInt());
  if (!f || !f->size) return sendError(404, "no such photo");
  server.sendHeader("Cache-Control", "no-cache");  // ids start again at 1 after a restart
  server.send_P(200, "image/jpeg", reinterpret_cast<const char*>(f->jpeg), f->size);
}

void handleScan() {
  if (!authorized()) return;
  const int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray list = doc.to<JsonArray>();
  for (int i = 0; i < n; ++i) {
    JsonObject o = list.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendJson(200, doc);
}

void handleLog() {
  if (!authorized()) return;
  JsonDocument doc;
  logToJson(doc.to<JsonArray>());
  sendJson(200, doc);
}

void handleUpdateCheck() {
  if (!authorized()) return;
  otaRequestCheck();
  JsonDocument doc;
  doc["checking"] = true;
  sendJson(200, doc);
}

// Installs the latest GitHub release; runs from the main loop.
void handleUpdateInstall() {
  if (!authorized() || refuseWhileUpdating()) return;
  logMsg("Update requested from the interface");
  otaRequestInstall();
  JsonDocument doc;
  doc["installing"] = true;
  sendJson(200, doc);
}

void handleRestart() {
  if (!authorized()) return;
  JsonDocument doc;
  doc["restart"] = true;
  sendJson(200, doc);
  appRestartSoon();
}

void handleUploadDone() {
  if (!authorized()) return;
  if (upload_error.length() || Update.hasError()) {
    return sendError(500, upload_error.length() ? upload_error : String(Update.errorString()));
  }
  JsonDocument doc;
  doc["updated"] = true;
  sendJson(200, doc);
  appRestartSoon();
}

void handleUpload() {
  if (!config.ui_password.isEmpty() &&
      !server.authenticate("admin", config.ui_password.c_str())) {
    upload_error = "unauthorized";
    return;
  }
  HTTPUpload& up = server.upload();
  esp_task_wdt_reset();  // the whole upload runs inside one call of the loop
  if (up.status == UPLOAD_FILE_START) {
    upload_error = "";
    if (otaBusy()) {
      upload_error = "firmware update in progress";
    } else if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      upload_error = Update.errorString();
    } else {
      logMsg("Firmware upload started");
    }
  } else if (up.status == UPLOAD_FILE_WRITE && upload_error.isEmpty()) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) upload_error = Update.errorString();
  } else if (up.status == UPLOAD_FILE_END && upload_error.isEmpty()) {
    if (!Update.end(true)) upload_error = Update.errorString();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    upload_error = "upload aborted";
  }
}

void handleNotFound() {
  // Captive portal: send any foreign host to the setup page.
  if (netApActive() && server.hostHeader() != WiFi.softAPIP().toString()) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "not found");
}

}  // namespace

void webBegin() {
  for (const WebAsset& a : kWebAssets) {
    server.on(a.path, HTTP_GET, [&a]() {
      if (authorized()) serveAsset(a);
    });
  }
  server.on("/", HTTP_GET, []() {
    if (!authorized()) return;
    for (const WebAsset& a : kWebAssets) {
      if (strcmp(a.path, "/index.html") == 0) return serveAsset(a);
    }
    server.send(404, "text/plain", "no web interface");
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/capture", HTTP_POST, handleCapture);
  server.on("/api/read", HTTP_POST, handleRead);
  server.on("/api/test", HTTP_POST, handleTest);
  server.on("/api/photo.jpg", HTTP_GET, handleImage);
  server.on("/api/failures", HTTP_GET, handleFailures);
  server.on("/api/failure.jpg", HTTP_GET, handleFailureImage);
  server.on("/api/wifi/scan", HTTP_GET, handleScan);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/update/check", HTTP_POST, handleUpdateCheck);
  server.on("/api/update/install", HTTP_POST, handleUpdateInstall);
  server.on("/api/update", HTTP_POST, handleUploadDone, handleUpload);
  server.onNotFound(handleNotFound);
  server.begin();
}

void webLoop() { server.handleClient(); }
