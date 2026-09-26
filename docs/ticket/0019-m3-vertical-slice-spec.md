# 0019 — M3 Noto vertical slice — milestone spec

**Status:** accepted (spec) — execution tracked per-ticket, rolling under this spec
**Triage label:** ready-for-agent (local tracker = this file; no further triage needed)
**Blocked by:** M2 complete (0012/0010)
**Points to ADR:** 0009 (M3 decisions). Execution tickets rolling under this spec:
**0015** (fence, archived) · **0016** (fixture freeze, archived) · **0017** (observability,
archived) · **0018** (human category verification, open).

## Problem Statement

The 6 Noto-3D-style assets exist and render, but the pipeline is unobservable and unverified as a
milestone product. When asked to select `warm` the editor prints nothing about what it chose; the
window title is a frozen `(M2)` string; nothing checks that `assets/manifest.json`, its declared
file hashes, and the actual files on disk stay in agreement as the asset set evolves; and nothing
human-verifies that each placeholder still represents the Noto category its metadata claims. The
team building and extending the editor cannot answer "which asset is this run rendering?", "is the
frozen fixture intact?", or "do these 6 assets really map to the Noto categories we say they do?".

## Solution

Close M3 as a **verification + observability pass over the existing 6-asset fixture**, without
adding renderer capability or manifest schema:
1. **Freeze** the fixture with a dependency-free headless contract test that reads
   `assets/manifest.json` directly and validates file presence + SHA-256 + invariant/mapping
   against the frozen 6-ID set — independent of `AssetRegistry`, so either side drifting is caught.
2. **Observe** the selection at runtime: every choice (default or `--asset=<id>`) prints one
   identity line; the window title reflects `<id> [<source>] | noto3d (M3)`; a `--list-assets`
   CLI enumerates the frozen set and exits before any GL context.
3. **Verify by human** that each placeholder still represents its declared Noto category
   (one-shot manual matrix, PASS/notes), explicitly not a similarity measurement.

## User Stories

1. As a developer running the editor, I want `--asset=<id>` to print the selected asset's id,
   geometry source, and reference path, so that I can tell at a glance what the run is rendering.
2. As a developer running the editor with no `--asset` flag, I want the same identity line printed
   for the default-selected asset, so that the default selection is observable via the same code
   path as an explicit one.
3. As a developer glancing at the window, I want the title bar to show the selected asset id and
   geometry source, so that I can identify the active asset without reading stdout.
4. As a developer, I want `--list-assets` to enumerate the 6 frozen assets with geometry source
   and reference path and exit 0, so that I can query the asset set without opening a GL window.
5. As a developer/CI, I want the frozen fixture contract (exactly 6 IDs, `demo_checker` excluded,
   `geometry_source == placeholder`, ADR-0007 nullness invariant, reference and geometry files
   existing with SHA-256 equal to `metadata.reference_sha256`/`geometry_sha256`,
   `axis_data.up_axis ∈ {x,y,z}`, expected placeholder mapping) validated headlessly in CTest, so
   that future fixture changes cannot silently break the milestone.
6. As a developer, I want the contract test to read the manifest **directly** rather than through
   the registry, so that drift in the manifest and drift in the registry's assumptions are both
   detectable.
7. As a developer adding assets in later milestones, I want the frozen-set assertion to be a named
   constant (`kM3FrozenAssetIds`, exactly the 6 M3 IDs) rather than "all ids", so that growing the
   manifest does not break the M3 contract test.
8. As a human reviewer, I want to render each frozen asset, compare it with its official reference
   PNG, and record PASS/notes in a 6-row matrix, so that we have milestone evidence that every
   asset still represents its declared Noto category.
9. As a project lead, I want the M3 evidence to be a one-shot record (not a per-build regression,
   no similarity/screenshot/readback pipeline), so that effort stays proportionate to a fixture
   whose geometry is placeholders by design (ADR-0001/0002).
10. As a maintainer, I want M3 to leave the renderer, the manifest schema, and the asset files
    untouched, so that M4 scene work starts from a clean contract.
11. As a developer, I want existing flag behaviour preserved (`--frames=`, `--demo`, exit codes:
    valid → 0; `--frames=abc`/`--asset=nope` → non-zero), so that the M3 changes are purely
    additive.

## Implementation Decisions

- **Scope lock** — the 6 existing assets (`smile, warm, cool, crown, top_hat, sunglasses` → the
  three shared placeholders `base_face/accessory/generic`) are the M3 fixture; no new assets, no
  OBJ edits, no manifest schema change, no renderer/GL/text capability added.
- **Fixture freeze is an independent contract test** — parses `assets/manifest.json` directly,
  resolves paths relative to `NOTO3D_ASSETS_DIR`, hashes files with an in-file FIPS 180-4 SHA-256
  (no third-party crypto dependency). It never calls the production `AssetRegistry`, so manifest↔
  registry drift is caught; assertions target manifest contract + filesystem only.
- **Frozen set is a named constant** (`kM3FrozenAssetIds`, exactly the 6 IDs), distinct from "all
  ids"; `demo_checker` remains an out-of-manifest demo fixture.
- **Unified selection diagnostics** — the default-selection branch and the `--asset=<id>` branch
  converge on one identity line (`selected asset '<id>' geometry=<source> reference=<path>`);
  the prior default-only special-case logging is removed.
- **Dynamic window title via existing API** — `glfwSetWindowTitle(window, "<id> [<source>] |
  noto3d (M3)")`; no text renderer/overlay.
- **`--list-assets` is a diagnostic CLI, not a pipeline capability** — uses the existing
  `loadManifestOnly()` + `entries()` surface; prints a header line then one line per asset; exits 0
  before any GLFW/GL context. No new enumeration API.
- **Human verification is one-shot** — 6-row manual matrix (PASS/notes); locked exclusions: no
  runtime reference-image loading, no screenshot/readback, no image-similarity, no
  `ReferencePreview` GPU quad (M5 UI territory). `reference_image` stays metadata-only.
- **Ordering is a fence chain** — 0015 baseline gate (pure verification; runtime-string changes
  deferred to 0017) → 0016 freeze → 0017 observability → 0018 human check; M4 is deferred until
  M3 closes.

## Testing Decisions

- **What good looks like** — test external behaviour only: the manifest contract (data + filesystem)
  and the process-level CLI surface (stdout lines + exit codes). Never registry internals, pointer
  addresses, or CPU-cache details.
- **Primary seam (one)**: the existing headless CTest family in `tests/` driven by `ctest`.
  - 0016 lands as a new CTest suite (**asset-manifest-integrity**) alongside the existing
    **asset-registry-contract**; both run with the same `NOTO3D_ASSETS_DIR` compile-time define.
  - Secondary, manual, process-level checks for 0017: run `noto3d-editor` with/without `--asset=`,
    assert the identity line + exit codes; run `--list-assets` and confirm header + 6 lines + exit 0;
    read the window title via the X server (`WM_NAME`) for the dynamic-title case. No new CTest
    target is required — keep the seam count at one.
- **Prior art** — `tests/asset/registry_test.cpp` (headless registry contract with `check()`/
  "ALL PASS" runner); `tests/core/gl_capability_test.cpp`; the M3 0016 suite extends exactly this
  pattern without depending on it (independent assertion set). 0018 is by design a manual matrix,
  not a test seam.
- **Regression** — `ctest` green on `dev` + `release` (suite count 5) plus unchanged exit-code
  behaviour for existing flags.

## Out of Scope

- New assets, OBJs, or manifest schema additions.
- Any renderer/GL/text capability, window/overlay UI, or reference-image loading at runtime.
- Image/geometry similarity, pixel/pose/color comparison, screenshot or readback pipelines.
- `ReferencePreview` UI (M5 territory) and ImGui work (M5).
- M4 scene milestone and all scene/serialization work (deferred until M3 closes).
- Per-build automated re-verification of human category judgement (0018 is a milestone record).

## Further Notes

- M3 verification is deliberately not numeric similarity: placeholder geometry is not official
  geometry (ADR-0001/0002), so "category/identity consistency" is the only meaningful check.
- The geometry SHA-256 doubles as the shared-placeholder invariant (e.g. `base_face.obj` hashes
  equal across `smile`/`warm`/`cool`); the 0016 test asserts it per entry through
  `metadata.geometry_sha256`.
- `--list-assets` runs pre-GL and is headless-safe; it is the natural tool for 0018's reviewer to
  confirm the frozen set before rendering each asset.
- Execution status: 0015/0016/0017 complete and archived (all acceptance checks ✓); 0018 open —
  its matrix fill is the human-facing remaining work.