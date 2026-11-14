#include "mqtt.h"

#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "camera.h"
#include "diag.h"
#include "generated/version.h"
#include "ota.h"

namespace {

WiFiClient net;
PubSubClient client(net);
String id;
String base;  // <prefix>/<id>
bool discovery_due = true;
String announced_prefix;  // discovery prefix the entities were published under
String ha_status_topic;  // <discovery prefix>/status, Home Assistant's birth message
bool read_requested = false;
bool state_pending = false;  // a new successful reading waits for the broker
bool mqtt_logged_down = false;
bool ever_connected = false;
uint32_t last_attempt_ms = 0;

String topic(const char* leaf) { return base + "/" + leaf; }

// Entity names in the configured language.
const char* label(const char* key) {
  struct Entry { const char* key; const char* fr; const char* en; };
  static const Entry kNames[] = {
      {"level", "Niveau", "Level"},
      {"volume", "Volume", "Volume"},
      {"percent", "Remplissage", "Fill level"},
      {"angle", "Angle de l'aiguille", "Needle angle"},
      {"contrast", "Contraste de l'aiguille", "Needle contrast"},
      {"rssi", "Signal Wi-Fi", "Wi-Fi signal"},
      {"status", "État de la lecture", "Reading status"},
      {"photo", "Photo", "Photo"},
      {"read", "Lire maintenant", "Read now"},
      {"last_reading", "Dernière lecture", "Last reading"},
      {"boot", "Dernier démarrage", "Last start"},
      {"reset_reason", "Cause du dernier redémarrage", "Last restart cause"},
      {"heap", "Mémoire libre", "Free memory"},
      {"firmware", "Firmware", "Firmware"},
  };
  for (const Entry& e : kNames) {
    if (strcmp(e.key, key) == 0) return config.lang == "en" ? e.en : e.fr;
  }
  return key;
}

void onMessage(char* t, uint8_t* payload, unsigned int len) {
  if (topic("cmd") == t && len == 4 && memcmp(payload, "read", 4) == 0) {
    read_requested = true;
  }
  if (topic("cmd") == t && len == 7 && memcmp(payload, "install", 7) == 0) {
    logMsg("Update requested over MQTT");
    otaRequestInstall();
  }
  // Home Assistant restarted: announce the entities again.
  if (ha_status_topic == t && len == 6 && memcmp(payload, "online", 6) == 0) {
    discovery_due = true;
  }
}

void addDevice(JsonObject doc) {
  JsonObject dev = doc["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add(id);
  dev["name"] = config.name;
  dev["manufacturer"] = "gauge-cam";
  dev["model"] = "ESP32-CAM";
  dev["sw_version"] = GAUGECAM_VERSION;
  dev["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
  doc["availability_topic"] = topic("availability");
}

String configTopic(const String& prefix, const char* component, const char* object) {
  return prefix + "/" + component + "/" + id + "/" + object + "/config";
}

void publishConfig(const char* component, const char* object, JsonDocument& doc) {
  const String t = configTopic(config.discovery_prefix, component, object);
  if (doc.isNull()) {  // removes the entity
    client.publish(t.c_str(), "", true);
    return;
  }
  doc["unique_id"] = id + "_" + object;
  addDevice(doc.as<JsonObject>());
  String payload;
  serializeJson(doc, payload);
  client.publish(t.c_str(), payload.c_str(), true);
}

struct SensorSpec {
  const char* object;
  const char* leaf;  // state topic leaf
  const char* value_key;  // JSON key, nullptr: the payload itself
  const char* unit;
  const char* device_class;
  bool diagnostic;
  bool measurement;  // state_class measurement
  bool expires;  // unavailable after three missed readings
};

void sensor(const SensorSpec& spec, bool enabled = true, bool enabled_by_default = true) {
  JsonDocument doc;
  if (!enabled) {
    publishConfig("sensor", spec.object, doc);
    return;
  }
  doc["name"] = label(spec.object);
  doc["state_topic"] = topic(spec.leaf);
  if (spec.value_key) doc["value_template"] = String("{{ value_json.") + spec.value_key + " | default(None) }}";
  if (spec.unit) doc["unit_of_measurement"] = spec.unit;
  if (spec.device_class) doc["device_class"] = spec.device_class;
  if (spec.diagnostic) doc["entity_category"] = "diagnostic";
  if (spec.measurement) doc["state_class"] = "measurement";
  if (spec.expires) doc["expire_after"] = config.interval_s * 3 + 120;
  if (!enabled_by_default) doc["enabled_by_default"] = false;
  publishConfig("sensor", spec.object, doc);
}

// Every entity this device may announce.
constexpr struct {
  const char* component;
  const char* object;
} kEntities[] = {
    {"sensor", "level"}, {"sensor", "volume"}, {"sensor", "percent"}, {"sensor", "last_reading"},
    {"sensor", "angle"}, {"sensor", "contrast"}, {"sensor", "rssi"}, {"sensor", "status"},
    {"sensor", "boot"}, {"sensor", "reset_reason"}, {"sensor", "heap"}, {"image", "photo"},
    {"button", "read"}, {"update", "firmware"},
};

// Empty retained configs: Home Assistant deletes the entities.
void removeDiscovery(const String& prefix) {
  for (const auto& e : kEntities) client.publish(configTopic(prefix, e.component, e.object).c_str(), "", true);
}

void publishDiscovery() {
  if (announced_prefix.length() && announced_prefix != config.discovery_prefix) {
    removeDiscovery(announced_prefix);
  }
  if (!config.ha_discovery) {
    // Also clears what a previous boot announced before discovery was turned off.
    removeDiscovery(config.discovery_prefix);
    announced_prefix = "";
    return;
  }
  announced_prefix = config.discovery_prefix;
  const bool cm = config.unit == "cm";
  const gauge::Tank tank = config.tank();
  sensor({"level", "state", "level", config.unit.c_str(), cm ? "distance" : nullptr, false, true, true});
  sensor({"volume", "state", "volume_l", "L", "volume_storage", false, true, true}, tank.ready());
  sensor({"percent", "state", "percent", "%", nullptr, false, true, true},
         tank.capacity() > 0 || tank.fullLevel() > 0);
  sensor({"last_reading", "state", "time", nullptr, "timestamp", false, false, false});
  sensor({"angle", "state", "angle", "°", nullptr, true, false, false});
  sensor({"contrast", "state", "contrast", nullptr, nullptr, true, false, false});
  sensor({"rssi", "diag", "rssi", "dBm", "signal_strength", true, false, false});
  sensor({"status", "status", nullptr, nullptr, nullptr, true, false, false});
  sensor({"boot", "diag", "boot", nullptr, "timestamp", true, false, false});
  sensor({"reset_reason", "diag", "reset_reason", nullptr, nullptr, true, false, false});
  sensor({"heap", "diag", "heap_free", "B", "data_size", true, false, false}, true, false);
  {
    JsonDocument doc;
    doc["name"] = label("photo");
    doc["image_topic"] = topic("image");
    doc["content_type"] = "image/jpeg";
    publishConfig("image", "photo", doc);
  }
  {
    JsonDocument doc;
    doc["name"] = label("read");
    doc["command_topic"] = topic("cmd");
    doc["payload_press"] = "read";
    publishConfig("button", "read", doc);
  }
  {
    JsonDocument doc;
    doc["name"] = label("firmware");
    doc["state_topic"] = topic("update");
    doc["command_topic"] = topic("cmd");
    doc["payload_install"] = "install";
    doc["device_class"] = "firmware";
    doc["entity_category"] = "config";
    publishConfig("update", "firmware", doc);
  }
}

void publishDiag() {
  JsonDocument doc;
  doc["rssi"] = WiFi.RSSI();
  static uint32_t boot_epoch = 0;  // computed once, so that it does not drift by a second
  if (!boot_epoch && epochNow()) boot_epoch = epochNow() - millis() / 1000;
  if (boot_epoch) doc["boot"] = isoTime(boot_epoch);
  doc["reset_reason"] = resetReasonLabel();
  doc["heap_free"] = ESP.getFreeHeap();
  doc["version"] = GAUGECAM_VERSION;
  String payload;
  serializeJson(doc, payload);
  client.publish(topic("diag").c_str(), payload.c_str(), true);
}

void publishState(const Reading& good) {
  JsonDocument doc;
  auto put = [&doc](const char* key, float v, int decimals) {
    if (isnan(v)) return;
    const float scale = powf(10, decimals);
    doc[key] = roundf(v * scale) / scale;
  };
  put("level", good.level, 1);
  put("volume_l", good.volume_l, 0);
  put("percent", good.percent, 1);
  put("angle", good.gauge.needle_angle, 1);
  put("contrast", good.gauge.contrast, 0);
  if (good.at_epoch) doc["time"] = isoTime(good.at_epoch);
  String payload;
  serializeJson(doc, payload);
  if (client.publish(topic("state").c_str(), payload.c_str(), true)) state_pending = false;
}

bool connect() {
  if (config.mqtt_host.isEmpty() || WiFi.status() != WL_CONNECTED) return false;
  client.setServer(config.mqtt_host.c_str(), config.mqtt_port);
  const String will = topic("availability");
  const char* user = config.mqtt_user.isEmpty() ? nullptr : config.mqtt_user.c_str();
  const char* pass = config.mqtt_pass.isEmpty() ? nullptr : config.mqtt_pass.c_str();
  if (!client.connect(id.c_str(), user, pass, will.c_str(), 0, true, "offline")) {
    if (!mqtt_logged_down) logMsg("MQTT: cannot connect to %s:%u (state %d)", config.mqtt_host.c_str(),
                                  config.mqtt_port, client.state());
    mqtt_logged_down = true;
    return false;
  }
  logMsg("MQTT connected to %s:%u", config.mqtt_host.c_str(), config.mqtt_port);
  ever_connected = true;
  mqtt_logged_down = false;
  client.publish(will.c_str(), "online", true);
  client.subscribe(topic("cmd").c_str());
  ha_status_topic = config.discovery_prefix + "/status";
  if (config.ha_discovery) client.subscribe(ha_status_topic.c_str());
  discovery_due = true;
  return true;
}

}  // namespace

void mqttBegin(const String& device_id) {
  id = device_id;
  base = config.mqtt_prefix + "/" + id;
  client.setBufferSize(1024);
  client.setCallback(onMessage);
}

void mqttReconfigure() {
  if (client.connected()) {
    client.publish(topic("availability").c_str(), "offline", true);
    client.disconnect();
  }
  base = config.mqtt_prefix + "/" + id;
  last_attempt_ms = 0;
  ever_connected = false;  // a new server proves nothing yet
  mqtt_logged_down = false;
}

void mqttLoop() {
  if (!client.connected()) {
    if (last_attempt_ms && millis() - last_attempt_ms < 15000) return;
    if (last_attempt_ms && !mqtt_logged_down && config.mqtt_host.length()) {
      logMsg("MQTT connection lost");
      mqtt_logged_down = true;
    }
    last_attempt_ms = millis();
    if (!connect()) return;
  }
  client.loop();
  if (discovery_due) {
    publishDiscovery();
    discovery_due = false;
    mqttPublishReading(lastAttempt(), lastGood(), false);
    mqttPublishPhoto();
    mqttPublishUpdate();
  }
}

bool mqttConnected() { return client.connected(); }
bool mqttEverConnected() { return ever_connected; }

void mqttPublishReading(const Reading& attempt, const Reading& good, bool new_good) {
  if (new_good) state_pending = true;
  if (!client.connected()) return;
  if (attempt.at_ms) {
    const String status = attempt.ok ? String("ok") : attempt.error.substring(0, 200);
    client.publish(topic("status").c_str(), status.c_str(), true);
  }
  // Values are only sent after a new successful reading: Home Assistant shows
  // them as unavailable once three readings in a row are missing.
  if (state_pending && good.at_ms) publishState(good);
  publishDiag();
}

void mqttPublishUpdate() {
  if (!client.connected()) return;
  JsonDocument doc;
  otaToHomeAssistant(doc.to<JsonObject>());
  String payload;
  serializeJson(doc, payload);
  client.publish(topic("update").c_str(), payload.c_str(), true);
}

void mqttPublishPhoto() {
  const Photo& p = lastPhoto();
  if (!client.connected() || !p.size) return;
  const String t = topic("image");
  // Streamed: the photo is larger than the client buffer.
  if (!client.beginPublish(t.c_str(), p.size, true)) return;
  client.write(p.jpeg, p.size);
  client.endPublish();
}

bool mqttTest(const Config& c, String& error) {
  if (c.mqtt_host.isEmpty()) {
    error = "no MQTT server given";
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    error = "no Wi-Fi connection yet";
    return false;
  }
  WiFiClient test_net;
  PubSubClient test(test_net);
  test.setServer(c.mqtt_host.c_str(), c.mqtt_port);
  test.setSocketTimeout(5);
  const String test_id = id + "-test";  // the running connection keeps its own id
  const char* user = c.mqtt_user.isEmpty() ? nullptr : c.mqtt_user.c_str();
  const char* pass = c.mqtt_pass.isEmpty() ? nullptr : c.mqtt_pass.c_str();
  esp_task_wdt_reset();
  const bool ok = test.connect(test_id.c_str(), user, pass);
  esp_task_wdt_reset();
  if (ok) {
    test.disconnect();
    logMsg("MQTT test: connected to %s:%u", c.mqtt_host.c_str(), c.mqtt_port);
    return true;
  }
  switch (test.state()) {
    case MQTT_CONNECT_FAILED: error = "MQTT server unreachable"; break;
    case MQTT_CONNECTION_TIMEOUT: error = "MQTT server did not answer"; break;
    case MQTT_CONNECT_BAD_PROTOCOL: error = "MQTT server refused the protocol version"; break;
    case MQTT_CONNECT_BAD_CLIENT_ID: error = "MQTT server refused the client id"; break;
    case MQTT_CONNECT_UNAVAILABLE: error = "MQTT server unavailable"; break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
    case MQTT_CONNECT_UNAUTHORIZED: error = "MQTT server refused the user or password"; break;
    default: error = "MQTT connection closed"; break;
  }
  logMsg("MQTT test: %s:%u, %s", c.mqtt_host.c_str(), c.mqtt_port, error.c_str());
  return false;
}

bool mqttTakeReadRequest() {
  const bool requested = read_requested;
  read_requested = false;
  return requested;
}
