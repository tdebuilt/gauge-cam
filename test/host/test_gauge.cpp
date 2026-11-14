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
  failures += testJumpFilter();
  return failures ? 1 : 0;
}
