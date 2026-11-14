// Needle gauge reader: pivot detection, needle search and multi-point scale.
//
// Portable C++ (no Arduino dependency) so it runs on the ESP32 and in host
// tests. Angles use the clock convention: 0 degrees = 12 o'clock, increasing
// clockwise, image Y axis pointing down.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gauge {

// One brightness byte per pixel, row-major. Brightness is the max of the
// colour channels so that coloured marks stay bright and a black needle dark.
struct Image {
  int width = 0;
  int height = 0;
  const uint8_t* data = nullptr;
  uint8_t at(int x, int y) const { return data[y * width + x]; }
};

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
};

// A graduation of the dial: pixel position and value, given in dial order.
struct Point {
  float x = 0, y = 0, value = 0;
};

// Maps a clock angle to a value by piecewise-linear interpolation.
struct Scale {
  float start = 0;  // clock angle of the first point
  int direction = 1;  // +1 clockwise, -1 counter-clockwise
  std::vector<float> offsets;  // strictly increasing arc offsets, degrees
  std::vector<float> values;

  float span() const { return offsets.empty() ? 0.f : offsets.back(); }
  float offset(float angle) const;
  float angleAt(float offset) const;
  float value(float angle) const;
};

enum class PivotMode { Fixed, Hub };

struct Settings {
  PivotMode pivot_mode = PivotMode::Hub;
  // Pivot clicked by the user. Fixed mode uses it as is; hub mode searches
  // the dark hub around it, which follows small camera shifts.
  float pivot_x = 0, pivot_y = 0;
  std::vector<Point> points;
};

struct Result {
  bool ok = false;
  std::string error;
  float cx = 0, cy = 0;  // pivot used
  int hub_radius = 0;  // 0 when no hub was measured
  float r_start = 0, r_end = 0;  // needle search band, pixels from the pivot
  float needle_angle = 0;  // clock degrees
  float contrast = 0;  // median ray brightness minus needle ray brightness
  float value = 0;
  Scale scale;
};

float clockAngle(float cx, float cy, float x, float y);

bool buildScale(const std::vector<Point>& points, float cx, float cy,
                Scale& out, std::string& error);

// Dark disc around (hint_x, hint_y) within search_radius pixels.
bool detectHub(const Image& img, float hint_x, float hint_y,
               float search_radius, float& cx, float& cy, int& radius);

// Darkest ray within the scale arc (plus a margin).
bool findNeedle(const Image& img, float cx, float cy, float r_start,
                float r_end, const Scale& scale, float& angle,
                float& contrast, float margin_deg = 4.f,
                float step_deg = 0.25f);

// Full reading. Settings coordinates are those of `img`.
Result read(const Image& img, const Settings& settings);

// Reading on a region of interest of a larger image. Settings and result
// coordinates are those of the full image.
Result readRoi(const Image& full, const Rect& roi, const Settings& settings);

// Brightness (max channel) of an interleaved 3-byte-per-pixel image,
// restricted to roi, into out (resized to roi.w * roi.h).
void brightnessFromRgb(const uint8_t* rgb, int width, int height,
                       const Rect& roi, std::vector<uint8_t>& out);

// Rejects isolated jumps of the level (a reflection, a shadow taken for the
// needle). A level further than max_jump from the last accepted one becomes a
// candidate, accepted once `confirmations` further readings agree with it
// (within max_jump / 2): a refill gets through after a few readings, a single
// wrong one does not. max_jump <= 0 disables the filter.
class JumpFilter {
 public:
  enum class Verdict { Accepted, Suspect, Confirmed };

  Verdict feed(float level, float max_jump, int confirmations);
  // Forgets the reference, e.g. after a calibration change.
  void reset() { has_reference_ = false; pending_ = 0; }
  bool hasReference() const { return has_reference_; }
  float reference() const { return reference_; }
  // Readings that agree with the current candidate so far (0: none).
  int pending() const { return pending_; }

 private:
  bool has_reference_ = false;
  float reference_ = 0;
  float candidate_ = 0;
  int pending_ = 0;
};

// Clockwise rotation by a multiple of 90 degrees. For 90 degrees the pixel
// (x, y) of a width x height image lands on (height - 1 - y, x) of a
// height x width image.
void rotatePoint(float& x, float& y, int width, int height, int degrees);

// Rotates an interleaved 3-byte-per-pixel image into dst (same size in
// bytes, must not overlap src). See rotatePoint for the convention.
void rotateRgb(const uint8_t* src, int width, int height, int degrees, uint8_t* dst);

}  // namespace gauge
