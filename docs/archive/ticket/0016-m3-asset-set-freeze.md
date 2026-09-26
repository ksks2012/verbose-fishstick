# 0016 — M3 Noto asset set verification & freeze

**What to build:**

Promote the existing 6 M0.5/M2 Noto-3D-style assets to a **machine-verifiable frozen M3 fixture**.
The 6 assets already exist in `assets/manifest.json` (smile/warm/cool → `base_face.obj`,
crown/top_hat → `accessory.obj`, sunglasses → `generic.obj`); this ticket does NOT add assets,
OBJs, or manifest schema. Its product is a repeatable headless invariant test so future changes
cannot silently break the frozen fixture.

This is an **M3 fixture contract test**, not a new asset pipeline.

Roles: 0015 = baseline fence · 0016 = fixture freeze · 0017 = runtime observability ·
0018 = human category validation.

## Core product

`tests/asset/manifest_integrity_test.cpp` — a dependency-free headless CTest suite that reads
`assets/manifest.json` directly (path from `NOTO3D_ASSETS_DIR`, like the registry tests) and
validates the manifest contract **without calling the production `AssetRegistry`** (the test must
not re-test registry logic through the registry; it validates the filesystem+manifest contract
independently, so drift in either side is caught).

## What the test asserts (per entry, for the frozen set)

- Entry/ID membership: the M3 frozen set **is exactly** `smile, warm, cool, crown, top_hat,
  sunglasses` (named `kM3FrozenAssetIds`, NOT "all asset ids" — the manifest may gain assets in
  M3+/M4 without this test breaking).
- `geometry` resolves to an existing file; `geometry_source == "placeholder"`, and the ADR-0007
  invariant holds (`geometry != null ↔ geometry_source != null`).
- `reference_image` resolves to an existing file and its **SHA-256 == `metadata.reference_sha256`**.
- Placeholder geometry file's **SHA-256 == `metadata.geometry_sha256`** (both hash fields already
  exist in the schema; geometry hash doubles as the "6 assets share 3 placeholders" invariant,
  e.g. base_face.obj must hash equal across smile/warm/cool).
- `axis_data.up_axis ∈ {x, y, z}`; `metadata` has the four fields `source/revision/license/
  reference_sha256` (+ `geometry_sha256`).
- Expected placeholder mapping is a fixture contract: smile/warm/cool → `base_face.obj`,
  crown/top_hat → `accessory.obj`, sunglasses → `generic.obj`.
- **`demo_checker` is not part of the manifest** (it is an out-of-manifest demo fixture).

Assertions are on manifest contract + filesystem, never on AssetRegistry pointer/address/CPU-cache
implementation details.

## Acceptance

- [x] `tests/asset/manifest_integrity_test.cpp` added and registered in CTest; all assertions above
- [x] `ctest` green (dev + release); suite count now 5
- [x] Existing 6 assets untouched; no schema change; `demo_checker` still excluded
- [x] Docs describing the manifest/tests consistent with the new fixture contract
      (README "5 suites", CHANGELOG + CONTEXT M3 section)

**Blocked by:** 0015 (complete)
**Status:** completed

> Evidence (2026-09-26): `noto3d-manifest-tests` ALL PASS (SHA-256 KATs empty/'abc' + every
> reference/geometry hash matches `metadata.*_sha256`; frozen 6-ID set; demo_checker excluded;
> placeholder mapping + ADR-0007 invariant verified). Cross-checked against `sha256sum`
> (base_face.obj `94384f…`). dev + release ctest green 5/5. SHA-256 implemented in-file
> (FIPS 180-4) — self-contained, no third-party dependency.
**Points to ADR:** 0007 (manifest v1 single source of truth, invariant), 0002 (reference_image
metadata-only), 0008 (dependency-free headless tests)
**Rolls under spec:** ADR-0009 (M3 spec embedded; execution tickets only in docs/ticket/)