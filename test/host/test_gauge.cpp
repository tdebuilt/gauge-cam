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
  return failures ? 1 : 0;
}
