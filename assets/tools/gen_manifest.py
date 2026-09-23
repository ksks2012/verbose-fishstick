#!/usr/bin/env python3
"""Regenerate assets/manifest.json (ADR-0007 contract).

Computes sha256 over the vendored reference PNGs and geometry OBJs so the
checksums in the manifest are always in sync with the shipped files.

Usage: python3 assets/tools/gen_manifest.py
"""
import hashlib
import json
import os

ASSETS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(ASSETS)

# Source pin for the official Noto 3D PNG references (ADR-0001/0002).
SOURCE = {
    "url": "https://github.com/googlefonts/noto-emoji",
    "path_prefix": "3D/png/512",
    "revision": "06121655d0e82f9cae6e7ba6feed4fa6fdbfc2a4",
    "license": "SIL OFL 1.1",
}

# id -> (repo filename stem, reference file, geometry file)
EMOJI = {
    "smile":      ("emoji_u1f600", "emoji_u1f600_512.png", "base_face.obj"),
    "warm":       ("emoji_u1f60a", "emoji_u1f60a_512.png", "base_face.obj"),
    "cool":       ("emoji_u1f60e", "emoji_u1f60e_512.png", "base_face.obj"),
    "crown":      ("emoji_u1f451", "emoji_u1f451_512.png", "accessory.obj"),
    "sunglasses": ("emoji_u1f576", "emoji_u1f576_512.png", "generic.obj"),
    "top_hat":    ("emoji_u1f3a9", "emoji_u1f3a9_512.png", "accessory.obj"),
}


def sha256(path):
    h = hashlib.sha256()
    with open(os.path.join(ASSETS, path), "rb") as f:
        h.update(f.read())
    return h.hexdigest()


def main():
    assets = []
    for aid, (stem, ref, geo) in EMOJI.items():
        ref_rel = f"references/noto3d/{ref}"
        geo_rel = f"geometry/placeholders/{geo}"
        assets.append({
            "id": aid,
            "reference_image": ref_rel,
            "geometry": geo_rel,
            "geometry_source": "placeholder",
            "axis_data": {
                "up_axis": "y",
                "recorded_winding": None,  # placeholder authored CCW-outward
            },
            "metadata": {
                "source": f"{SOURCE['url']}/blob/{SOURCE['revision']}/3D/png/512/{stem}.png",
                "revision": SOURCE["revision"],
                "license": SOURCE["license"],
                "reference_sha256": sha256(ref_rel),
                "geometry_sha256": sha256(geo_rel),
            },
        })

    manifest = {
        "manifest_version": 1,
        "source": SOURCE,
        "official_mesh": {
            "status": "PENDING",
            "note": "googlefonts/noto-emoji issue #549: raw OBJ 'coming with Unicode 18'; "
                    "no mesh files exist on any branch as of the pinned revision.",
        },
        "assets": assets,
    }

    out = os.path.join(ASSETS, "manifest.json")
    with open(out, "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print("wrote", out)


if __name__ == "__main__":
    main()