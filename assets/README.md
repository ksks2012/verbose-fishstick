# Assets — M0.5 Asset Contract

Status: **verified** for the pinned revision (ADR-0001/0002/0007).

## Asset Contract

```
Format (official source):
    Geometry:   none available yet — official repo ships NO mesh files
    Images:     PNG, 512×512 (also 32/72/128 in repo)
    Fonts:      TTF bitmap fonts of the 3D look (not used by this project)

Geometry status:
    official mesh: PENDING — googlefonts/noto-emoji#549, "coming with Unicode 18"
    MVP geometry:  placeholder OBJ (this repo, CCW-outward, Y-up, unit scale)

Geometry:
    positions: yes
    normals:   yes (smooth, area-weighted)
    UVs:       yes (degenerate 0.5,0.5 for placeholders; real UV mapping out of scope)
    indexing:  triangles (GL_UNSIGNED_INT)

Materials:
    MTL:        not used (placeholders are single-material)
    texture:    reference images are reference-only, NOT textures (ADR-0002)
    color:      per-asset baseColor set at scene/material level (ADR-0006)

Texture (when the texture path is exercised):
    PNG, sRGB → GL_SRGB8_ALPHA8, linearization handled by the sRGB pipeline

Coordinate:
    up axis:     Y (placeholders authored Y-up)
    handedness:  right-handed, -Z forward
    unit scale:  1.0 = base_face radius; later official meshes get per-asset
                 axis_data conversion baked at load (ADR-0007)

Topology:
    placeholders: triangles only, watertight (convex), 0 inverted faces (validated)

License (references):
    SIL Open Font License 1.1 — see LICENSE-OFL-1.1.txt
    sourced from googlefonts/noto-emoji @ 0612165 (pinned revision)
```

## Layout

```
assets/
├── manifest.json              # single source of truth (ADR-0007), regenerable
├── README.md                  # this contract
├── tools/
│   ├── gen_placeholders.py    # regenerates placeholder OBJs (validates winding)
│   └── gen_manifest.py        # regenerates manifest.json + sha256 sync
├── references/
│   ├── LICENSE-OFL-1.1.txt
│   └── noto3d/                # 6 pinned official 512px 3D renders (reference images)
├── geometry/
│   └── placeholders/          # base_face / accessory / generic
└── test/
    └── cube.obj               # permanent infrastructure test asset (ADR-0001)
```

## Asset IDs (curated set, ADR-0002)

| id | reference | placeholder geometry |
|---|---|---|
| `smile` | 😀 u1F600 | `base_face` |
| `warm` | 😊 u1F60A | `base_face` |
| `cool` | 😎 u1F60E | `base_face` |
| `crown` | 👑 u1F451 | `accessory` |
| `sunglasses` | 🕶 u1F576 | `generic` |
| `top_hat` | 🎩 u1F3A9 | `accessory` |

## Known facts / decisions

- Reference PNGs are **metadata-only** at runtime (env: `reference_image`); never promoted to
  textures implicitly (ADR-0002/0007).
- `geometry_source` is `placeholder` for all current entries; when official OBJ lands with
  Unicode 18, update the entry's `geometry` + `geometry_source` (+ record `recorded_winding`
  and any `up_axis` conversion). Scene schema does not change.
- Loader contract: `SUCCESS` / `LOADED_WITH_WARNINGS` / `FAILED`; no implicit fallback;
  FAILED disables the entity + diagnostic (ADR-0007).