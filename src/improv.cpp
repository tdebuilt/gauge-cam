#include "improv.h"

#include <WiFi.h>

#include <vector>

namespace {

enum : uint8_t {
  kTypeState = 0x01,
  kTypeError = 0x02,
  kTypeRpc = 0x03,
  kTypeRpcResult = 0x04,
};
enum : uint8_t { kStateAuthorized = 0x02, kStateProvisioning = 0x03, kStateProvisioned = 0x04 };
enum : uint8_t { kErrorNone = 0x00, kErrorInvalidRpc = 0x01, kErrorUnknownRpc = 0x02,
                 kErrorUnableToConnect = 0x03 };
enum : uint8_t { kCmdWifi = 0x01, kCmdState = 0x02, kCmdInfo = 0x03, kCmdScan = 0x04 };

String fw_name, fw_version, dev_name;
bool (*connect_fn)(const String&, const String&) = nullptr;
String (*url_fn)() = nullptr;
std::vector<uint8_t> rx;

void send(uint8_t type, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> pkt = {'I', 'M', 'P', 'R', 'O', 'V', 1, type,
                              static_cast<uint8_t>(data.size())};
  pkt.insert(pkt.end(), data.begin(), data.end());
  uint8_t sum = 0;
  for (uint8_t b : pkt) sum += b;
  pkt.push_back(sum);
  pkt.push_back('\n');
  Serial.write(pkt.data(), pkt.size());
  Serial.flush();
}

void sendState(uint8_t state) { send(kTypeState, {state}); }
void sendError(uint8_t error) { send(kTypeError, {error}); }

void sendResult(uint8_t cmd, const std::vector<String>& strings) {
  std::vector<uint8_t> body;
  for (const String& s : strings) {
    body.push_back(static_cast<uint8_t>(s.length()));
    body.insert(body.end(), s.begin(), s.end());
  }
  std::vector<uint8_t> data = {cmd, static_cast<uint8_t>(body.size())};
  data.insert(data.end(), body.begin(), body.end());
  send(kTypeRpcResult, data);
}

uint8_t currentState() {
  return WiFi.status() == WL_CONNECTED ? kStateProvisioned : kStateAuthorized;
}

void handleRpc(const uint8_t* data, size_t len) {
  if (len < 2 || data[1] + 2u != len) {
    sendError(kErrorInvalidRpc);
    return;
  }
  const uint8_t cmd = data[0];
  const uint8_t* p = data + 2;
  const uint8_t* end = data + len;
  switch (cmd) {
    case kCmdWifi: {
      if (p >= end || p + 1 + p[0] > end) return sendError(kErrorInvalidRpc);
      const String ssid(reinterpret_cast<const char*>(p + 1), p[0]);
      p += 1 + p[0];
      if (p >= end || p + 1 + p[0] > end) return sendError(kErrorInvalidRpc);
      const String pass(reinterpret_cast<const char*>(p + 1), p[0]);
      sendState(kStateProvisioning);
      if (connect_fn && connect_fn(ssid, pass)) {
        sendError(kErrorNone);
        sendState(kStateProvisioned);
        sendResult(kCmdWifi, {url_fn ? url_fn() : String()});
      } else {
        sendError(kErrorUnableToConnect);
        sendState(kStateAuthorized);
      }
      return;
    }
    case kCmdState:
      sendState(currentState());
      if (currentState() == kStateProvisioned) sendResult(kCmdState, {url_fn ? url_fn() : String()});
      return;
    case kCmdInfo:
      sendResult(kCmdInfo, {fw_name, fw_version, "ESP32", dev_name});
      return;
    case kCmdScan: {
      const int n = WiFi.scanNetworks();
      for (int i = 0; i < n; ++i) {
        sendResult(kCmdScan, {WiFi.SSID(i), String(WiFi.RSSI(i)),
                              WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"});
      }
      WiFi.scanDelete();
      sendResult(kCmdScan, {});
      return;
    }
    default:
      sendError(kErrorUnknownRpc);
  }
}

// Consumes one complete packet from rx, or one stray byte; waits otherwise.
void parse() {
  static const uint8_t kHeader[] = {'I', 'M', 'P', 'R', 'O', 'V'};
  for (size_t i = 0; i < rx.size() && i < sizeof kHeader; ++i) {
    if (rx[i] != kHeader[i]) {
      rx.erase(rx.begin());  // resynchronise on the next byte
      return;
    }
  }
  if (rx.size() < 9) return;
  const size_t total = 9 + rx[8] + 1;
  if (rx.size() < total) return;
  uint8_t sum = 0;
  for (size_t i = 0; i < total - 1; ++i) sum += rx[i];
  const bool valid = rx[6] == 1 && sum == rx[total - 1];
  if (valid && rx[7] == kTypeRpc) handleRpc(rx.data() + 9, rx[8]);
  rx.erase(rx.begin(), rx.begin() + total);
}

}  // namespace

void improvBegin(const String& firmware, const String& version, const String& device_name,
                 bool (*connect)(const String&, const String&), String (*url)()) {
  fw_name = firmware;
  fw_version = version;
  dev_name = device_name;
  connect_fn = connect;
  url_fn = url;
}

void improvLoop() {
  while (Serial.available()) {
    rx.push_back(static_cast<uint8_t>(Serial.read()));
    if (rx.size() > 300) rx.clear();
    size_t before;
    do {
      before = rx.size();
      parse();
    } while (!rx.empty() && rx.size() != before);
  }
}
