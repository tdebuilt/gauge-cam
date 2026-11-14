// Command-line reader used by tools/mock_device.py: same code as the firmware.
//
//   gauge_cli image.ppm mode pivot_x pivot_y roi_x roi_y roi_w roi_h n x y value ...
//
// Prints the result as JSON on stdout.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "gauge.h"

int main(int argc, char** argv) {
  if (argc < 10) {
    std::fprintf(stderr, "usage: gauge_cli image.ppm mode px py rx ry rw rh n [x y v]...\n");
    return 2;
  }
  std::ifstream in(argv[1], std::ios::binary);
  std::string magic;
  int w = 0, h = 0, maxval = 0;
  in >> magic >> w >> h >> maxval;
  in.get();
  std::vector<uint8_t> rgb(static_cast<size_t>(w) * h * 3);
  in.read(reinterpret_cast<char*>(rgb.data()), rgb.size());
  if (magic != "P6" || !in) {
    std::fprintf(stderr, "cannot read %s\n", argv[1]);
    return 2;
  }
  gauge::Settings s;
  s.pivot_mode = std::string(argv[2]) == "fixed" ? gauge::PivotMode::Fixed : gauge::PivotMode::Hub;
  s.pivot_x = std::atof(argv[3]);
  s.pivot_y = std::atof(argv[4]);
  gauge::Rect roi{std::atoi(argv[5]), std::atoi(argv[6]), std::atoi(argv[7]), std::atoi(argv[8])};
  const int n = std::atoi(argv[9]);
  for (int i = 0; i < n && 10 + 3 * i + 2 < argc; ++i) {
    s.points.push_back({static_cast<float>(std::atof(argv[10 + 3 * i])),
                        static_cast<float>(std::atof(argv[11 + 3 * i])),
                        static_cast<float>(std::atof(argv[12 + 3 * i]))});
  }
  std::vector<uint8_t> bright;
  gauge::brightnessFromRgb(rgb.data(), w, h, gauge::Rect{}, bright);
  const gauge::Result r = gauge::readRoi(gauge::Image{w, h, bright.data()}, roi, s);
  std::printf("{\"ok\":%s,\"error\":\"%s\",\"value\":%.3f,\"cx\":%.1f,\"cy\":%.1f,"
              "\"hub_radius\":%d,\"r_start\":%.1f,\"r_end\":%.1f,\"needle_angle\":%.2f,"
              "\"contrast\":%.1f,\"scale_ok\":%s,\"scale_start\":%.2f,\"scale_direction\":%d,"
              "\"scale_span\":%.2f}\n",
              r.ok ? "true" : "false", r.error.c_str(), r.value, r.cx, r.cy, r.hub_radius,
              r.r_start, r.r_end, r.needle_angle, r.contrast,
              r.scale.offsets.empty() ? "false" : "true", r.scale.start, r.scale.direction,
              r.scale.span());
  return 0;
}
