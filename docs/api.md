# HTTP API

[Français](api.fr.md) · [Back to the README](../README.md)

The web interface uses this API, and scripts and AI agents can use it too. All answers are JSON,
except the photos. If an interface password is set, every call needs HTTP basic auth with the user
`admin`.

The device describes its own API: `GET /api` gives the links, and `GET /api/openapi.json` gives the
full [OpenAPI](../web/openapi.json) description, with every setting, its unit and its limits. For
an AI agent, start with [Driving a gauge with an AI agent](agents.md).

## Read

| Call | Returns |
|---|---|
| `GET /api` | links to the OpenAPI description and the docs |
| `GET /api/openapi.json` | the OpenAPI 3.1 description of this API |
| `GET /api/status` | version, uptime, Wi-Fi, MQTT, update state, last readings, last settings change |
| `GET /api/config` | settings, without passwords |
| `GET /api/config/undo` | what an undo would change, without changing it |
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
| `POST /api/config` | saves the settings of the body; missing keys keep their value; answers with what changed |
| `POST /api/config/undo` | brings back the settings before the last change; a second undo redoes it |
| `POST /api/mqtt/test` | tries one MQTT connection with the settings of the body |
| `POST /api/update/check` | looks for a new release now |
| `POST /api/update/install` | installs the release found |
| `POST /api/update` | installs the uploaded firmware file |
| `POST /api/restart` | restarts the device |

Errors come as `{"error": "..."}` with a 4xx or 5xx status.

## Settings worth knowing

- **Passwords are never sent back.** `GET /api/config` only says whether each one is set
  (`*_set` keys). A change shows as `"changed"`, without the value.
- **What changed**: `POST /api/config` and `POST /api/config/undo` answer with
  `changes`, for example `{"pivot_x": {"before": 308.7, "after": 310}}`. The log gets a line
  `Settings changed: pivot_x`, and Home Assistant shows the time in *Last settings change*.
- **Undo** keeps today's Wi-Fi, access point, interface password and set-up progress: it never cuts
  the device off.
- **`update_url`**: the update source. Empty means GitHub; otherwise the address of a local mirror.
- **`setup`**: the set-up steps (`photo`, `frame`, `grad`, `check`, `tank`, `ha`), each `done` or
  `later`. `null` puts a step back on the to-do list.
- **Coordinates** (`roi`, `pivot_x`, `pivot_y`, `points`) are pixels of `/api/photo.jpg`, which is
  already turned by `rotation`.

## Example: change a setting after a dry run

```sh
# what would the reading be with the centre found on the hub?
curl -u admin -H 'Content-Type: application/json' -d '{"pivot_mode":"hub"}' http://DEVICE/api/test
# keep it: the answer lists what changed
curl -u admin -H 'Content-Type: application/json' -d '{"pivot_mode":"hub"}' http://DEVICE/api/config
# worse after all? go back
curl -u admin -X POST http://DEVICE/api/config/undo
```
