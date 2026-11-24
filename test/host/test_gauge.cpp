// Host test: reads every case of cases.txt (see tools/make_fixtures.py).
//
//   test/host/run.sh
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "gauge.h"

namespace {

bool readPpm(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgb) {
  std::ifstream in(path, std::ios::binary);
  std::string magic;
  int maxval = 0;
  in >> magic >> w >> h >> maxval;
  in.get();
  if (magic != "P6" || maxval != 255 || w <= 0 || h <= 0) return false;
  rgb.resize(static_cast<size_t>(w) * h * 3);
  in.read(reinterpret_cast<char*>(rgb.data()), rgb.size());
  return static_cast<size_t>(in.gcount()) == rgb.size();
}

// Settings and region of interest of the same dial in a rotated photo.
void rotateCase(gauge::Settings& s, gauge::Rect& roi, int w, int h, int degrees) {
  gauge::rotatePoint(s.pivot_x, s.pivot_y, w, h, degrees);
  for (gauge::Point& p : s.points) gauge::rotatePoint(p.x, p.y, w, h, degrees);
  if (roi.w <= 0 || roi.h <= 0) return;
  float x0 = roi.x, y0 = roi.y, x1 = roi.x + roi.w - 1, y1 = roi.y + roi.h - 1;
  gauge::rotatePoint(x0, y0, w, h, degrees);
  gauge::rotatePoint(x1, y1, w, h, degrees);
  roi = {static_cast<int>(std::min(x0, x1)), static_cast<int>(std::min(y0, y1)),
         static_cast<int>(std::fabs(x1 - x0)) + 1, static_cast<int>(std::fabs(y1 - y0)) + 1};
}

// Unit checks of JumpFilter; returns the number of failures.
// Hub smaller than the needle is thick, lighter cap in its middle, blurred:
// the needle root joined to the hub must not pull the centre towards it.
// Same proportions as a photo of an Afriso cellar gauge at 640x480.
int testHubCentre() {
  constexpr int kW = 240, kH = 200;
  constexpr float kCx = 120.4f, kCy = 97.7f, kHub = 26, kCap = 11, kNeedle = 3.5f;
  int failures = 0;
  float worst = 0;
  for (int deg = 0; deg < 360; deg += 15) {
    const float a = deg * 3.14159265f / 180, sa = std::sin(a), ca = std::cos(a);
    std::vector<float> sharp(kW * kH);
    for (int y = 0; y < kH; ++y) {
      for (int x = 0; x < kW; ++x) {
        const float dx = x - kCx, dy = y - kCy, d = std::hypot(dx, dy);
        const float along = dx * sa - dy * ca, across = std::fabs(dx * ca + dy * sa);
        float v = 200;  // dial face
        if (along > 0 && along < 90 && across < kNeedle) v = 25;  // needle
        if (d < kHub) v = 30;                                       // hub
        if (d < kCap) v = 95;                                       // lighter cap
        sharp[y * kW + x] = v;
      }
    }
    std::vector<uint8_t> px(kW * kH);
    for (int y = 0; y < kH; ++y) {  // 5x5 box blur, like a soft focus
      for (int x = 0; x < kW; ++x) {
        float sum = 0;
        int n = 0;
        for (int j = -2; j <= 2; ++j) {
          for (int i = -2; i <= 2; ++i) {
            const int xx = x + i, yy = y + j;
            if (xx < 0 || yy < 0 || xx >= kW || yy >= kH) continue;
            sum += sharp[yy * kW + xx];
            ++n;
          }
        }
        px[y * kW + x] = static_cast<uint8_t>(sum / n + 0.5f);
      }
    }
    gauge::Image img{kW, kH, px.data()};
    float cx = 0, cy = 0;
    int r = 0;
    // A click a few pixels off, towards the needle.
    const bool ok = gauge::detectHub(img, kCx + 6 * sa, kCy - 6 * ca, 0.12f * kH, cx, cy, r);
    const float err = std::hypot(cx - kCx, cy - kCy);
    worst = std::max(worst, err);
    if (!ok || err > 1.f || std::abs(r - kHub) > 2) {
      std::printf("FAIL hub centre, needle at %d deg: (%.1f, %.1f) r %d, "
                  "expected (%.1f, %.1f) r %.0f\n", deg, cx, cy, r, kCx, kCy, kHub);
      ++failures;
    }
  }
  std::printf("%s hub centre beside a thick needle (worst %.2f px)\n", failures ? "FAIL" : "OK  ",
              worst);
  return failures;
}

int testJumpFilter() {
  using V = gauge::JumpFilter::Verdict;
  struct Step { float level; V expected; };
  const std::vector<std::vector<Step>> scenarios = {
      // Slow consumption, then one wrong reading, then normal again.
      {{100, V::Accepted}, {99.5f, V::Accepted}, {40, V::Suspect}, {99.2f, V::Accepted}},
      // Refill: confirmed by two more readings that agree.
      {{40, V::Accepted}, {120, V::Suspect}, {120.5f, V::Suspect}, {120.2f, V::Confirmed},
       {119.9f, V::Accepted}},
      // Two different wrong readings do not confirm each other.
      {{80, V::Accepted}, {20, V::Suspect}, {140, V::Suspect}, {30, V::Suspect}, {80.4f, V::Accepted}},
  };
  int failures = 0;
  for (size_t i = 0; i < scenarios.size(); ++i) {
    gauge::JumpFilter f;
    for (size_t k = 0; k < scenarios[i].size(); ++k) {
      const Step& step = scenarios[i][k];
      if (f.feed(step.level, 5, 2) != step.expected) {
        std::printf("FAIL jump filter scenario %zu step %zu (level %g)\n", i, k, step.level);
        ++failures;
      }
    }
  }
  gauge::JumpFilter off;
  off.feed(100, 0, 2);
  if (off.feed(10, 0, 2) != V::Accepted) {
    std::printf("FAIL jump filter: max_jump 0 must accept everything\n");
    ++failures;
  }
  std::printf("%s jump filter\n", failures ? "FAIL" : "OK  ");
  return failures;
}


// Tank volumes against hand-computed values; returns the number of failures.
int testTank() {
  using gauge::Tank;
  using gauge::TankShape;
  int failures = 0;
  auto check = [&failures](const char* what, float got, float want, float tol) {
    if (std::isnan(got) != std::isnan(want) || (!std::isnan(want) && std::fabs(got - want) > tol)) {
      std::printf("FAIL tank %s: %g, expected %g\n", what, got, want);
      ++failures;
    }
  };
  Tank box;
  box.length_cm = 200;
  box.width_cm = 100;
  check("box not full height", box.volume(50), 1000, 0.01f);
  check("box percent unknown", box.percent(50), NAN, 0);
  box.height_cm = 150;
  check("box capacity", box.capacity(), 3000, 0.01f);
  check("box clamp high", box.volume(170), 3000, 0.01f);
  check("box clamp low", box.volume(-5), 0, 0.01f);
  check("box percent", box.percent(75), 50, 0.01f);

  Tank h;  // 100 cm diameter, 200 cm long: 1570.8 L
  h.shape = TankShape::HorizontalCylinder;
  h.length_cm = 200;
  h.diameter_cm = 100;
  check("hcyl capacity", h.capacity(), 1570.80f, 0.1f);
  check("hcyl half", h.volume(50), 785.40f, 0.1f);
  check("hcyl quarter height", h.volume(25), 307.09f, 0.1f);  // segment 1535.5 cm2
  check("hcyl three quarters", h.volume(75), 1570.80f - 307.09f, 0.1f);
  check("hcyl empty", h.volume(0), 0, 0.01f);
  check("hcyl percent", h.percent(50), 50, 0.01f);

  Tank v;
  v.shape = TankShape::VerticalCylinder;
  v.diameter_cm = 100;
  v.height_cm = 120;
  check("vcyl 10 cm", v.volume(10), 78.54f, 0.01f);
  check("vcyl capacity", v.capacity(), 942.48f, 0.05f);

  Tank t;
  t.shape = TankShape::Table;
  t.table = {{0, 0}, {40, 500}, {150, 3000}};
  check("table interpolated", t.volume(95), 1750, 0.01f);
  check("table first point", t.volume(-3), 0, 0.01f);
  check("table capacity", t.capacity(), 3000, 0.01f);
  check("table percent", t.percent(40), 500.f / 30, 0.01f);
  t.table = {{0, 0}};
  check("table not ready", t.volume(10), NAN, 0);
  std::printf("%s tank volumes\n", failures ? "FAIL" : "OK  ");
  return failures;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: test_gauge cases.txt\n";
    return 2;
  }
  std::ifstream cases(argv[1]);
  std::string line;
  int total = 0, failures = 0;
  while (std::getline(cases, line)) {
    if (line.empty()) continue;
    std::istringstream in(line);
    std::string name, file, mode;
    gauge::Rect roi;
    gauge::Settings s;
    float expected = 0, tolerance = 0;
    int n = 0;
    in >> name >> file >> roi.x >> roi.y >> roi.w >> roi.h >> mode >> s.pivot_x >>
        s.pivot_y >> expected >> tolerance >> n;
    s.pivot_mode = mode == "fixed" ? gauge::PivotMode::Fixed : gauge::PivotMode::Hub;
    for (int i = 0; i < n; ++i) {
      gauge::Point p;
      in >> p.x >> p.y >> p.value;
      s.points.push_back(p);
    }

    int w = 0, h = 0;
    std::vector<uint8_t> rgb, rotated, bright;
    if (!readPpm(file, w, h, rgb)) {
      ++total;
      std::printf("FAIL %-22s cannot read %s\n", name.c_str(), file.c_str());
      ++failures;
      continue;
    }
    // Every case again in a photo rotated like the firmware does it.
    for (int degrees : {0, 90, 180, 270}) {
      gauge::Settings rs = s;
      gauge::Rect rroi = roi;
      const uint8_t* data = rgb.data();
      int iw = w, ih = h;
      if (degrees) {
        rotated.resize(rgb.size());
        gauge::rotateRgb(rgb.data(), w, h, degrees, rotated.data());
        rotateCase(rs, rroi, w, h, degrees);
        data = rotated.data();
        if (degrees != 180) std::swap(iw, ih);
      }
      const std::string label = degrees ? name + "@" + std::to_string(degrees) : name;
      ++total;
      gauge::brightnessFromRgb(data, iw, ih, gauge::Rect{}, bright);
      gauge::Image img{iw, ih, bright.data()};
      const gauge::Result r = gauge::readRoi(img, rroi, rs);
      const bool ok = r.ok && std::fabs(r.value - expected) <= tolerance;
      failures += !ok;
      if (r.ok) {
        std::printf("%s %-26s %7.2f (expected %g +- %g) pivot (%.0f, %.0f) hub %d "
                    "needle %.1f contrast %.0f\n",
                    ok ? "OK  " : "FAIL", label.c_str(), r.value, expected, tolerance,
                    r.cx, r.cy, r.hub_radius, r.needle_angle, r.contrast);
      } else {
        std::printf("FAIL %-26s error: %s\n", label.c_str(), r.error.c_str());
      }
    }
  }
  std::printf("%d/%d cases within tolerance\n", total - failures, total);
  failures += testHubCentre();
  failures += testJumpFilter();
  failures += testTank();
  return failures ? 1 : 0;
}
