#!/usr/bin/env python3
"""Local mirror of the gauge-cam releases, for devices without Internet access.

Run it daily on a machine of the local network that has Internet access, with
OUT_DIR served over HTTP (any static web server). It downloads the latest
release, checks gauge-cam.bin against the SHA-256 digest GitHub publishes, and
writes:

    OUT_DIR/latest.json            what the device reads (same fields as GitHub)
    OUT_DIR/vX.Y.Z/gauge-cam.bin   the firmware, kept for the last --keep releases

Then set the device's "Update source" (Settings > Network and MQTT) to the
address of OUT_DIR, e.g. http://192.168.1.10:8095/gauge-cam : Home Assistant
offers the update and installs it as for a device on the Internet.

    tools/mirror_release.py /srv/www/gauge-cam [--keep 3] [--repo owner/name]

Exit status 0 when the mirror holds the latest release (fetched or already
there), 1 on any error; the mirror is then left as it was. Standard library
only.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile
import urllib.request

ASSET = "gauge-cam.bin"
TAG = re.compile(r"^v\d+\.\d+\.\d+$")
UA = {"User-Agent": "gauge-cam-mirror", "Accept": "application/vnd.github+json"}


def fetch(url, timeout=30):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout) as r:
        return r.read()


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(65536), b""):
            h.update(block)
    return h.hexdigest()


def write_atomic(path, data):
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path), prefix=".tmp-")
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        os.chmod(tmp, 0o644)
        os.replace(tmp, path)
    except BaseException:
        os.unlink(tmp)
        raise


def version_key(tag):
    return tuple(int(n) for n in tag[1:].split("."))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out_dir")
    ap.add_argument("--keep", type=int, default=3, help="releases kept in the mirror (default 3)")
    ap.add_argument("--repo", default="tdebuilt/gauge-cam")
    args = ap.parse_args()
    out = os.path.abspath(args.out_dir)
    os.makedirs(out, exist_ok=True)

    rel = json.loads(fetch(f"https://api.github.com/repos/{args.repo}/releases/latest"))
    tag = rel.get("tag_name", "")
    if not TAG.match(tag):
        sys.exit(f"unexpected release tag: {tag!r}")
    asset = next((a for a in rel.get("assets", []) if a.get("name") == ASSET), None)
    digest = str((asset or {}).get("digest", ""))
    if not asset or not re.fullmatch(r"sha256:[0-9a-f]{64}", digest):
        sys.exit(f"release {tag}: no {ASSET} with a SHA-256 digest")
    want, size = digest[7:], int(asset["size"])

    target = os.path.join(out, tag, ASSET)
    if os.path.isfile(target) and sha256(target) == want:
        fetched = False
    else:
        os.makedirs(os.path.dirname(target), exist_ok=True)
        data = fetch(asset["browser_download_url"], timeout=120)
        if len(data) != size or hashlib.sha256(data).hexdigest() != want:
            sys.exit(f"release {tag}: {ASSET} does not match the published size and SHA-256")
        write_atomic(target, data)
        fetched = True

    # Written last: the device never sees a release whose file is not in place.
    latest = {
        "tag_name": tag,
        "html_url": rel.get("html_url", ""),
        "body": (rel.get("body") or "")[:2000],
        "published_at": rel.get("published_at", ""),
        "assets": [{"name": ASSET, "browser_download_url": f"{tag}/{ASSET}", "size": size, "digest": digest}],
    }
    write_atomic(os.path.join(out, "latest.json"), (json.dumps(latest, indent=1) + "\n").encode())

    tags = sorted((d for d in os.listdir(out) if TAG.match(d) and os.path.isdir(os.path.join(out, d))),
                  key=version_key, reverse=True)
    removed = [d for d in tags[max(1, args.keep):] if d != tag]
    for d in removed:
        shutil.rmtree(os.path.join(out, d))
    print(f"{tag}: {'fetched and checked' if fetched else 'already mirrored'}"
          f"{', removed ' + ', '.join(removed) if removed else ''}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as err:
        sys.exit(f"mirror failed: {err}")
