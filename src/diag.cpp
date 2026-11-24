#include "diag.h"

#include <esp_attr.h>
#include <esp_system.h>
#include <stdarg.h>
#include <time.h>

#include "config.h"

namespace {

constexpr size_t kLines = 80;
constexpr size_t kLineSize = 120;
constexpr uint32_t kMagic = 0x6761636d;  // "gacm"
constexpr uint32_t kValidEpoch = 1700000000;  // the clock is set after this

struct Line {
  uint32_t uptime_s;
  uint32_t epoch;
  bool previous;  // written before the last restart
  char text[kLineSize];
};

Line* lines = nullptr;
size_t line_count = 0;  // lines written since boot
String previous_note;

// Kept across a software restart, lost at power on.
struct Persistent {
  uint32_t magic;
  char note[96];
  uint8_t camera_restarts;
};
RTC_NOINIT_ATTR Persistent persistent;

// The last lines of the log, kept across a software restart or a crash so
// that the log still explains what happened before it.
constexpr size_t kKeptLines = 16;
constexpr size_t kKeptSize = 100;
constexpr uint32_t kKeptMagic = 0x67616c31;  // "gal1"
struct KeptLog {
  uint32_t magic;
  uint32_t count;
  struct {
    uint32_t uptime_s;
    uint32_t epoch;
    char text[kKeptSize];
  } lines[kKeptLines];
};
RTC_NOINIT_ATTR KeptLog kept;

struct Reason {
  esp_reset_reason_t code;
  const char* id;
  const char* fr;
  const char* en;
};
constexpr Reason kReasons[] = {
    {ESP_RST_POWERON, "poweron", "Mise sous tension", "Power on"},
    {ESP_RST_EXT, "external", "Bouton reset", "Reset button"},
    {ESP_RST_SW, "software", "Redémarrage logiciel", "Software restart"},
    {ESP_RST_PANIC, "panic", "Plantage", "Crash"},
    {ESP_RST_INT_WDT, "int_wdt", "Chien de garde (interruptions)", "Interrupt watchdog"},
    {ESP_RST_TASK_WDT, "task_wdt", "Chien de garde (programme bloqué)", "Task watchdog (stuck)"},
    {ESP_RST_WDT, "wdt", "Chien de garde", "Watchdog"},
    {ESP_RST_DEEPSLEEP, "deepsleep", "Sortie de veille", "Wake from sleep"},
    {ESP_RST_BROWNOUT, "brownout", "Sous-tension (alimentation trop faible)", "Brown-out (weak power supply)"},
    {ESP_RST_SDIO, "sdio", "SDIO", "SDIO"},
};
const Reason* reason = nullptr;

}  // namespace

void diagBegin() {
  const esp_reset_reason_t code = esp_reset_reason();
  for (const Reason& r : kReasons) {
    if (r.code == code) reason = &r;
  }
  if (code == ESP_RST_POWERON || code == ESP_RST_BROWNOUT || persistent.magic != kMagic) {
    memset(&persistent, 0, sizeof persistent);
    persistent.magic = kMagic;
  }
  persistent.note[sizeof persistent.note - 1] = 0;
  previous_note = persistent.note;
  persistent.note[0] = 0;  // a later crash must not reuse this note

  lines = static_cast<Line*>(ps_malloc(kLines * sizeof(Line)));
  if (!lines) lines = static_cast<Line*>(malloc(kLines * sizeof(Line)));

  const bool cold = code == ESP_RST_POWERON || code == ESP_RST_BROWNOUT;
  if (!cold && kept.magic == kKeptMagic && lines) {
    const uint32_t first = kept.count > kKeptLines ? kept.count - kKeptLines : 0;
    for (uint32_t i = first; i < kept.count; ++i) {
      const auto& k = kept.lines[i % kKeptLines];
      Line& line = lines[line_count++ % kLines];
      line.uptime_s = k.uptime_s;
      line.epoch = k.epoch;
      line.previous = true;
      strlcpy(line.text, k.text, sizeof line.text);
    }
  }
  kept.magic = kKeptMagic;
  kept.count = 0;
}

void logMsg(const char* fmt, ...) {
  char text[kLineSize];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof text, fmt, args);
  va_end(args);
  Serial.println(text);
  if (!lines) return;
  Line& line = lines[line_count % kLines];
  line.uptime_s = millis() / 1000;
  line.epoch = epochNow();
  line.previous = false;
  memcpy(line.text, text, sizeof text);
  ++line_count;
  auto& k = kept.lines[kept.count++ % kKeptLines];
  k.uptime_s = line.uptime_s;
  k.epoch = line.epoch;
  strlcpy(k.text, text, sizeof k.text);
}

void logToJson(JsonArray out) {
  if (!lines) return;
  const size_t first = line_count > kLines ? line_count - kLines : 0;
  for (size_t i = first; i < line_count; ++i) {
    const Line& line = lines[i % kLines];
    JsonObject o = out.add<JsonObject>();
    o["t"] = line.uptime_s;
    o["at"] = line.epoch;
    o["msg"] = line.text;
    if (line.previous) o["previous"] = true;
  }
}

const char* resetReasonId() { return reason ? reason->id : "unknown"; }

const char* resetReasonLabel() {
  if (!reason) return config.lang == "en" ? "Unknown" : "Inconnue";
  return config.lang == "en" ? reason->en : reason->fr;
}

const String& previousRestartNote() { return previous_note; }

void diagRestart(const String& why) {
  logMsg("Restarting: %s", why.c_str());
  strlcpy(persistent.note, why.c_str(), sizeof persistent.note);
  persistent.magic = kMagic;
  Serial.flush();
  delay(200);
  ESP.restart();
  while (true) {
  }
}

uint8_t cameraRestartCount() { return persistent.camera_restarts; }

void setCameraRestartCount(uint8_t n) { persistent.camera_restarts = n; }

void clockBegin(const String& server) {
  if (server.length()) configTime(0, 0, server.c_str());
}

uint32_t epochNow() {
  const time_t now = time(nullptr);
  return now > kValidEpoch ? static_cast<uint32_t>(now) : 0;
}

String isoTime(uint32_t epoch) {
  if (!epoch) return "";
  const time_t t = epoch;
  struct tm tm;
  gmtime_r(&t, &tm);
  char buf[32];
  strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S+00:00", &tm);
  return buf;
}
