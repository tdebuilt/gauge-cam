#include "camera.h"

#include <esp_camera.h>
#include <img_converters.h>

#include "gauge.h"

namespace {

// AI-Thinker ESP32-CAM pin map.
constexpr int kPwdn = 32, kReset = -1, kXclk = 0, kSiod = 26, kSioc = 27;
constexpr int kY9 = 35, kY8 = 34, kY7 = 39, kY6 = 36, kY5 = 21, kY4 = 19,
              kY3 = 18, kY2 = 5, kVsync = 25, kHref = 23, kPclk = 22;
constexpr int kLedPin = 4;
constexpr int kLedChannel = 7;  // LEDC channel 0 drives the camera clock

Photo photo;
size_t photo_capacity = 0;
int failures_in_a_row = 0;  // frames the sensor did not deliver

framesize_t frameSize(const String& name) {
  if (name == "QVGA") return FRAMESIZE_QVGA;
  if (name == "SVGA") return FRAMESIZE_SVGA;
  return FRAMESIZE_VGA;
}

// Copies a JPEG into the photo buffer, growing it when needed.
bool keepJpeg(const uint8_t* data, size_t size, int width, int height, String& error) {
  if (size > photo_capacity) {
    free(photo.jpeg);
    photo.jpeg = static_cast<uint8_t*>(ps_malloc(size));
    photo_capacity = photo.jpeg ? size : 0;
  }
  if (!photo.jpeg) {
    photo.size = 0;
    error = "out of memory for the photo";
    return false;
  }
  memcpy(photo.jpeg, data, size);
  photo.size = size;
  photo.width = width;
  photo.height = height;
  photo.taken_ms = millis();
  return true;
}

// fmt2jpg_cb output callback: appends to the photo buffer, grown in PSRAM.
size_t appendJpeg(void* overflow, size_t index, const void* data, size_t len) {
  if (!len) return 0;  // end of stream
  if (index + len > photo_capacity) {
    const size_t capacity = (index + len) * 3 / 2 + 16384;
    uint8_t* grown = static_cast<uint8_t*>(ps_realloc(photo.jpeg, capacity));
    if (!grown) {
      *static_cast<bool*>(overflow) = true;
      return 0;
    }
    photo.jpeg = grown;
    photo_capacity = capacity;
  }
  memcpy(photo.jpeg + index, data, len);
  photo.size = index + len;
  return len;
}

// Decodes the frame, rotates it and encodes it again: the web interface, the
// reading and Home Assistant then all see the dial upright.
bool keepRotated(camera_fb_t* fb, const Config& cfg, String& error) {
  const int w = fb->width, h = fb->height;
  const size_t bytes = static_cast<size_t>(w) * h * 3;
  uint8_t* rgb = static_cast<uint8_t*>(ps_malloc(bytes));
  uint8_t* turned = static_cast<uint8_t*>(ps_malloc(bytes));
  bool ok = rgb && turned && fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);
  esp_camera_fb_return(fb);
  if (!ok) {
    error = rgb && turned ? "cannot decode the photo" : "out of memory to rotate the photo";
  } else {
    gauge::rotateRgb(rgb, w, h, cfg.rotation, turned);
    free(rgb);
    rgb = nullptr;
    const int out_w = cfg.rotation % 180 ? h : w, out_h = cfg.rotation % 180 ? w : h;
    // Sensor quality: 4 (best) .. 63; encoder quality: 100 (best) .. 0.
    const uint8_t quality = constrain(95 - cfg.jpeg_quality, 30, 95);
    // Encoded straight into the photo buffer (fmt2jpg caps its output at 128 KB).
    photo.size = 0;
    bool overflow = false;
    ok = fmt2jpg_cb(turned, bytes, out_w, out_h, PIXFORMAT_RGB888, quality, appendJpeg, &overflow) &&
         !overflow;
    if (ok) {
      photo.width = out_w;
      photo.height = out_h;
      photo.taken_ms = millis();
    } else {
      photo.size = 0;
      error = overflow ? "out of memory for the photo" : "cannot encode the rotated photo";
    }
  }
  free(rgb);
  free(turned);
  return ok;
}

}  // namespace

void ledSet(uint8_t intensity) { ledcWrite(kLedChannel, intensity); }

bool cameraBegin(const Config& cfg) {
  ledcSetup(kLedChannel, 5000, 8);
  ledcAttachPin(kLedPin, kLedChannel);
  ledSet(0);

  camera_config_t c = {};
  c.pin_pwdn = kPwdn;
  c.pin_reset = kReset;
  c.pin_xclk = kXclk;
  c.pin_sccb_sda = kSiod;
  c.pin_sccb_scl = kSioc;
  c.pin_d7 = kY9;
  c.pin_d6 = kY8;
  c.pin_d5 = kY7;
  c.pin_d4 = kY6;
  c.pin_d3 = kY5;
  c.pin_d2 = kY4;
  c.pin_d1 = kY3;
  c.pin_d0 = kY2;
  c.pin_vsync = kVsync;
  c.pin_href = kHref;
  c.pin_pclk = kPclk;
  c.xclk_freq_hz = 20000000;
  c.ledc_timer = LEDC_TIMER_0;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_JPEG;
  // Allocate for the largest size offered so it can change without reboot.
  c.frame_size = FRAMESIZE_SVGA;
  c.jpeg_quality = cfg.jpeg_quality;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;  // one buffer: stale frames are dropped by hand
  if (esp_camera_init(&c) != ESP_OK) return false;
  cameraApply(cfg);
  return true;
}

void cameraApply(const Config& cfg) {
  sensor_t* s = esp_camera_sensor_get();
  if (!s) return;
  // Changing the frame size resets the sensor window: only when it differs.
  if (s->status.framesize != frameSize(cfg.framesize)) s->set_framesize(s, frameSize(cfg.framesize));
  s->set_quality(s, cfg.jpeg_quality);
  s->set_vflip(s, cfg.vflip);
  s->set_hmirror(s, cfg.hmirror);
  s->set_brightness(s, cfg.cam_brightness);
  s->set_contrast(s, cfg.cam_contrast);
  s->set_exposure_ctrl(s, cfg.exposure_auto);
  s->set_gain_ctrl(s, cfg.exposure_auto);
  if (!cfg.exposure_auto) {
    s->set_aec_value(s, cfg.exposure);
    s->set_agc_gain(s, cfg.gain);
  }
}

bool cameraRetry(const Config& cfg) {
  esp_camera_deinit();
  return cameraBegin(cfg);
}

int cameraFailuresInARow() { return failures_in_a_row; }

bool cameraCapture(const Config& cfg, String& error) {
  if (cfg.led_intensity) {
    ledSet(cfg.led_intensity);
    delay(cfg.led_ms);
  }
  // Drop a few frames so exposure and white balance adapt to the LED.
  for (int i = 0; i < 2; ++i) {
    camera_fb_t* stale = esp_camera_fb_get();
    if (stale) esp_camera_fb_return(stale);
  }
  camera_fb_t* fb = esp_camera_fb_get();
  ledSet(0);
  if (!fb) {
    ++failures_in_a_row;
    error = "camera capture failed";
    return false;
  }
  failures_in_a_row = 0;
  if (cfg.rotation) return keepRotated(fb, cfg, error);
  const bool ok = keepJpeg(fb->buf, fb->len, fb->width, fb->height, error);
  esp_camera_fb_return(fb);
  return ok;
}

const Photo& lastPhoto() { return photo; }
