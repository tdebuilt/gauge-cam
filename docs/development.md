# Development

[Français](development.fr.md) · [Back to the README](../README.md)

## Commands

```sh
test/host/run.sh                    # algorithm tests on a computer (g++, Python + Pillow)
python3 tools/mock_device.py        # the interface on http://127.0.0.1:8080/, no hardware needed
pio run && tools/make_factory.sh    # firmware and flash images in dist/
```

The simulator shows a drawn tank dial (`tools/oil_dial.py`) and reads it with the real C++ code. It
also accepts firmware uploads, to try the update tools.

## Layout

| Path | Content |
|---|---|
| `lib/gauge/` | the reading algorithm, portable C++ |
| `src/` | the Arduino firmware |
| `web/` | the interface, compressed into the firmware at build time (`tools/prebuild.py`, 150 KB at most) |
| `flasher/index.html` | the flasher page on GitHub Pages |
| `tools/` | simulator, test images, fonts, update tools |
| `docs/plan-refonte-ux.md` | design decisions of the interface |

The interface uses two fonts cut down by `tools/subset_fonts.py` (SIL OFL, `web/fonts-license.txt`).
The flasher page uses the same ones. More languages go in `web/i18n.js`.

## Releasing

GitHub Actions tests and builds every push, and publishes the flasher page from `main`.

For a release:

1. Add a section `## vX.Y.Z` at the top of [`CHANGELOG.md`](../CHANGELOG.md). It becomes the
   release notes that Home Assistant shows, so keep the first 250 characters meaningful.
2. Push a tag `vX.Y.Z`.

The workflow then publishes the release with `gauge-cam.bin` and the SHA-256 digests that updates
rely on. A tag without a changelog section is not released.
