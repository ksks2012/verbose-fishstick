# ADR-0009 — M3 vertical slice: frozen fixture, observability, human category verification

Status: Accepted

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