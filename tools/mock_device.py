#!/usr/bin/env python3
"""Simulated gauge-cam device to work on the web interface without hardware.

Serves web/ and the device API. Photos show the drawn oil gauge of
tools/oil_dial.py at 100 cm (placed in a 640x480 frame, like a VGA capture) and readings run the real
C++ code through build/gauge_cli.

    test/host/run.sh            # builds build/gauge_cli too
    python3 tools/mock_device.py [port [address]]

POST /mock/level?v=40 redraws the dial at another level (to try the jump
filter and the list of refused readings). The update check always finds a
release v0.2.0; installing it takes a few seconds.
"""
import copy
import io
import json
import os
import subprocess
import sys
import math
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from PIL import Image, ImageEnhance

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from oil_dial import draw_oil_dial  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")
CLI = os.path.join(ROOT, "build", "gauge_cli")
TYPES = {".html": "text/html", ".js": "application/javascript", ".css": "text/css", ".woff2": "font/woff2"}
SECRETS = ("wifi_pass", "mqtt_pass", "ui_password", "ap_password")

CONFIG = {
    "name": "Gauge Cam", "lang": "fr", "wifi_ssid": "maison", "mqtt_host": "", "mqtt_port": 1883,
    "mqtt_user": "", "mqtt_prefix": "gauge-cam", "discovery_prefix": "homeassistant",
    "ha_discovery": True, "ntp_server": "pool.ntp.org", "update_check": True,
    "wifi_pass": "secret", "mqtt_pass": "", "ui_password": "", "ap_password": "",
    "samples": 3, "max_jump": 5,
    "interval_s": 3600, "framesize": "VGA", "vflip": False, "hmirror": False, "rotation": 0,
    "led_intensity": 128, "led_ms": 400, "cam_brightness": 0, "cam_contrast": 0,
    "exposure_auto": True, "exposure": 300, "gain": 0,
    "jpeg_quality": 10, "roi": [0, 0, 0, 0], "pivot_mode": "hub", "pivot_x": -1, "pivot_y": -1,
    "points": [], "unit": "cm", "min_contrast": 20, "tank_shape": "box", "tank_length_cm": 0,
    "tank_width_cm": 0, "tank_height_cm": 0, "tank_diameter_cm": 0, "tank_table": [], "level_offset_cm": 0,
}
FRAME = {"QVGA": (320, 240), "VGA": (640, 480), "SVGA": (800, 600)}
CONFIRMATIONS = 2
FILTER = {"ref": None, "candidate": None, "pending": 0}  # same logic as gauge::JumpFilter
FAILURES = []  # newest first, at most 4
UPDATE = {"installed": "v0.1.0", "latest": "", "checked": 0, "busy": False, "progress": None,
          "error": "", "restart_note": ""}
LOG = []
STATE = {"photo": None, "photo_size": (0, 0), "photo_at": 0, "failure_id": 0, "attempt": None, "good": None, "boot": time.time()}
DIAL = draw_oil_dial(100)[0]
TMP = tempfile.mkdtemp(prefix="gauge-cam-mock-")
PPM = os.path.join(TMP, "photo.ppm")


def log(msg):
    LOG.append({"t": int(time.time() - STATE["boot"]), "at": int(time.time()), "msg": msg})
    del LOG[:-80]


def tank_volume(cfg, level):
    """Same formulas as gauge::Tank; returns (volume, percent), None when unknown."""
    shape, table = cfg["tank_shape"], cfg["tank_table"]
    length, width, height, diam = (cfg["tank_length_cm"], cfg["tank_width_cm"], cfg["tank_height_cm"],
                                   cfg["tank_diameter_cm"])
    ready = {"box": length > 0 and width > 0, "hcyl": length > 0 and diam > 0, "vcyl": diam > 0,
             "table": len(table) >= 2}[shape]
    full = {"box": height, "hcyl": diam, "vcyl": height, "table": table[-1][0] if table else 0}[shape]

    def volume(lv):
        if full > 0:
            lv = min(lv, full)
        lv = max(lv, table[0][0] if shape == "table" else 0)
        if shape == "box":
            return lv * length * width / 1000
        if shape == "vcyl":
            return lv * math.pi * diam * diam / 4 / 1000
        if shape == "hcyl":
            r = diam / 2
            return (r * r * math.acos((r - lv) / r) - (r - lv) * math.sqrt(max(0, 2 * r * lv - lv * lv))) * length / 1000
        for (l0, v0), (l1, v1) in zip(table, table[1:]):
            if lv <= l1:
                return v0 + (v1 - v0) * (lv - l0) / (l1 - l0)
        return table[-1][1]

    vol = volume(level) if ready else None
    cap = volume(full) if ready and full > 0 else 0
    pct = 100 * vol / cap if cap > 0 else (100 * level / full if full > 0 else None)
    return vol, pct


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
    if not cfg["exposure_auto"]:  # fixed exposure: brightness follows the settings
        light *= cfg["exposure"] / 300 * (1 + cfg["gain"] / 8)
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
        out["volume_l"], out["percent"] = tank_volume(cfg, level)
    return out


def jump_filter(r):
    """Holds back a jump until two more readings agree (see main.cpp)."""
    if not r["ok"]:
        return
    level, max_jump, f = r["level"], CONFIG["max_jump"], FILTER
    if max_jump <= 0 or f["ref"] is None or abs(level - f["ref"]) <= max_jump:
        f.update(ref=level, pending=0)
        return
    if f["pending"] and abs(level - f["candidate"]) <= max_jump / 2:
        f["pending"] += 1
    else:
        f["pending"] = 1
    f["candidate"] = level
    if f["pending"] > CONFIRMATIONS:
        f.update(ref=level, pending=0)
        return
    r.update(ok=False, suspect=True, error=(
        f"implausible jump from {f['ref']:.1f} to {level:.1f} {CONFIG['unit']}, "
        f"checking again ({f['pending']}/{CONFIRMATIONS + 1})"))


def keep_failure(r):
    STATE["failure_id"] += 1
    FAILURES.insert(0, {"id": STATE["failure_id"], "reading": r, "photo": STATE["photo"]})
    del FAILURES[4:]


def reading_json(r, cfg):
    """Same shape as readingToJson() in the firmware."""
    if not r:
        return {"ok": False, "age_s": -1, "level": None, "unit": cfg["unit"], "volume_l": None, "percent": None}
    out = {"ok": r["ok"], "age_s": int(time.time() - r["at"]), "at": int(r["at"]), "unit": cfg["unit"],
           "level": round(r["level"], 1) if r.get("level") is not None else None,
           "volume_l": round(r["volume_l"]) if r.get("volume_l") is not None else None,
           "percent": round(r["percent"], 1) if r.get("percent") is not None else None}
    if r.get("error") and not r["ok"]:
        out["error"] = r["error"]
    if r.get("suspect"):
        out["suspect"] = True
    if r.get("scale_ok"):
        d = {k: r[k] for k in ("cx", "cy", "hub_radius", "r_start", "r_end", "scale_start",
                               "scale_direction", "scale_span")}
        if r.get("gauge_ok"):
            d.update(needle_angle=round(r["needle_angle"], 1), contrast=round(r["contrast"]),
                     gauge_value=round(r["value"], 2))
        out["detail"] = d
    return out


def install():
    for p in range(0, 101, 10):
        UPDATE["progress"] = p
        time.sleep(0.4)
    log(f"Restarting: update to {UPDATE['latest']} installed")
    UPDATE.update(installed=UPDATE["latest"], busy=False, progress=None,
                  restart_note=f"update to {UPDATE['latest']} installed")
    STATE["boot"] = time.time()
    LOG.clear()
    log(f"gauge-cam {UPDATE['installed']} (gauge-cam-mock01), started by: software")


def update_json():
    available = bool(UPDATE["latest"]) and UPDATE["latest"] != UPDATE["installed"]
    out = {"installed": UPDATE["installed"], "latest": UPDATE["latest"], "available": available,
           "checked_s": int(time.time() - UPDATE["checked"]) if UPDATE["checked"] else -1,
           "busy": UPDATE["busy"], "release_url": "https://github.com/tdebuilt/gauge-cam/releases/latest",
           "pending_verify": False}
    if UPDATE["progress"] is not None:
        out["progress"] = UPDATE["progress"]
    return out


def public_config():
    out = {k: v for k, v in CONFIG.items() if k not in SECRETS}
    for k in SECRETS:
        out[k + "_set"] = bool(CONFIG[k])
    return out


def apply_config(body):
    nxt = copy.deepcopy(CONFIG)
    moved = any(k in body and body[k] != CONFIG[k] for k in ("mqtt_host", "mqtt_port", "mqtt_user"))
    if moved and CONFIG["mqtt_pass"] and not body.get("mqtt_pass") and not body.get("mqtt_pass_clear"):
        raise ValueError("type the MQTT password again to change the server, port or user")
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
    if not 1 <= nxt["samples"] <= 5:
        raise ValueError("samples must be between 1 and 5")
    if nxt["max_jump"] < 0:
        raise ValueError("max_jump must be 0 (off) or more")
    if nxt["ap_password"] and not 8 <= len(nxt["ap_password"]) <= 63:
        raise ValueError("the access point password needs 8 to 63 characters")
    if nxt["rotation"] not in (0, 90, 180, 270):
        raise ValueError("rotation must be 0, 90, 180 or 270")
    if nxt["framesize"] not in FRAME:
        raise ValueError("framesize must be QVGA, VGA or SVGA")
    if not 0 <= nxt["exposure"] <= 1200 or not 0 <= nxt["gain"] <= 30:
        raise ValueError("exposure must be 0 to 1200 and gain 0 to 30")
    if nxt["tank_shape"] not in ("box", "hcyl", "vcyl", "table"):
        raise ValueError("tank_shape must be box, hcyl, vcyl or table")
    table = nxt["tank_table"]
    if len(table) > 40 or any(len(r) != 2 for r in table) or any(
            b[0] <= a[0] or b[1] < a[1] for a, b in zip(table, table[1:])):
        raise ValueError("tank table: at most 40 rows, levels increasing, litres never decreasing")
    restart = nxt["wifi_ssid"] != CONFIG["wifi_ssid"] or nxt["wifi_pass"] != CONFIG["wifi_pass"]
    CONFIG.clear()
    CONFIG.update(nxt)
    FILTER.update(ref=None, pending=0)
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
                "name": CONFIG["name"], "id": "gauge-cam-mock01", "version": UPDATE["installed"],
                "uptime_s": int(time.time() - STATE["boot"]), "heap_free": 180000, "heap_min": 121000,
                "psram_free": 3200000, "time": int(time.time()),
                "reset_reason": "software" if UPDATE["restart_note"] else "poweron",
                **({"restart_note": UPDATE["restart_note"]} if UPDATE["restart_note"] else {}),
                "update": update_json(),
                "wifi": {"connected": True, "ssid": CONFIG["wifi_ssid"], "ip": "127.0.0.1", "rssi": -61,
                         "hostname": "gauge-cam-mock01", "ap": False},
                "mqtt": {"configured": bool(CONFIG["mqtt_host"]), "connected": bool(CONFIG["mqtt_host"])},
                **({"photo": {"width": STATE["photo_size"][0], "height": STATE["photo_size"][1],
                            "taken_ms": int(STATE["photo_at"] * 1000), "bytes": len(STATE["photo"]), "age_s": age}}
                   if STATE["photo"] else {}),
                "dial_ready": CONFIG["pivot_x"] >= 0 and len(CONFIG["points"]) >= 2,
                "last_failure": STATE["failure_id"],
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
        if path == "/api/failures":
            return self.send(200, [{"id": f["id"], "photo": bool(f["photo"]), **reading_json(f["reading"], CONFIG)}
                                   for f in FAILURES])
        if path == "/api/failure.jpg":
            wanted = self.path.split("id=")[-1]
            for f in FAILURES:
                if str(f["id"]) == wanted and f["photo"]:
                    return self.send(200, f["photo"], "image/jpeg")
            return self.send(404, {"error": "no such photo"})
        if path == "/api/log":
            return self.send(200, LOG)
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
                jump_filter(r)
                STATE["attempt"] = r
                log(f"Reading: {r['level']:.1f} {CONFIG['unit']}" if r["ok"] else f"Reading refused: {r['error']}")
                if r["ok"]:
                    STATE["good"] = r
                elif CONFIG["pivot_x"] >= 0 and len(CONFIG["points"]) >= 2:
                    keep_failure(r)
                return self.send(200, reading_json(r, CONFIG))
            if path == "/api/test":
                draft = copy.deepcopy(CONFIG)
                draft.update({k: v for k, v in self.body().items() if k in draft})
                return self.send(200, reading_json(read(draft), draft))
            if path == "/mock/level":
                global DIAL
                DIAL = draw_oil_dial(float(self.path.split("v=")[-1]))[0]
                return self.send(200, {"ok": True})
            if path == "/api/update/check":
                UPDATE.update(latest="v0.2.0", checked=time.time())
                log("Latest release: v0.2.0" + (" (update available)" if UPDATE["installed"] != "v0.2.0" else ""))
                return self.send(200, {"checking": True})
            if path == "/api/update/install":
                if UPDATE["busy"]:
                    return self.send(503, {"error": "firmware update in progress"})
                if not UPDATE["latest"] or UPDATE["latest"] == UPDATE["installed"]:
                    return self.send(500, {"error": "no newer release to install"})
                UPDATE.update(busy=True, progress=0)
                log("Update requested from the interface")
                threading.Thread(target=install, daemon=True).start()
                return self.send(200, {"installing": True})
            if path == "/api/restart":
                return self.send(200, {"restart": True})
            if path == "/api/update":
                return self.send(500, {"error": "not supported by the mock"})
        except (ValueError, subprocess.CalledProcessError) as err:
            return self.send(400, {"error": str(err)})
        self.send(404, {"error": "not found"})


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    host = sys.argv[2] if len(sys.argv) > 2 else "127.0.0.1"  # e.g. a LAN address for a demo
    log("gauge-cam v0.1.0 (gauge-cam-mock01), started by: poweron")
    log("Wi-Fi connected to maison (-61 dBm): http://127.0.0.1/")
    capture(CONFIG)
    print(f"mock gauge-cam on http://{host}:{port}/")
    ThreadingHTTPServer((host, port), Handler).serve_forever()
