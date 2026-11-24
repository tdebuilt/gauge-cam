# Updates

[Français](updates.fr.md) · [Back to the README](../README.md)

## Which way for your device

| The device… | Do this | In Home Assistant |
|---|---|---|
| has Internet | Nothing: it asks GitHub every day. | *Firmware* entity → *Install* |
| has no Internet | Run a **local mirror** and enter its address in the device. | *Firmware* entity → *Install* |
| has no Internet and no mirror | **Upload** each release from a computer. | the new version shows once installed |

Whichever way:

- the device checks the file against the **SHA-256 digest** published with the release, before
  installing it;
- if the new firmware crashes, or cannot join the Wi-Fi within 5 minutes, the device goes back to the
  previous one;
- the release notes show in Home Assistant, in the *Firmware* entity.

## With Internet

Nothing to set. The device checks GitHub once a day over HTTPS, with certificate checks. HTTPS needs
the right time, so the device must also reach an NTP server.

## Local mirror

For a device without Internet. A machine of your network that does have Internet (server, NAS,
Raspberry Pi) keeps a copy of the latest release.

1. **Run the tool every day**, for example from cron:

   ```
   15 4 * * *  /opt/gauge-cam/tools/mirror_release.py /srv/www/gauge-cam
   ```

   `tools/mirror_release.py` needs only Python. It downloads the latest release, checks it against
   the digest GitHub publishes, and keeps the last 3 releases.
2. **Serve the folder** with any static web server.
3. **Point the device at it**: Settings → Network and MQTT → *Update source*, for example
   `http://192.168.1.10:8095/gauge-cam`.
4. **Open the firewall** from the device to that port, and to nothing else.

The mirror is plain HTTP on purpose: without Internet, the device may not have the time needed to
check a certificate. The digest is what protects the file, and the mirror only publishes a file it
has checked against GitHub.

## Upload from a computer

```sh
tools/push_firmware.sh http://DEVICE-ADDRESS [vX.Y.Z]
```

The script downloads `gauge-cam.bin`, checks its digest, sends it to the device and waits for the new
version. It asks for the interface password, or reads `GAUGECAM_PASSWORD`.

Without a mirror, turn off the daily check (Settings → Network and MQTT): it would fail every day.

## By hand

Settings → System, then pick the `gauge-cam.bin` file of the
[latest release](https://github.com/tdebuilt/gauge-cam/releases/latest).

This way skips the digest check: the firmware only checks that the image is valid.
