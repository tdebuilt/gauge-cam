# Changelog

One section per release, newest first. The section of a release becomes its release notes, which
Home Assistant shows on the *Firmware* entity: keep it short, the first 250 characters matter.

## v0.8.3

2026-11-25

- The needle is the middle of its dark band, not its darkest ray: a reflection along a wide needle
  made readings jump by about 3 % of the scale between photos. Tested on a real gauge: 0.2 cm spread
  instead of 3.3 cm.

## v0.8.2

2026-11-25

- The first reading after a start waits for the clock (2 min at most): Home Assistant no longer shows
  an unknown *Last reading* until the next scheduled one.

## v0.8.1

2026-11-25

- The clock is trusted only once NTP has answered: right after an update, a wrong date (2083) could
  reach Home Assistant as the time of a reading and of the last start, and trigger a false alert.

## v0.8.0

2026-11-24

- Settings → System → *Undo the last change*; also `POST /api/config/undo` (a second undo redoes).
- Every settings change is logged with its keys, and shown in Home Assistant (*Last settings change*).
- The log keeps its last lines across a restart or a crash.
- The device describes its API (`/api/openapi.json`); a skill and a guide let AI agents use it.

## v0.7.3

2026-11-24

- Release notes come from this changelog, with a link to it instead of the raw commit comparison.

## v0.7.2

2026-11-24

- Home Assistant shows what changed in a release, not only a link.
- The summary sent to Home Assistant is cut at a line end.

## v0.7.1

2026-11-24

- Settings → System shows where updates come from: GitHub or a local mirror.

## v0.7.0

2026-11-24

- *Update source* setting: devices without Internet update from a local mirror, still from Home
  Assistant.
- `tools/mirror_release.py` keeps that mirror; README and flasher page describe the three ways.

## v0.6.1

2026-11-24

- Updates from GitHub check the SHA-256 digest published with the release before installing.
- `tools/push_firmware.sh` installs a release on a device without Internet.

## v0.6.0

2026-11-24

First release. Reads a needle gauge with an ESP32-CAM, sends the value to Home Assistant over MQTT,
tank volume included; set-up and updates from the browser.

- Hub mode: the needle centre is fitted to the hub outline, the needle root no longer pulls it.
