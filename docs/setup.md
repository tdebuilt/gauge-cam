# Setup

[Français](setup.fr.md) · [Back to the README](../README.md)

## Hardware

| Part | Note |
|---|---|
| AI-Thinker ESP32-CAM | OV2640 camera, 4 MB PSRAM |
| ESP32-CAM-MB USB board | to flash it and power it |
| 5 V, 1 A power supply | a weak supply makes the camera or the Wi-Fi fail |

Mount the camera **facing** the dial. In a cellar, the flash LED gives enough light.

## Network

The device needs to reach:

- the **MQTT server**;
- an **NTP server** (UDP 123), so that readings and logs are dated;
- for updates, **one** of these:
  - GitHub over HTTPS: `api.github.com`, `github.com` and `release-assets.githubusercontent.com`;
  - a local mirror, if the device has no Internet (see [Updates](updates.md)).

## 1. Flash

1. Open the [flasher page](https://tdebuilt.github.io/gauge-cam/) in Chrome or Edge.
2. Plug in the ESP32-CAM-MB and click *Install*.
3. Enter your Wi-Fi when the page asks, then click *Visit device*.

If the page cannot connect, put the board in download mode:

1. Hold **IO0** on the MB board.
2. Press **RST** on the ESP32-CAM itself, briefly. The RST button of the MB board does not always work.
3. Release **IO0** and click *Install* again.

## 2. The six set-up steps

The device opens its set-up by itself. Each step can wait: click *Later*. Until every step is done,
the **Level** tab lists what is missing, with a *Do it* button for each one.

| Step | What you do |
|---|---|
| **Photo** | Set the light and the rotation, so that the dial is upright. |
| **Framing** | Drag the corners around the dial. The device finds the needle centre; click to correct it. |
| **Scale** | Pick the gauge model, then click the graduations in order (details below). |
| **Check** | Take a test reading and check that the value is right. |
| **Tank** | Enter the shape and size of the tank, or click *Not a tank* for any other dial. |
| **Home Assistant** | Enter the device name, the MQTT server, the user and the password. |

### Scale

- **Models**: tank gauge in cm (a mark every 10 cm up to the end of the scale), 0–100 %, or any dial
  whose values you type.
- **Clicking**: the next value comes by itself. *No mark at…* skips a value the dial does not print.
- **More precision**: click between two graduations to add a point; a bubble asks for its value.
- **Corrections**: drag a point to move it, click it to change or delete it.
- **Zoom**: mouse wheel, two fingers, or the +/− buttons.

### Tank

Pick the shape, then **one** of:

- the capacity written on the tank's plate, and its height (the end of the scale by default);
- the inner dimensions;
- a gauging table, from level to litres, for any other shape.

### Home Assistant

*Test and finish* saves the connection only if it works.

Home Assistant builds the entity ids from the device name at the first connection. Renaming the
device later does not change them, so choose the name first.

## 3. Afterwards

Every setting stays available in the **Dial** and **Settings** tabs. Each change to the camera
settings takes a new photo without saving anything:

- LED intensity;
- rotation (0, 90, 180 or 270°), for a camera mounted sideways;
- flips, resolution, brightness and contrast;
- fixed exposure and gain, so that every photo looks the same.

The frame, the centre and the graduations follow the rotation. The device rotates the photo itself,
so Home Assistant gets it upright too.

## Security

- **Interface password**: optional, user `admin`, in Settings → Network and MQTT.
- **Passwords are never sent back** by the device.
- **The stored MQTT password** only works with the server, port and user it was typed for.
  Changing one of them asks for the password again.
- **Fallback access point**: until the Wi-Fi has worked once, the device also opens an open access
  point, `gauge-cam-xxxxxx`, with a captive portal. After that, it opens only if you gave it a
  password, after 30 s without Wi-Fi. Without that password, change the Wi-Fi over USB from the
  flasher page.
