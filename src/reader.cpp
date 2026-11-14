#include "reader.h"

#include <img_converters.h>

#include "camera.h"

namespace {

Reading attempt;
Reading good;
constexpr size_t kFailures = 4;
Failure failures[kFailures];
uint32_t failure_count = 0;

// Decodes the JPEG and keeps the brightness (max channel) of every pixel.
bool brightness(const Photo& p, std::vector<uint8_t>& out, String& error) {
  const size_t rgb_size = static_cast<size_t>(p.width) * p.height * 3;
  uint8_t* rgb = static_cast<uint8_t*>(ps_malloc(rgb_size));
  if (!rgb) {
    error = "out of memory to decode the photo";
    return false;
  }
  const bool decoded = fmt2rgb888(p.jpeg, p.size, PIXFORMAT_JPEG, rgb);
  if (decoded) gauge::brightnessFromRgb(rgb, p.width, p.height, gauge::Rect{}, out);
  free(rgb);
  if (!decoded) error = "cannot decode the photo";
  return decoded;
}

void putNumber(JsonObject out, const char* key, float v, int decimals) {
  if (isnan(v)) {
    out[key] = nullptr;
  } else {
    const float scale = powf(10, decimals);
    out[key] = roundf(v * scale) / scale;
  }
}

}  // namespace

Reading readPhoto(const Config& cfg) {
  Reading r;
  r.at_ms = millis();
  const Photo& p = lastPhoto();
  if (!p.size) {
    r.error = "no photo yet";
    return r;
  }
  if (!cfg.dialReady()) {
    r.error = "dial not configured: set the pivot, the start and the end";
    return r;
  }
  std::vector<uint8_t> bright;
  if (!brightness(p, bright, r.error)) return r;
  gauge::Image img{p.width, p.height, bright.data()};
  r.gauge = gauge::readRoi(img, cfg.roi, cfg.gaugeSettings());
  if (!r.gauge.ok) {
    r.error = r.gauge.error.c_str();
    return r;
  }
  if (r.gauge.contrast < cfg.min_contrast) {
    r.error = "needle contrast too low (" + String(r.gauge.contrast, 0) +
              " < " + String(cfg.min_contrast, 0) + "): check the light";
    return r;
  }
  r.level = r.gauge.value + cfg.level_offset_cm;
  if (cfg.tankReady()) {
    r.volume_l = r.level * cfg.tank_length_cm * cfg.tank_width_cm / 1000.f;
  }
  if (cfg.tank_height_cm > 0) r.percent = 100.f * r.level / cfg.tank_height_cm;
  r.ok = true;
  return r;
}

void readingToJson(const Reading& r, const Config& cfg, JsonObject out) {
  out["ok"] = r.ok;
  if (r.error.length()) out["error"] = r.error;
  if (r.suspect) out["suspect"] = true;
  out["age_s"] = r.at_ms ? (millis() - r.at_ms) / 1000 : -1;
  putNumber(out, "level", r.level, 1);
  out["unit"] = cfg.unit;
  putNumber(out, "volume_l", r.volume_l, 0);
  putNumber(out, "percent", r.percent, 1);
  const gauge::Result& g = r.gauge;
  if (g.scale.offsets.empty()) return;  // pivot or scale not found
  JsonObject d = out["detail"].to<JsonObject>();
  d["cx"] = g.cx;
  d["cy"] = g.cy;
  d["hub_radius"] = g.hub_radius;
  d["r_start"] = g.r_start;
  d["r_end"] = g.r_end;
  d["scale_start"] = g.scale.start;
  d["scale_direction"] = g.scale.direction;
  d["scale_span"] = g.scale.span();
  if (g.ok) {
    putNumber(d, "needle_angle", g.needle_angle, 1);
    putNumber(d, "contrast", g.contrast, 0);
    putNumber(d, "gauge_value", g.value, 2);
  }
}

const Reading& lastAttempt() { return attempt; }
const Reading& lastGood() { return good; }

void storeReading(const Reading& r) {
  attempt = r;
  if (r.ok) good = r;
}

void keepFailure(const Reading& r, const uint8_t* jpeg, size_t size) {
  Failure& slot = failures[failure_count % kFailures];
  free(slot.jpeg);
  slot = Failure();
  slot.id = ++failure_count;
  slot.reading = r;
  if (jpeg && size) {
    slot.jpeg = static_cast<uint8_t*>(ps_malloc(size));
    if (slot.jpeg) {
      memcpy(slot.jpeg, jpeg, size);
      slot.size = size;
    }
  }
}

void failuresToJson(const Config& cfg, JsonArray out) {
  for (size_t i = 0; i < kFailures && i < failure_count; ++i) {
    const Failure& f = failures[(failure_count - 1 - i) % kFailures];
    JsonObject o = out.add<JsonObject>();
    o["id"] = f.id;
    o["photo"] = f.size > 0;
    readingToJson(f.reading, cfg, o);
  }
}

const Failure* findFailure(uint32_t id) {
  for (const Failure& f : failures) {
    if (f.id && f.id == id) return &f;
  }
  return nullptr;
}

uint32_t lastFailureId() { return failure_count; }
