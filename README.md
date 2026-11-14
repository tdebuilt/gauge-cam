# Gauge Cam

[Français](README.fr.md)

ESP32-CAM firmware that reads a needle gauge, such as an oil tank level indicator, and publishes
the level, volume and fill percentage to Home Assistant over MQTT. Everything is set up from the
browser, in the spirit of AI-on-the-edge: flashing, Wi-Fi, dial, tank.

> Status: algorithm checked on a photo of a real Afriso dial; repository tests, web interface and
> Home Assistant integration run on a computer with a drawn tank gauge (`tools/oil_dial.py`);
> **not yet tried on a real ESP32-CAM**.

## Features

- **Reading on the ESP32**, no neural network: the needle centre is the dark hub (tracked from one
  photo to the next) or a fixed pixel; the needle is the darkest ray between the hub and the
  graduations; the value is interpolated between neighbouring calibration points, which handles
  non-linear scales. Coloured marks (red zone) are not mistaken for the needle, and their values
  are read.
- **Tank volume** for a box, a horizontal or a vertical cylinder, or any other shape through a
  level → litres gauging table; the fill percentage is the volume over the capacity.
- **Plausibility filter**: each reading takes 3 photos and keeps the median; a level that jumps
  further than 5 cm (adjustable) from the last one is checked again 2 minutes later and accepted
  only once the next 2 readings agree, so a refill gets through and a stray reflection does not.
  The last 4 refused readings stay visible with their photo in the Level tab.
- **Flash LED** lit before each photo; a reading whose needle contrast is too low (dark photo) is
  rejected and the last good value is kept. Exposure and gain can be fixed so that every photo is
  the same.
- **Capture settings next to the photo**, as in AI-on-the-edge: LED intensity, rotation (0, 90,
  180, 270°, for a camera mounted sideways), flips, resolution, brightness and contrast. Each change
  takes a new photo without saving anything; the frame, centre and graduations follow the rotation.
  The photo is rotated on the ESP32, so Home Assistant gets it upright too.
- **Home Assistant MQTT discovery** (can be turned off, entities are then removed): level, volume,
  fill, time of the last reading, photo, "read now" button, firmware update, and as diagnostics the
  reading status, needle angle, contrast, Wi-Fi signal, last start and its cause, free memory.
  Announced again whenever Home Assistant restarts. Level, volume and fill turn unavailable after
  three missed readings instead of showing an old value.
- **Updates from Home Assistant** or from Settings → System: the device looks for a new GitHub release
  every day and installs it over HTTPS (certificates checked). A new firmware that crashes, or cannot
  join Wi-Fi within 5 minutes, is replaced by the previous one at the next restart.
- **Runs unattended**: a task watchdog restarts a stuck program; the camera is started again, then
  the device restarts (at most 3 times since power on) when it fails; the device also restarts after
  1 hour without Wi-Fi, or without the MQTT server it had reached. Settings → System keeps a log of the
  last 80 events and shows why the device last restarted (crash, watchdog, brown-out…).
- **Clock** from an NTP server (`pool.ntp.org` by default, or the gateway): readings, refused
  readings and the log are dated.
- **Web interface** in French and English (more languages: `web/i18n.js`), firmware update,
  configuration backup and restore. Passwords are never sent back, and the stored MQTT password is
  only used with the server it was typed for: changing the server, port or user needs it again.
- **Browser flashing** (Chrome or Edge) with Wi-Fi set-up right after, over USB (Improv). As long as
  Wi-Fi has never worked, the device also opens an open `gauge-cam-xxxxxx` access point with a
  captive portal. Once Wi-Fi works, that fallback access point only opens (after 30 s without
  Wi-Fi) if it was given a password; otherwise Wi-Fi is changed over USB from the flasher page.

## Hardware

- AI-Thinker ESP32-CAM (OV2640, 4 MB PSRAM) and its **ESP32-CAM-MB** USB board.
- Stable 5 V supply (1 A): brown-outs break the camera or Wi-Fi.
- Camera fixed **facing** the dial; the LED is enough in a cellar.

Network: the MQTT server; for updates, HTTPS to `api.github.com`, `github.com` and
`release-assets.githubusercontent.com`; NTP (UDP 123) to the chosen time server.

## Installation

1. Open the [flasher page](https://tdebuilt.github.io/gauge-cam/), plug in the ESP32-CAM-MB and
   click *Install*.
2. Set up Wi-Fi when offered, then *Visit device*: the device opens its **set-up**, six steps that
   can each wait (*Later*):
   - *Photo* (light and rotation);
   - *Framing* (drag the corners around the dial; the needle centre is found by itself, click to
     correct it);
   - *Scale* (pick the gauge model, then click the graduations in order: the next value comes by
     itself; a click between two graduations adds a point and a bubble asks for its value; zoom with
     the wheel, two fingers or +/− for precise placement);
   - *Check* (a test reading: is the value right?);
   - *Tank* (shape, then the capacity on its plate and the height, which defaults to the end of the
     scale; or the inner dimensions, or a gauging table);
   - *Home Assistant* (MQTT server, user and password; *Test and finish* only saves a connection
     that works).
3. Until every step is done, the Level tab lists what is missing, with a *Do it* button for each.
   The same settings stay in the **Dial** and **Settings** tabs.

Later updates: the *Firmware* entity in Home Assistant, Settings → System (*Check for an update*), or
by hand with the `gauge-cam.bin` file of the
[latest release](https://github.com/tdebuilt/gauge-cam/releases/latest).

## MQTT

| Topic | Content |
|---|---|
| `gauge-cam/<id>/state` | JSON: `level`, `volume_l`, `percent`, `angle`, `contrast`, `time`, sent after each successful reading (retained) |
| `gauge-cam/<id>/diag` | JSON: `rssi`, `boot`, `reset_reason`, `heap_free`, `version` (retained) |
| `gauge-cam/<id>/update` | state of the Home Assistant firmware update entity (retained) |
| `gauge-cam/<id>/status` | `ok` or the error of the last reading (retained) |
| `gauge-cam/<id>/image` | last JPEG photo (retained) |
| `gauge-cam/<id>/availability` | `online` / `offline` |
| `gauge-cam/<id>/cmd` | `read` triggers a reading, `install` installs the latest release |
| `homeassistant/status` | `online` (Home Assistant restarted) announces the entities again |

## HTTP API

`GET /api/status`, `GET|POST /api/config`, `POST /api/capture` (optional body: unsaved capture
settings), `POST /api/read`,
`POST /api/test` (reads the last photo with unsaved settings), `GET /api/photo.jpg`,
`GET /api/failures` and `GET /api/failure.jpg?id=` (refused readings), `GET /api/log`,
`POST /api/update/check` and `POST /api/update/install` (GitHub release),
`POST /api/mqtt/test` (one connection with unsaved MQTT settings), `GET /api/wifi/scan`,
`POST /api/restart`, `POST /api/update` (firmware). The `setup` key of the configuration holds the
set-up steps (`photo`, `frame`, `grad`, `check`, `tank`, `ha`) marked `done` or `later`; `null`
puts a step back to do. Passwords are never sent back. An interface password (user `admin`) can be set.

## Development

```sh
test/host/run.sh                # algorithm tests on a computer (g++, Python + Pillow)
python3 tools/mock_device.py    # interface on http://127.0.0.1:8080/ without hardware
pio run && tools/make_factory.sh    # firmware and images in dist/
```

`lib/gauge` is the algorithm in portable C++; `src/` the Arduino firmware; `web/` the interface,
compressed into the firmware at build time (`tools/prebuild.py`, 150 KB at most), with two fonts
cut down by `tools/subset_fonts.py` (SIL OFL, `web/fonts-license.txt`). Redesign in progress:
`docs/plan-refonte-ux.md`. GitHub Actions tests and builds
every push, publishes the flasher page from `main` and a release for each `v*` tag.

## License

MIT.
