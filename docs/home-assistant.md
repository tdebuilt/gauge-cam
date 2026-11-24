# Home Assistant

[Français](home-assistant.fr.md) · [Back to the README](../README.md)

The device announces itself through **MQTT discovery**. With the MQTT integration in place, nothing
else is needed. Turning discovery off in Settings → Network and MQTT removes the entities.

Entity names follow the device language (Settings → Network and MQTT): French or English.

## Entities

| Entity | Content |
|---|---|
| Level | needle value, in cm or the dial's unit |
| Volume | litres, once the tank is set |
| Fill level | % of the capacity |
| Last reading | time of the last **good** reading |
| Photo | last photo, upright |
| Read now | button: takes a reading at once |
| Firmware | update entity: version, release notes, *Install* |

Diagnostics: reading status, needle angle, needle contrast, Wi-Fi signal, last start and its cause, last settings change (the time
the dial, camera or tank settings last changed).
Free memory is there too, disabled by default.

The device announces its entities again whenever Home Assistant restarts.

## MQTT topics

`<id>` is the device id, for example `gauge-cam-201baf`. The prefix `gauge-cam` can be changed.

| Topic | Content |
|---|---|
| `gauge-cam/<id>/state` | JSON after each good reading: `level`, `volume_l`, `percent`, `angle`, `contrast`, `time` (retained) |
| `gauge-cam/<id>/diag` | JSON: `rssi`, `boot`, `reset_reason`, `heap_free`, `version`, `config_changed` (retained) |
| `gauge-cam/<id>/status` | `ok`, or the error of the last reading (retained) |
| `gauge-cam/<id>/image` | last JPEG photo (retained) |
| `gauge-cam/<id>/update` | state of the firmware entity (retained) |
| `gauge-cam/<id>/availability` | `online` or `offline` |
| `gauge-cam/<id>/cmd` | send `read` to take a reading, `install` to install the latest release |
| `homeassistant/status` | `online` from Home Assistant: the device announces its entities again |
