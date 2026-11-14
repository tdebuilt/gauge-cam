#include "mqtt.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "camera.h"
#include "generated/version.h"

namespace {

WiFiClient net;
PubSubClient client(net);
String id;
String base;  // <prefix>/<id>
bool discovery_due = true;
String announced_prefix;  // discovery prefix the entities were published under
String ha_status_topic;  // <discovery prefix>/status, Home Assistant's birth message
bool read_requested = false;
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

void sensor(const char* object, const char* name, const char* value_key,
            const char* unit, const char* device_class, bool diagnostic,
            bool enabled = true) {
  JsonDocument doc;
  if (!enabled) {
    publishConfig("sensor", object, doc);
    return;
  }
  doc["name"] = name;
  doc["state_topic"] = topic("state");
  doc["value_template"] = String("{{ value_json.") + value_key + " }}";
  if (unit) doc["unit_of_measurement"] = unit;
  if (device_class) doc["device_class"] = device_class;
  if (diagnostic) {
    doc["entity_category"] = "diagnostic";
  } else {
    doc["state_class"] = "measurement";
  }
  publishConfig("sensor", object, doc);
}

// Every entity this device may announce.
constexpr struct {
  const char* component;
  const char* object;
} kEntities[] = {
    {"sensor", "level"}, {"sensor", "volume"}, {"sensor", "percent"}, {"sensor", "angle"},
    {"sensor", "contrast"}, {"sensor", "rssi"}, {"sensor", "status"}, {"image", "photo"},
    {"button", "read"},
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
  sensor("level", label("level"), "level", config.unit.c_str(), cm ? "distance" : nullptr, false);
  sensor("volume", label("volume"), "volume_l", "L", "volume_storage", false, config.tankReady());
  sensor("percent", label("percent"), "percent", "%", nullptr, false, config.tank_height_cm > 0);
  sensor("angle", label("angle"), "angle", "°", nullptr, true);
  sensor("contrast", label("contrast"), "contrast", nullptr, nullptr, true);
  sensor("rssi", label("rssi"), "rssi", "dBm", "signal_strength", true);
  {
    JsonDocument doc;
    doc["name"] = label("status");
    doc["state_topic"] = topic("status");
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:gauge";
    publishConfig("sensor", "status", doc);
  }
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
}

bool connect() {
  if (config.mqtt_host.isEmpty() || WiFi.status() != WL_CONNECTED) return false;
  client.setServer(config.mqtt_host.c_str(), config.mqtt_port);
  const String will = topic("availability");
  const char* user = config.mqtt_user.isEmpty() ? nullptr : config.mqtt_user.c_str();
  const char* pass = config.mqtt_pass.isEmpty() ? nullptr : config.mqtt_pass.c_str();
  if (!client.connect(id.c_str(), user, pass, will.c_str(), 0, true, "offline")) {
    return false;
  }
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
}

void mqttLoop() {
  if (!client.connected()) {
    if (last_attempt_ms && millis() - last_attempt_ms < 15000) return;
    last_attempt_ms = millis();
    if (!connect()) return;
  }
  client.loop();
  if (discovery_due) {
    publishDiscovery();
    discovery_due = false;
    mqttPublishReading(lastAttempt(), lastGood());
    mqttPublishPhoto();
  }
}

bool mqttConnected() { return client.connected(); }

void mqttPublishReading(const Reading& attempt, const Reading& good) {
  if (!client.connected()) return;
  if (attempt.at_ms) {
    const String status = attempt.ok ? String("ok") : attempt.error.substring(0, 200);
    client.publish(topic("status").c_str(), status.c_str(), true);
  }
  if (!good.at_ms) return;  // values stay unknown until a first success
  JsonDocument doc;
  auto put = [&doc](const char* key, float v, int decimals) {
    if (isnan(v)) return;
    const float scale = powf(10, decimals);
    doc[key] = roundf(v * scale) / scale;
  };
  put("level", good.level, 1);
  put("volume_l", good.volume_l, 0);
  put("percent", good.percent, 1);
  if (good.ok) {
    put("angle", good.gauge.needle_angle, 1);
    put("contrast", good.gauge.contrast, 0);
  }
  doc["rssi"] = WiFi.RSSI();
  String payload;
  serializeJson(doc, payload);
  client.publish(topic("state").c_str(), payload.c_str(), true);
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

bool mqttTakeReadRequest() {
  const bool requested = read_requested;
  read_requested = false;
  return requested;
}
