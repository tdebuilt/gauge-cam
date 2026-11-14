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
- **Rectangular tank**: volume = level × length × width, fill = level / height.
- **Plausibility filter**: each reading takes 3 photos and keeps the median; a level that jumps
  further than 5 cm (adjustable) from the last one is checked again 2 minutes later and accepted
  only once the next 2 readings agree, so a refill gets through and a stray reflection does not.
  The last 4 refused readings stay visible with their photo in the Reading tab.
- **Flash LED** lit before each photo; a reading whose needle contrast is too low (dark photo) is
  rejected and the last good value is kept.
- **Capture settings next to the photo**, as in AI-on-the-edge: LED intensity, rotation (0, 90,
  180, 270°, for a camera mounted sideways), flips, resolution, brightness and contrast. Each change
  takes a new photo without saving anything; the outline, centre and points follow the rotation.
  The photo is rotated on the ESP32, so Home Assistant gets it upright too.
- **Home Assistant MQTT discovery** (can be turned off, entities are then removed): level, volume,
  fill, photo, "read now" button, reading status, needle angle, contrast and Wi-Fi signal. Announced
  again whenever Home Assistant restarts.
- **Web interface** in French and English (more languages: `web/i18n.js`), firmware update,
  configuration backup and restore.
- **Browser flashing** (Chrome or Edge) with Wi-Fi set-up right after, over USB (Improv). As long as
  Wi-Fi has never worked, the device also opens an open `gauge-cam-xxxxxx` access point with a
  captive portal. Once Wi-Fi works, that fallback access point only opens (after 30 s without
  Wi-Fi) if it was given a password; otherwise Wi-Fi is changed over USB from the flasher page.

## Hardware

- AI-Thinker ESP32-CAM (OV2640, 4 MB PSRAM) and its **ESP32-CAM-MB** USB board.
- Stable 5 V supply (1 A): brown-outs break the camera or Wi-Fi.
- Camera fixed **facing** the dial; the LED is enough in a cellar.

## Installation

1. Open the [flasher page](https://tdelix.github.io/gauge-cam/), plug in the ESP32-CAM-MB and
   click *Install*.
2. Set up Wi-Fi when offered, then open the device.
3. **Dial** tab: *New photo*, adjust the light and rotation if needed, then on the image: the dial outline, the needle centre, the start
   and end of the scale with their values, then intermediate points wherever the graduation
   spacing changes, red zone included. *Test*, then *Save*.
4. **Tank** tab: inner dimensions. **Network** tab: MQTT server.

Later updates: System tab, `gauge-cam.bin` file of the
[latest release](https://github.com/tdelix/gauge-cam/releases/latest).

## MQTT

| Topic | Content |
|---|---|
| `gauge-cam/<id>/state` | JSON: `level`, `volume_l`, `percent`, `angle`, `contrast`, `rssi` (retained) |
| `gauge-cam/<id>/status` | `ok` or the error of the last reading (retained) |
| `gauge-cam/<id>/image` | last JPEG photo (retained) |
| `gauge-cam/<id>/availability` | `online` / `offline` |
| `gauge-cam/<id>/cmd` | `read` triggers a reading |
| `homeassistant/status` | `online` (Home Assistant restarted) announces the entities again |

## HTTP API

`GET /api/status`, `GET|POST /api/config`, `POST /api/capture` (optional body: unsaved capture
settings), `POST /api/read`,
`POST /api/test` (reads the last photo with unsaved settings), `GET /api/photo.jpg`,
`GET /api/failures` and `GET /api/failure.jpg?id=` (refused readings),
`GET /api/wifi/scan`, `POST /api/restart`, `POST /api/update` (firmware). Passwords are never
sent back. An interface password (user `admin`) can be set.

## Development

```sh
test/host/run.sh                # algorithm tests on a computer (g++, Python + Pillow)
python3 tools/mock_device.py    # interface on http://127.0.0.1:8080/ without hardware
pio run && tools/make_factory.sh    # firmware and images in dist/
```

`lib/gauge` is the algorithm in portable C++; `src/` the Arduino firmware; `web/` the interface,
compressed into the firmware at build time (`tools/prebuild.py`). GitHub Actions tests and builds
every push, publishes the flasher page from `main` and a release for each `v*` tag.

## License

MIT.
