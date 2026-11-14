#!/bin/sh
# Assemble the images published for flashing, after `pio run`:
#   dist/gauge-cam-factory.bin  full image at offset 0 (browser flasher, esptool)
#   dist/gauge-cam.bin          application only (update from the web interface)
set -e
cd "$(dirname "$0")/.."
BUILD=.pio/build/esp32cam
BOOT_APP0=$(find "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages/framework-arduinoespressif32" -name boot_app0.bin | head -1)
ESPTOOL=$(find "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages/tool-esptoolpy" -name esptool.py | head -1)
mkdir -p dist
python3 "$ESPTOOL" --chip esp32 merge_bin -o dist/gauge-cam-factory.bin \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 "$BUILD/bootloader.bin" 0x8000 "$BUILD/partitions.bin" \
  0xe000 "$BOOT_APP0" 0x10000 "$BUILD/firmware.bin"
cp "$BUILD/firmware.bin" dist/gauge-cam.bin
ls -l dist
