---
name: gauge-cam
description: Read, check and tune a gauge-cam device (an ESP32-CAM that reads an analog tank gauge from photos) through its HTTP API. Use when asked for the tank level, why a reading failed or looks wrong, to take a photo of the gauge, or to adjust the dial settings (needle centre, graduations, camera, tank).
---

# gauge-cam

The device has an HTTP API that describes itself. Two environment variables tell where it is:

- `GAUGE_CAM_URL`: for example `http://192.168.1.50` (no trailing slash)
- `GAUGE_CAM_PASSWORD`: the interface password, user `admin`; empty when none is set

Never put the password on a command line or in your answer. Call the API like this:

```sh
gc() {  # gc METHOD PATH [JSON]
  printf 'user = "admin:%s"\n' "$GAUGE_CAM_PASSWORD" |
    curl -sS --config - -X "$1" -H 'Content-Type: application/json' ${3:+-d "$3"} "$GAUGE_CAM_URL$2"
}
gc GET /api/openapi.json   # every route and setting, with units and limits
```

## Rules

1. **Look before acting**: `GET /api/status` (last good reading in `good`, last try in `attempt`),
   `GET /api/config`, `GET /api/log`, `GET /api/failures`.
2. **Photos**: `GET /api/photo.jpg` is the photo of the last reading. `POST /api/capture` takes a new
   one first; it switches the LED on for a moment. Coordinates (`roi`, `pivot_x`, `pivot_y`,
   `points`) are pixels of that photo.
3. **Try before saving**: `POST /api/test` with draft settings reads the last photo and stores
   nothing. Compare its `level` and `detail.needle_angle` with what you see on the photo.
4. **Save only with the user's approval**: show the reading before and after, then
   `POST /api/config` with the changed keys only. The answer lists `changes`.
5. **Check**: `POST /api/read` takes new photos and reads them.
6. **Worse after all**: `GET /api/config/undo` shows what an undo changes, `POST /api/config/undo`
   brings back the previous settings.
7. **Never send** `wifi_ssid`, `wifi_pass`, `ap_password`, `ui_password` or the MQTT settings
   unless the user asks: a wrong Wi-Fi setting cuts the device off.
8. `POST /api/update/install`, `POST /api/update` and `POST /api/restart` restart the device: ask
   first.

## Reading the answers

- A reading: `ok`, `level` in `unit`, `volume_l`, `percent`, `error` when it failed. `suspect: true`
  means a jump that the next readings must confirm (a refill passes after 3 readings).
- `detail`: `cx`, `cy` (needle centre found), `needle_angle`, `contrast`. A low contrast or a
  centre far from the hub on the photo explains a wrong reading.
- `GET /api/log`: `Settings changed: <keys>` lines tell when the settings changed; lines with
  `previous: true` come from before the last restart.

## Tuning the dial

- **Needle centre**: `pivot_mode` `hub` finds it on each photo; `fixed` uses `pivot_x`, `pivot_y`.
  Try both with `/api/test`.
- **Graduations**: `points` is a list of `[x, y, value]` in dial order, at least the start and the
  end; more points where the scale is uneven.
- **Camera**: try `led_intensity`, `exposure_auto`, `exposure`, `gain` with `POST /api/capture` and
  a JSON body, then look at the photo.

Full reference: `docs/api.md` and `docs/agents.md` of https://github.com/tdebuilt/gauge-cam.
