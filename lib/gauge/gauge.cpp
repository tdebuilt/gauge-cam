#include "gauge.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace gauge {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

float wrap360(float a) {
  float r = std::fmod(a, 360.f);
  return r < 0 ? r + 360.f : r;
}

float radians(float deg) { return deg * kPi / 180.f; }

int medianOf(const Image& img) {
  uint32_t hist[256] = {0};
  const size_t n = static_cast<size_t>(img.width) * img.height;
  for (size_t i = 0; i < n; ++i) hist[img.data[i]]++;
  size_t seen = 0;
  for (int v = 0; v < 256; ++v) {
    seen += hist[v];
    if (seen * 2 >= n) return v;
  }
  return 255;
}

float medianOf(std::vector<float> values) {
  if (values.empty()) return kNaN;
  const size_t mid = values.size() / 2;
  std::nth_element(values.begin(), values.begin() + mid, values.end());
  float upper = values[mid];
  if (values.size() % 2) return upper;
  float lower = *std::max_element(values.begin(), values.begin() + mid);
  return (lower + upper) / 2;
}

// Smallest radius whose ring is brighter on average than threshold, 0 if none.
int ringRadius(const Image& img, float cx, float cy, float threshold,
               int max_r) {
  constexpr int kSamples = 96;
  float cosv[kSamples], sinv[kSamples];
  for (int i = 0; i < kSamples; ++i) {
    cosv[i] = std::cos(2 * kPi * i / kSamples);
    sinv[i] = std::sin(2 * kPi * i / kSamples);
  }
  for (int r = 2; r < max_r; ++r) {
    int count = 0;
    float sum = 0;
    for (int i = 0; i < kSamples; ++i) {
      const int x = static_cast<int>(std::lround(cx + r * cosv[i]));
      const int y = static_cast<int>(std::lround(cy + r * sinv[i]));
      if (x < 0 || y < 0 || x >= img.width || y >= img.height) continue;
      sum += img.at(x, y);
      ++count;
    }
    if (count < kSamples / 2) return 0;
    if (sum / count > threshold) return r;
  }
  return 0;
}

// Mean brightness of the (2k+1)^2 box around each pixel, clamped at edges.
class BoxMean {
 public:
  BoxMean(const Image& img, int k) : img_(img), k_(k) {
    const int w = img.width + 1;
    sums_.assign(static_cast<size_t>(w) * (img.height + 1), 0);
    for (int y = 0; y < img.height; ++y) {
      uint32_t row = 0;
      for (int x = 0; x < img.width; ++x) {
        row += img.at(x, y);
        sums_[(y + 1) * w + x + 1] = sums_[y * w + x + 1] + row;
      }
    }
  }
  float at(int x, int y) const {
    const int w = img_.width + 1;
    const int x0 = std::max(0, x - k_), x1 = std::min(img_.width, x + k_ + 1);
    const int y0 = std::max(0, y - k_), y1 = std::min(img_.height, y + k_ + 1);
    const uint32_t s = sums_[y1 * w + x1] - sums_[y0 * w + x1] -
                       sums_[y1 * w + x0] + sums_[y0 * w + x0];
    return static_cast<float>(s) / ((x1 - x0) * (y1 - y0));
  }

 private:
  const Image& img_;
  int k_;
  std::vector<uint32_t> sums_;
};

}  // namespace

float clockAngle(float cx, float cy, float x, float y) {
  return wrap360(std::atan2(x - cx, cy - y) * 180.f / kPi);
}

float Scale::offset(float angle) const {
  return wrap360((angle - start) * direction);
}

float Scale::angleAt(float off) const { return wrap360(start + direction * off); }

float Scale::value(float angle) const {
  float off = offset(angle);
  const float end = span();
  if (off > end) {
    // Dead zone past the last point: snap to the nearest end.
    off = (off - end < 360.f - off) ? end : 0.f;
  }
  if (off <= offsets.front()) return values.front();
  for (size_t i = 1; i < offsets.size(); ++i) {
    if (off <= offsets[i]) {
      const float t = (off - offsets[i - 1]) / (offsets[i] - offsets[i - 1]);
      return values[i - 1] + t * (values[i] - values[i - 1]);
    }
  }
  return values.back();
}

bool buildScale(const std::vector<Point>& points, float cx, float cy,
                Scale& out, std::string& error) {
  if (points.size() < 2) {
    error = "calibration needs at least 2 points";
    return false;
  }
  std::vector<float> angles;
  for (const Point& p : points) angles.push_back(clockAngle(cx, cy, p.x, p.y));
  for (int direction : {1, -1}) {
    std::vector<float> offsets;
    bool increasing = true;
    for (float a : angles) {
      const float off = wrap360((a - angles[0]) * direction);
      if (!offsets.empty() && off <= offsets.back()) increasing = false;
      offsets.push_back(off);
    }
    if (increasing) {
      out.start = angles[0];
      out.direction = direction;
      out.offsets = offsets;
      out.values.clear();
      for (const Point& p : points) out.values.push_back(p.value);
      return true;
    }
  }
  error = "calibration points are not in dial order around the pivot";
  return false;
}

bool detectHub(const Image& img, float hint_x, float hint_y,
               float search_radius, float& cx, float& cy, int& radius) {
  const int min_dim = std::min(img.width, img.height);
  const int k = std::max(2, static_cast<int>(min_dim * 0.04f));
  BoxMean blurred(img, k);

  const int x0 = std::max(0, static_cast<int>(hint_x - search_radius));
  const int x1 = std::min(img.width - 1, static_cast<int>(hint_x + search_radius));
  const int y0 = std::max(0, static_cast<int>(hint_y - search_radius));
  const int y1 = std::min(img.height - 1, static_cast<int>(hint_y + search_radius));
  if (x0 > x1 || y0 > y1) return false;
  float best = std::numeric_limits<float>::max();
  int bx = x0, by = y0;
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      const float v = blurred.at(x, y);
      if (v < best) {
        best = v;
        bx = x;
        by = y;
      }
    }
  }
  cx = static_cast<float>(bx);
  cy = static_cast<float>(by);
  const float threshold = (best + medianOf(img)) / 2;
  const int max_r = min_dim / 4;

  radius = ringRadius(img, cx, cy, threshold, max_r);
  if (radius == 0) return false;
  // Refine the centre with the centroid of the dark pixels around it.
  const int win = static_cast<int>(radius * 1.5f);
  const float limit = (radius * 1.2f) * (radius * 1.2f);
  double sx = 0, sy = 0;
  long count = 0;
  for (int y = std::max(0, by - win); y <= std::min(img.height - 1, by + win); ++y) {
    for (int x = std::max(0, bx - win); x <= std::min(img.width - 1, bx + win); ++x) {
      const float dx = x - cx, dy = y - cy;
      if (dx * dx + dy * dy <= limit && img.at(x, y) < threshold) {
        sx += x;
        sy += y;
        ++count;
      }
    }
  }
  if (count > 0) {
    cx = static_cast<float>(sx / count);
    cy = static_cast<float>(sy / count);
    const int refined = ringRadius(img, cx, cy, threshold, max_r);
    if (refined) radius = refined;
  }
  return radius >= std::max(3, static_cast<int>(min_dim * 0.015f));
}

bool findNeedle(const Image& img, float cx, float cy, float r_start,
                float r_end, const Scale& scale, float& angle,
                float& contrast, float margin_deg, float step_deg) {
  const float span = std::min(360.f, scale.span() + 2 * margin_deg);
  const float first = span < 360.f ? -margin_deg : 0.f;
  const int steps = static_cast<int>(std::ceil(span / step_deg));
  const int r0 = static_cast<int>(r_start), r1 = static_cast<int>(r_end);
  const int radii = r1 - r0;
  if (radii < 3 || steps < 1) return false;

  std::vector<float> profile(steps, kNaN);
  for (int i = 0; i < steps; ++i) {
    const float a = radians(scale.angleAt(first + i * step_deg));
    const float sa = std::sin(a), ca = std::cos(a);
    int count = 0;
    float sum = 0;
    for (int r = r0; r < r1; ++r) {
      const int x = static_cast<int>(std::lround(cx + r * sa));
      const int y = static_cast<int>(std::lround(cy - r * ca));
      if (x < 0 || y < 0 || x >= img.width || y >= img.height) continue;
      sum += img.at(x, y);
      ++count;
    }
    if (count * 2 >= radii) profile[i] = sum / count;
  }

  int best = -1;
  std::vector<float> valid;
  for (int i = 0; i < steps; ++i) {
    if (std::isnan(profile[i])) continue;
    valid.push_back(profile[i]);
    if (best < 0 || profile[i] < profile[best]) best = i;
  }
  if (best < 0) return false;
  // Sub-step refinement with a parabola through the minimum and neighbours.
  float shift = 0;
  if (best > 0 && best < steps - 1 && !std::isnan(profile[best - 1]) &&
      !std::isnan(profile[best + 1])) {
    const float l = profile[best - 1], m = profile[best], r = profile[best + 1];
    const float denom = l - 2 * m + r;
    if (denom > 0) shift = 0.5f * (l - r) / denom;
  }
  angle = scale.angleAt(first + (best + shift) * step_deg);
  contrast = medianOf(valid) - profile[best];
  return true;
}

Result read(const Image& img, const Settings& settings) {
  Result res;
  if (!img.data || img.width < 16 || img.height < 16) {
    res.error = "image too small";
    return res;
  }
  if (settings.points.size() < 2) {
    res.error = "set at least the start and end of the scale";
    return res;
  }
  const int min_dim = std::min(img.width, img.height);

  if (settings.pivot_mode == PivotMode::Hub) {
    const float search = std::max(10.f, 0.12f * min_dim);
    if (!detectHub(img, settings.pivot_x, settings.pivot_y, search, res.cx,
                   res.cy, res.hub_radius)) {
      res.error = "needle hub not found near the pivot";
      return res;
    }
  } else {
    res.cx = settings.pivot_x;
    res.cy = settings.pivot_y;
    // Measure the hub anyway, to start the needle search past it.
    float local = 0;
    int n = 0;
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        const int x = static_cast<int>(res.cx) + dx, y = static_cast<int>(res.cy) + dy;
        if (x < 0 || y < 0 || x >= img.width || y >= img.height) continue;
        local += img.at(x, y);
        ++n;
      }
    }
    if (n) {
      const float threshold = (local / n + medianOf(img)) / 2;
      res.hub_radius = ringRadius(img, res.cx, res.cy, threshold, min_dim / 4);
      if (res.hub_radius < 3) res.hub_radius = 0;
    }
  }

  if (!buildScale(settings.points, res.cx, res.cy, res.scale, res.error)) {
    return res;
  }
  float nearest = std::numeric_limits<float>::max();
  for (const Point& p : settings.points) {
    nearest = std::min(nearest, std::hypot(p.x - res.cx, p.y - res.cy));
  }
  // Stop short of the graduations so ticks and numerals are not scanned.
  res.r_end = 0.85f * nearest;
  res.r_start = res.hub_radius ? 1.3f * res.hub_radius : std::max(5.f, res.r_end / 8);
  if (res.r_end - res.r_start < 5) {
    char buf[120];
    std::snprintf(buf, sizeof buf,
                  "needle search band too narrow (%.0f-%.0f px): graduations "
                  "too close to the pivot",
                  res.r_start, res.r_end);
    res.error = buf;
    return res;
  }
  if (!findNeedle(img, res.cx, res.cy, res.r_start, res.r_end, res.scale,
                  res.needle_angle, res.contrast)) {
    res.error = "needle not found";
    return res;
  }
  res.value = res.scale.value(res.needle_angle);
  res.ok = true;
  return res;
}

Result readRoi(const Image& full, const Rect& roi_in, const Settings& settings) {
  Rect roi = roi_in;
  if (roi.w <= 0 || roi.h <= 0) roi = {0, 0, full.width, full.height};
  roi.x = std::max(0, std::min(roi.x, full.width - 1));
  roi.y = std::max(0, std::min(roi.y, full.height - 1));
  roi.w = std::min(roi.w, full.width - roi.x);
  roi.h = std::min(roi.h, full.height - roi.y);

  std::vector<uint8_t> crop(static_cast<size_t>(roi.w) * roi.h);
  for (int y = 0; y < roi.h; ++y) {
    std::copy_n(full.data + (roi.y + y) * full.width + roi.x, roi.w,
                crop.begin() + static_cast<size_t>(y) * roi.w);
  }
  Image sub{roi.w, roi.h, crop.data()};
  Settings local = settings;
  local.pivot_x -= roi.x;
  local.pivot_y -= roi.y;
  for (Point& p : local.points) {
    p.x -= roi.x;
    p.y -= roi.y;
  }
  Result res = read(sub, local);
  res.cx += roi.x;
  res.cy += roi.y;
  return res;
}

void brightnessFromRgb(const uint8_t* rgb, int width, int height,
                       const Rect& roi_in, std::vector<uint8_t>& out) {
  Rect roi = roi_in;
  if (roi.w <= 0 || roi.h <= 0) roi = {0, 0, width, height};
  out.resize(static_cast<size_t>(roi.w) * roi.h);
  for (int y = 0; y < roi.h; ++y) {
    const uint8_t* src = rgb + (static_cast<size_t>(roi.y + y) * width + roi.x) * 3;
    uint8_t* dst = out.data() + static_cast<size_t>(y) * roi.w;
    for (int x = 0; x < roi.w; ++x, src += 3) {
      dst[x] = std::max(src[0], std::max(src[1], src[2]));
    }
  }
}

namespace {

int quarterTurns(int degrees) { return ((degrees / 90) % 4 + 4) % 4; }

}  // namespace

void rotatePoint(float& x, float& y, int width, int height, int degrees) {
  for (int i = quarterTurns(degrees); i > 0; --i) {
    const float nx = height - 1 - y;
    y = x;
    x = nx;
    std::swap(width, height);
  }
}

void rotateRgb(const uint8_t* src, int width, int height, int degrees, uint8_t* dst) {
  const int turns = quarterTurns(degrees);
  const int out_w = turns % 2 ? height : width;
  for (int y = 0; y < height; ++y) {
    const uint8_t* s = src + static_cast<size_t>(y) * width * 3;
    for (int x = 0; x < width; ++x, s += 3) {
      int nx = x, ny = y;
      switch (turns) {
        case 1: nx = height - 1 - y; ny = x; break;
        case 2: nx = width - 1 - x; ny = height - 1 - y; break;
        case 3: nx = y; ny = width - 1 - x; break;
      }
      uint8_t* d = dst + (static_cast<size_t>(ny) * out_w + nx) * 3;
      d[0] = s[0];
      d[1] = s[1];
      d[2] = s[2];
    }
  }
}

}  // namespace gauge
