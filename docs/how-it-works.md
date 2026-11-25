# How it works

[Français](how-it-works.fr.md) · [Back to the README](../README.md)

## A reading

1. **Photo.** The flash LED lights up and the camera takes a photo.
2. **Centre.** The needle turns around a dark hub. The device fits a circle to the hub's outline,
   leaving out the root of the needle, and follows it from one photo to the next. You can also fix
   the centre on a pixel instead.
3. **Needle.** The needle is the middle of the dark band between the hub and the graduations, so a
   reflection along it does not move the reading. Coloured marks, such as a red zone, are not
   mistaken for it, and their values can still be read.
4. **Value.** The value is interpolated between the two neighbouring graduations. A scale with
   uneven spacing is fine: an Afriso dial, for example, has no mark between 0 and 40 cm.
5. **Volume.** The level becomes litres through the tank shape, and the fill level is the volume
   over the capacity.

Everything runs on the ESP32, in plain C++: no neural network and no server.

## Checks on each reading

| Check | Effect |
|---|---|
| **3 photos** | Each scheduled reading keeps the median of 3 photos. |
| **Contrast** | If the needle stands out too little (dark photo), the reading is refused and the last good value stays. |
| **Jump** | A level more than 5 cm (adjustable) away from the last one is read again 2 minutes later. It is accepted only once the next 2 readings agree: a refill gets through, a stray reflection does not. |
| **Refused readings** | The last 4 stay in the Level tab, with their photo. |
| **Missed readings** | After 3 missed readings, Home Assistant shows level, volume and fill level as unavailable rather than an old value. |

## Running unattended

- **Stuck program**: a watchdog restarts it.
- **Camera failure**: the device starts the camera again, then restarts, at most 3 times after power-on.
- **Network loss**: the device restarts after 1 hour without Wi-Fi, or without the MQTT server it had reached.
- **Several access points**: the device joins the strongest one, and leaves it before a planned restart.
  Two minutes after each connection it scans again and moves, at most once an hour, to an access point
  at least 6 dB stronger (after a power cut, the near one may refuse it for a while). The log tells the outcome of each check.
- **Log**: Settings → System keeps the last 80 events and the cause of the last restart (crash,
  watchdog, brown-out…).
- **Clock**: an NTP server dates the readings, the refused readings and the log. The default is
  `pool.ntp.org`; your router works too.
