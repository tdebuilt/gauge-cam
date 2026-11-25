#include "net.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "config.h"
#include "diag.h"

namespace {

constexpr uint32_t kApAfterMs = 30000;  // without station link
constexpr uint32_t kApKeepMs = 120000;  // after the station link is back
constexpr uint32_t kRetryMs = 30000;

String host;
DNSServer dns;
bool ap_active = false;
bool mdns_started = false;
uint32_t down_since = 0;
uint32_t up_since = 0;
uint32_t last_retry = 0;
uint32_t last_ap_try = 0;

// Open while Wi-Fi has never worked (first set-up, mistyped password);
// afterwards protected by ap_password, or not started at all without one.
void startAp() {
  const bool open = !config.wifi_verified;
  if (!open && config.ap_password.isEmpty()) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(host.c_str(), open ? nullptr : config.ap_password.c_str());
  dns.start(53, "*", WiFi.softAPIP());
  ap_active = true;
  logMsg("Access point %s (%s) on http://%s/", host.c_str(), open ? "open" : "WPA2",
         WiFi.softAPIP().toString().c_str());
}

void stopAp() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  ap_active = false;
  logMsg("Access point closed");
}

}  // namespace

void netBegin(const String& hostname) {
  host = hostname;
  WiFi.persistent(false);
  WiFi.setHostname(host.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  // Scan every channel and join the strongest access point of the SSID: the default fast scan
  // joins the first one found, so a far access point on channel 1 wins over a near one on 6.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  down_since = millis();
  if (config.wifi_ssid.length()) {
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_pass.c_str());
  } else {
    startAp();
  }
}

void netLoop() {
  const uint32_t now = millis();
  if (ap_active) dns.processNextRequest();
  if (WiFi.status() == WL_CONNECTED) {
    if (!up_since) {
      up_since = now;
      if (!config.wifi_verified) {
        config.wifi_verified = true;
        config.save();
      }
      logMsg("Wi-Fi connected to %s (%d dBm): http://%s/", config.wifi_ssid.c_str(), WiFi.RSSI(),
             WiFi.localIP().toString().c_str());
      clockBegin(config.ntp_server);
      if (!mdns_started && MDNS.begin(host.c_str())) {
        MDNS.addService("http", "tcp", 80);
        mdns_started = true;
      }
    }
    down_since = 0;
    if (ap_active && now - up_since > kApKeepMs) stopAp();
    return;
  }
  if (up_since) logMsg("Wi-Fi connection lost");
  up_since = 0;
  if (!down_since) down_since = now;
  if (!ap_active && now - down_since > kApAfterMs && now - last_ap_try > kApAfterMs) {
    last_ap_try = now;
    startAp();
  }
  if (config.wifi_ssid.length() && now - last_retry > kRetryMs) {
    last_retry = now;
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_pass.c_str());
  }
}

bool netConnect(const String& ssid, const String& pass) {
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), pass.c_str());
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(200);
  if (WiFi.status() != WL_CONNECTED) {
    if (config.wifi_ssid.length()) WiFi.begin(config.wifi_ssid.c_str(), config.wifi_pass.c_str());
    return false;
  }
  config.wifi_ssid = ssid;
  config.wifi_pass = pass;
  config.wifi_verified = true;
  config.save();
  return true;
}

bool netConnected() { return WiFi.status() == WL_CONNECTED; }
bool netApActive() { return ap_active; }
bool netApInUse() { return ap_active && WiFi.softAPgetStationNum() > 0; }

String netUrl() { return "http://" + WiFi.localIP().toString() + "/"; }

void netToJson(JsonObject out) {
  out["connected"] = netConnected();
  out["ssid"] = config.wifi_ssid;
  out["ip"] = WiFi.localIP().toString();
  out["rssi"] = netConnected() ? WiFi.RSSI() : 0;
  out["hostname"] = host;
  out["ap"] = ap_active;
  if (ap_active) out["ap_ip"] = WiFi.softAPIP().toString();
}
