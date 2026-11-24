#!/usr/bin/env bash
# Install a gauge-cam release on a device that has no Internet access, from a
# computer that has: download gauge-cam.bin from GitHub, check its SHA-256
# against the digest GitHub publishes, upload it to the device, wait for the
# new version. The device rolls back by itself if the new firmware cannot
# reach the Wi-Fi.
#
#   tools/push_firmware.sh http://192.168.1.50            # latest release
#   tools/push_firmware.sh http://192.168.1.50 v0.6.1     # a given release
#
# Web interface password: asked at the prompt, or GAUGECAM_PASSWORD in the
# environment; leave it empty if the interface has none. Needs curl, python3.
set -euo pipefail

REPO="tdebuilt/gauge-cam"
ASSET="gauge-cam.bin"
DEVICE="${1:-}"
TAG="${2:-latest}"
[[ "$DEVICE" =~ ^https?:// ]] || { echo "usage: $0 http://DEVICE [vX.Y.Z]"; exit 2; }
DEVICE="${DEVICE%/}"
[[ "$TAG" == latest || "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "bad tag: $TAG"; exit 2; }

if [[ -z "${GAUGECAM_PASSWORD+set}" ]]; then
  read -rsp "Password of the web interface (empty if none): " GAUGECAM_PASSWORD
  echo
fi
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# curl reads the credentials from stdin, never from its command line.
auth() {
  if [[ -n "$GAUGECAM_PASSWORD" ]]; then
    local p=${GAUGECAM_PASSWORD//\\/\\\\}
    p=${p//\"/\\\"}
    printf 'user = "admin:%s"\n' "$p"
  fi
}
device_version() {
  auth | curl -sS --fail --config - --max-time 15 "$DEVICE/api/status" \
    | python3 -c 'import json, sys; print(json.load(sys.stdin)["version"])'
}

installed=$(device_version) || { echo "The device does not answer (address, password?)"; exit 1; }
echo "Installed: $installed"

if [[ "$TAG" == latest ]]; then path="releases/latest"; else path="releases/tags/$TAG"; fi
curl -sS --fail --max-time 20 -H "Accept: application/vnd.github+json" \
  "https://api.github.com/repos/$REPO/$path" > "$WORK/release.json" \
  || { echo "Release $TAG not found on GitHub"; exit 1; }
read -r tag url digest size < <(python3 - "$WORK/release.json" "$ASSET" <<'PY'
import json, sys
rel = json.load(open(sys.argv[1]))
asset = next((a for a in rel.get("assets", []) if a["name"] == sys.argv[2]), None)
if not asset or not str(asset.get("digest", "")).startswith("sha256:"):
    sys.exit(1)
print(rel["tag_name"], asset["browser_download_url"], asset["digest"][7:], asset["size"])
PY
) || { echo "No $ASSET with a SHA-256 digest in release $TAG"; exit 1; }
echo "Release:   $tag ($size bytes)"
[[ "$installed" == "$tag" ]] && { echo "Already installed."; exit 0; }

curl -sS --fail -L --max-time 120 -o "$WORK/$ASSET" "$url"
[[ "$(sha256sum "$WORK/$ASSET" | cut -d' ' -f1)" == "$digest" ]] \
  || { echo "SHA-256 mismatch: not the published file, nothing sent"; exit 1; }
echo "Downloaded and checked, uploading (about a minute)..."

reply=$(auth | curl -sS --config - --max-time 240 \
  -F "firmware=@$WORK/$ASSET;type=application/octet-stream" "$DEVICE/api/update") \
  || { echo "Upload refused or interrupted"; exit 1; }
python3 -c 'import json, sys; sys.exit(0 if json.loads(sys.argv[1]).get("updated") is True else 1)' "$reply" 2>/dev/null \
  || { echo "The device refused the firmware: $reply"; exit 1; }

for _ in $(seq 1 24); do
  sleep 5
  now=$(device_version 2>/dev/null || true)
  [[ "$now" == "$tag" ]] && { echo "Done: the device runs $tag."; exit 0; }
done
echo "The device does not report $tag after 2 minutes (read: ${now:-nothing})."
exit 1
