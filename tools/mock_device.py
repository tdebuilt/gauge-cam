#!/usr/bin/env python3
"""Simulated gauge-cam device to work on the web interface without hardware.

Serves web/ and the device API. Photos show the drawn oil gauge of
tools/oil_dial.py at 100 cm (placed in a 640x480 frame, like a VGA capture) and readings run the real
C++ code through build/gauge_cli.

    test/host/run.sh            # builds build/gauge_cli too
    python3 tools/mock_device.py [port]
"""
import copy
import io
import json
import os
import subprocess
import sys
import tempfile
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from PIL import Image, ImageEnhance

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from oil_dial import draw_oil_dial  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")
CLI = os.path.join(ROOT, "build", "gauge_cli")
TYPES = {".html": "text/html", ".js": "application/javascript", ".css": "text/css"}
SECRETS = ("wifi_pass", "mqtt_pass", "ui_password")

CONFIG = {
    "name": "Gauge Cam", "lang": "fr", "wifi_ssid": "maison", "mqtt_host": "", "mqtt_port": 1883,
    "mqtt_user": "", "mqtt_prefix": "gauge-cam", "discovery_prefix": "homeassistant",
    "wifi_pass": "secret", "mqtt_pass": "", "ui_password": "",
    "interval_s": 3600, "framesize": "VGA", "vflip": False, "hmirror": False, "rotation": 0,
    "led_intensity": 128, "led_ms": 400, "cam_brightness": 0, "cam_contrast": 0,
    "jpeg_quality": 10, "roi": [0, 0, 0, 0], "pivot_mode": "hub", "pivot_x": -1, "pivot_y": -1,
    "points": [], "unit": "cm", "min_contrast": 20, "tank_length_cm": 0, "tank_width_cm": 0,
    "tank_height_cm": 0, "level_offset_cm": 0,
}
FRAME = {"QVGA": (320, 240), "VGA": (640, 480), "SVGA": (800, 600)}
STATE = {"photo": None, "photo_size": (0, 0), "photo_at": 0, "attempt": None, "good": None, "boot": time.time()}
DIAL = draw_oil_dial(100)[0]
TMP = tempfile.mkdtemp(prefix="gauge-cam-mock-")
PPM = os.path.join(TMP, "photo.ppm")


def capture(cfg):
    """A frame with the dial at about 70 % of the height, shot like the device:
    LED and exposure, sensor flips, then clockwise rotation."""
    dial = DIAL
    scale = 340 / dial.height
    dial = dial.resize((round(dial.width * scale), 340))
    frame = Image.new("RGB", (640, 480), (64, 60, 56))
    frame.paste(dial, (130, 70))
    frame = frame.resize(FRAME[cfg["framesize"]])
    light = 0.35 + 0.65 * cfg["led_intensity"] / 255 + 0.12 * cfg["cam_brightness"]
    frame = ImageEnhance.Brightness(frame).enhance(max(0.05, light))
    frame = ImageEnhance.Contrast(frame).enhance(1 + 0.2 * cfg["cam_contrast"])
    if cfg["hmirror"]:
        frame = frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    if cfg["vflip"]:
        frame = frame.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    if cfg["rotation"]:
        frame = frame.rotate(-cfg["rotation"], expand=True)  # PIL turns counter-clockwise
    frame.save(PPM)
    STATE["photo_size"] = frame.size
    buf = io.BytesIO()
    frame.save(buf, "JPEG", quality=88)
    STATE["photo"], STATE["photo_at"] = buf.getvalue(), time.time()


def read(cfg):
    if not STATE["photo"]:
        return {"ok": False, "error": "no photo yet", "at": time.time()}
    if cfg["pivot_x"] < 0 or len(cfg["points"]) < 2:
        return {"ok": False, "error": "dial not configured: set the pivot, the start and the end",
                "at": time.time()}
    args = [CLI, PPM, cfg["pivot_mode"], str(cfg["pivot_x"]), str(cfg["pivot_y"]),
            *map(str, cfg["roi"]), str(len(cfg["points"]))]
    for p in cfg["points"]:
        args += map(str, p)
    out = json.loads(subprocess.check_output(args))
    out["at"] = time.time()
    out["gauge_ok"] = out["ok"]
    if out["ok"] and out["contrast"] < cfg["min_contrast"]:
        out["ok"] = False
        out["error"] = f"needle contrast too low ({out['contrast']:.0f} < {cfg['min_contrast']:.0f}): check the light"
    if out["ok"]:
        level = out["value"] + cfg["level_offset_cm"]
        out["level"] = level
        if cfg["tank_length_cm"] > 0 and cfg["tank_width_cm"] > 0:
            out["volume_l"] = level * cfg["tank_length_cm"] * cfg["tank_width_cm"] / 1000
        if cfg["tank_height_cm"] > 0:
            out["percent"] = 100 * level / cfg["tank_height_cm"]
    return out


def reading_json(r, cfg):
    """Same shape as readingToJson() in the firmware."""
    if not r:
        return {"ok": False, "age_s": -1, "level": None, "unit": cfg["unit"], "volume_l": None, "percent": None}
    out = {"ok": r["ok"], "age_s": int(time.time() - r["at"]), "unit": cfg["unit"],
           "level": round(r["level"], 1) if r.get("level") is not None else None,
           "volume_l": round(r["volume_l"]) if r.get("volume_l") is not None else None,
           "percent": round(r["percent"], 1) if r.get("percent") is not None else None}
    if r.get("error") and not r["ok"]:
        out["error"] = r["error"]
    if r.get("scale_ok"):
        d = {k: r[k] for k in ("cx", "cy", "hub_radius", "r_start", "r_end", "scale_start",
                               "scale_direction", "scale_span")}
        if r.get("gauge_ok"):
            d.update(needle_angle=round(r["needle_angle"], 1), contrast=round(r["contrast"]),
                     gauge_value=round(r["value"], 2))
        out["detail"] = d
    return out


def public_config():
    out = {k: v for k, v in CONFIG.items() if k not in SECRETS}
    for k in SECRETS:
        out[k + "_set"] = bool(CONFIG[k])
    return out


def apply_config(body):
    nxt = copy.deepcopy(CONFIG)
    for key, value in body.items():
        if key.endswith("_clear") and key[:-6] in SECRETS:
            if value:
                nxt[key[:-6]] = ""
        elif key in SECRETS:
            if value:
                nxt[key] = value
        elif key in nxt:
            nxt[key] = value
    if nxt["pivot_mode"] not in ("hub", "fixed"):
        raise ValueError("pivot_mode must be hub or fixed")
    if nxt["lang"] not in ("fr", "en"):
        raise ValueError("lang must be fr or en")
    if nxt["interval_s"] < 60:
        raise ValueError("interval_s must be at least 60")
    if nxt["rotation"] not in (0, 90, 180, 270):
        raise ValueError("rotation must be 0, 90, 180 or 270")
    if nxt["framesize"] not in FRAME:
        raise ValueError("framesize must be QVGA, VGA or SVGA")
    restart = nxt["wifi_ssid"] != CONFIG["wifi_ssid"] or nxt["wifi_pass"] != CONFIG["wifi_pass"]
    CONFIG.clear()
    CONFIG.update(nxt)
    return restart


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n) if n else b""
        return json.loads(raw) if raw else {}

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/":
            path = "/index.html"
        if path == "/api/status":
            age = int(time.time() - STATE["photo_at"]) if STATE["photo"] else None
            return self.send(200, {
                "name": CONFIG["name"], "id": "gauge-cam-mock01", "version": "mock",
                "uptime_s": int(time.time() - STATE["boot"]), "heap_free": 180000, "psram_free": 3200000,
                "wifi": {"connected": True, "ssid": CONFIG["wifi_ssid"], "ip": "127.0.0.1", "rssi": -61,
                         "hostname": "gauge-cam-mock01", "ap": False},
                "mqtt": {"configured": bool(CONFIG["mqtt_host"]), "connected": bool(CONFIG["mqtt_host"])},
                **({"photo": {"width": STATE["photo_size"][0], "height": STATE["photo_size"][1], "bytes": len(STATE["photo"]), "age_s": age}}
                   if STATE["photo"] else {}),
                "dial_ready": CONFIG["pivot_x"] >= 0 and len(CONFIG["points"]) >= 2,
                "next_read_s": 1800,
                "attempt": reading_json(STATE["attempt"], CONFIG),
                "good": reading_json(STATE["good"], CONFIG),
            })
        if path == "/api/config":
            return self.send(200, public_config())
        if path == "/api/photo.jpg":
            if not STATE["photo"]:
                return self.send(404, {"error": "no photo yet"})
            return self.send(200, STATE["photo"], "image/jpeg")
        if path == "/api/wifi/scan":
            return self.send(200, [{"ssid": "maison", "rssi": -58, "open": False},
                                   {"ssid": "maison-iot", "rssi": -63, "open": False}])
        file = os.path.join(WEB, path.lstrip("/"))
        if os.path.isfile(file) and os.path.splitext(file)[1] in TYPES:
            return self.send(200, open(file, "rb").read(), TYPES[os.path.splitext(file)[1]])
        self.send(404, {"error": "not found"})

    def do_POST(self):
        path = self.path.split("?")[0]
        try:
            if path == "/api/config":
                restart = apply_config(self.body())
                return self.send(200, {"saved": True, "restart": restart})
            if path == "/api/capture":
                draft = copy.deepcopy(CONFIG)
                draft.update({k: v for k, v in self.body().items() if k in draft})
                if draft["rotation"] not in (0, 90, 180, 270) or draft["framesize"] not in FRAME:
                    raise ValueError("invalid capture settings")
                capture(draft)
                w, h = STATE["photo_size"]
                return self.send(200, {"width": w, "height": h})
            if path == "/api/read":
                if "photo=0" not in self.path:
                    capture(CONFIG)
                r = read(CONFIG)
                STATE["attempt"] = r
                if r["ok"]:
                    STATE["good"] = r
                return self.send(200, reading_json(r, CONFIG))
            if path == "/api/test":
                draft = copy.deepcopy(CONFIG)
                draft.update({k: v for k, v in self.body().items() if k in draft})
                return self.send(200, reading_json(read(draft), draft))
            if path == "/api/restart":
                return self.send(200, {"restart": True})
            if path == "/api/update":
                return self.send(500, {"error": "not supported by the mock"})
        except (ValueError, subprocess.CalledProcessError) as err:
            return self.send(400, {"error": str(err)})
        self.send(404, {"error": "not found"})


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    capture(CONFIG)
    print(f"mock gauge-cam on http://127.0.0.1:{port}/")
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
