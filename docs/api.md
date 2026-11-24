# HTTP API

[Français](api.fr.md) · [Back to the README](../README.md)

The web interface uses this API, and scripts can use it too. All answers are JSON, except the photos.
If an interface password is set, every call needs HTTP basic auth with the user `admin`.

## Read

| Call | Returns |
|---|---|
| `GET /api/status` | version, uptime, Wi-Fi, MQTT, update state |
| `GET /api/config` | settings, without passwords |
| `GET /api/photo.jpg` | last photo |
| `GET /api/failures` | last refused readings |
| `GET /api/failure.jpg?id=` | photo of a refused reading |
| `GET /api/log` | last 80 events |
| `GET /api/wifi/scan` | Wi-Fi networks in range |

## Act

| Call | Effect |
|---|---|
| `POST /api/capture` | takes a photo; an optional JSON body tries camera settings without saving them |
| `POST /api/read` | full reading; `?photo=0` reads the last photo again |
| `POST /api/test` | reads the last photo with the settings of the body, without saving them |
| `POST /api/config` | saves the settings of the body; missing keys keep their value |
| `POST /api/mqtt/test` | tries one MQTT connection with the settings of the body |
| `POST /api/update/check` | looks for a new release now |
| `POST /api/update/install` | installs the release found |
| `POST /api/update` | installs the uploaded firmware file |
| `POST /api/restart` | restarts the device |

## Settings worth knowing

- **Passwords are never sent back.** `GET /api/config` only says whether each one is set
  (`*_set` keys).
- **`update_url`**: the update source. Empty means GitHub; otherwise the address of a local mirror.
- **`setup`**: the set-up steps (`photo`, `frame`, `grad`, `check`, `tank`, `ha`), each `done` or
  `later`. `null` puts a step back on the to-do list.

## Example: change a setting after a dry run

```sh
# what would the reading be with the centre found on the hub?
curl -u admin -H 'Content-Type: application/json' -d '{"pivot_mode":"hub"}' http://DEVICE/api/test
# keep it
curl -u admin -H 'Content-Type: application/json' -d '{"pivot_mode":"hub"}' http://DEVICE/api/config
```
