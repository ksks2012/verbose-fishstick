# ADR-0009 — M3 vertical slice: frozen fixture, observability, human category verification

Status: Accepted

> Per DOCUMENTATION-LIFECYCLE: **specs (PRDs) live at the ADR layer**; `docs/ticket/` holds
> execution tickets derived from accepted ADRs. This ADR embeds the M3 milestone spec (Problem /
> Solution / User Stories / Testing / Out of Scope) below; the M3 execution tickets 0015–0018 roll
> under it.

## Context
M2 (asset pipeline + registry integration) is complete: `noto3d-editor` renders the 6 manifest
assets and the 0014 demo fixture. M3 is the "Noto vertical slice": prove the 6-asset product is a)
machine-frozen, b) observable at runtime, and c) human-verifiable as representing the Noto
categories the manifest declares — **without adding a single renderer/GL/text capability** and
**without touching the asset set or manifest schema**.

Observations that drove M3 (found at M2 close):
- `--asset=<id>` renders but prints no identity on stdout (only the default path logged
  "rendering manifest asset 'smile'").
- The window title is a hard-coded `noto3d-editor (M2)`.
- The 6 reference PNGs are metadata-only (ADR-0002); there is no runtime path that reports which
  reference a selected asset maps to.
- Nothing yet verifies the manifest↔filesystem contract independently of the registry code.

## Decision
M3 is scoped to the **existing 6 assets** (`smile, warm, cool, crown, top_hat, sunglasses` → the
three shared placeholders `base_face/accessory/generic`). Four sequential tickets:

- **0015 — baseline fence**: pure verification + objective cleanup. Fresh configure + build +
  `ctest` green on `dev`/`dev-clean`/`release` records the milestone baseline. No runtime-string
  changes; only objectively wrong text is fixed; anything that belongs to 0017's runtime identity
  is logged "Deferred to 0017" and left alone.
- **0016 — fixture freeze**: the manifest contract is enforced by a dependency-free headless test
  that parses `assets/manifest.json` **directly** (JSON + filesystem + SHA-256) and **never calls
  the production `AssetRegistry`** — the test is the independent contract check, so drift in either
  the manifest or the registry's assumptions is caught. The frozen set is typed as
  `kM3FrozenAssetIds` (exactly the 6 IDs), NOT "all ids", so the manifest may gain assets in later
  milestones without breaking it. `demo_checker` remains excluded (out-of-manifest demo fixture).
  Assertions are on manifest contract + filesystem only (never registry internals).
- **0017 — observability**: every selection (default and `--asset=<id>`) prints the SAME identity
  line (id, geometry source, reference path). The window title becomes `<id> [<source>] |
  noto3d (M3)` via the existing `glfwSetWindowTitle` (no text renderer). A `--list-assets`
  diagnostic CLI prints header + one line per asset from the existing `loadManifestOnly` +
  `entries()` surface and **exits 0 before any GL context is created**; no new enumeration API,
  no schema change.
- **0018 — human category verification**: a one-shot milestone evidence record. A human reviewer
  renders each frozen asset and confirms the placeholder still represents the Noto category the
  manifest declares (6-row matrix, PASS/notes). It verifies category/identity consistency only.

Verified **exclusions** for 0018 (locked): no runtime reference-image loading; no screenshot /
readback pipeline; no image-similarity algorithm; no `ReferencePreview` GPU quad (M5 UI
territory). The output is a milestone record, not a repeatable automated check. `reference_image`
stays metadata-only per ADR-0002.

M3 ordering is a fence→freeze→observe→verify chain; M4 (scene) is deliberately deferred until M3
closes. M3 makes no architectural change that M4+ must undo: it only adds a contract test, stdout
diagnostics, and a pre-GL CLI.

## Consequences
- The 6-asset fixture is machine-frozen and human-verified; identity is observable at runtime.
- M3 adds no renderer/GL/text capability and no manifest schema change.
- M3 verification is explicitly NOT similarity-based (placeholder geometry is not official
  geometry; numeric comparison has no engineering value — ADR-0001/0002).
- Future asset additions can extend the fixture contract without rewriting it
  (`kM3FrozenAssetIds` is not "all ids").

## Spec: M3 Noto vertical slice (PRD)

### Problem Statement
The 6 Noto-3D-style assets exist and render, but the pipeline is unobservable and unverified as a
milestone product. When asked to select `warm` the editor prints nothing about what it chose; the
window title is a frozen `(M2)` string; nothing checks that `assets/manifest.json`, its declared
file hashes, and the files on disk stay in agreement as the asset set evolves; and nothing
human-verifies that each placeholder still represents the Noto category its metadata claims. Built
and extended parties cannot answer "which asset is this run rendering?", "is the frozen fixture
intact?", or "do these 6 assets really map to the Noto categories we say they do?".

### Solution
Close M3 as a **verification + observability pass over the existing 6-asset fixture**, without
adding renderer capability or manifest schema: (1) **freeze** the fixture with a dependency-free
headless contract test reading `assets/manifest.json` directly (file presence + SHA-256 +
invariant/mapping against the frozen 6-ID set), independent of `AssetRegistry`; (2) **observe** the
selection at runtime — every choice prints one identity line, the window title reflects
`<id> [<source>] | noto3d (M3)`, and a `--list-assets` CLI enumerates the frozen set and exits
before any GL context; (3) **verify by human** that each placeholder still represents its declared
Noto category (one-shot manual matrix, PASS/notes) — explicitly not a similarity measurement.

### User Stories
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

### Testing Decisions
- **What good looks like** — test external behaviour only: the manifest contract (data +
  filesystem) and the process-level CLI surface (stdout lines + exit codes). Never registry
  internals, pointer addresses, or CPU-cache details.
- **Primary seam (one)**: the existing headless CTest family in `tests/` driven by `ctest`.
  0016 lands as a new suite (**asset-manifest-integrity**) alongside **asset-registry-contract**,
  both using the same `NOTO3D_ASSETS_DIR` compile-time define. 0017 is verified by manual
  process-level checks (identity line + exit codes, `--list-assets` output, window title via X
  `WM_NAME`); no new CTest target is required — keep the seam count at one.
- **Prior art** — `tests/asset/registry_test.cpp` (headless registry contract with `check()` /
  "ALL PASS" runner); `tests/core/gl_capability_test.cpp`; the 0016 suite extends exactly this
  pattern without depending on it (independent assertion set). 0018 is by design a manual matrix,
  not a test seam.
- **Regression** — `ctest` green on `dev` + `release` (suite count 5) plus unchanged exit-code
  behaviour for existing flags.

### Out of Scope
- New assets, OBJs, or manifest schema additions.
- Any renderer/GL/text capability, window/overlay UI, or reference-image loading at runtime.
- Image/geometry similarity, pixel/pose/color comparison, screenshot or readback pipelines.
- `ReferencePreview` UI (M5 territory) and ImGui work (M5).
- M4 scene milestone and all scene/serialization work (deferred until M3 closes).
- Per-build automated re-verification of human category judgement (0018 is a milestone record).

### Further Notes
- M3 verification is deliberately not numeric similarity: placeholder geometry is not official
  geometry (ADR-0001/0002), so "category/identity consistency" is the only meaningful check.
- The geometry SHA-256 doubles as the shared-placeholder invariant (e.g. `base_face.obj` hashes
  equal across `smile`/`warm`/`cool`); the 0016 test asserts it per entry through
  `metadata.geometry_sha256`.
- `--list-assets` runs pre-GL and is headless-safe; it is the natural tool for 0018's reviewer to
  confirm the frozen set before rendering each asset.
- Execution status: **M3 CLOSED.** 0015/0016/0017/0018 all complete and archived (acceptance ✓).
  0018's human matrix filled 2026-09-26: 6/6 PASS — rendered each `--asset=<id>` against its
  official reference PNG; observed the 3 shared placeholder silhouettes (base_face ×3,
  accessory ×2, generic ×1), consistent with the 0016 shared-geometry invariant. One informational
  note: `cool` (u1f60e) is a smiling face with sunglasses, which the base_face placeholder cannot
  convey — not a failure, deferred to the official-mesh watch item.