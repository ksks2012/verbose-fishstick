# 0039 — asset CPU/GPU split close-out: full graph verification + regression re-run

**Source:** ADR-0012 — tracer bullet 4, and the close of the 0027 spike chain (0036–0039).
**Status:** ready-for-agent
**Blocked by:** 0038

## What it delivers

The final verification pass over the completed `noto3d_asset_cpu` / `noto3d_asset` split. No new
architecture and (ideally) no new code: this ticket **proves** the whole chain is green, the link
graph is the intended one, and no preserved M4 contract moved. It is the "did the refactor change
anything observable?" check, made explicit rather than assumed.

## Scope

- Record the **final, actual CMake target graph** (as 0025⑧ did for the pre-split state) in this
  ticket: `noto3d_asset_cpu` (json + tinyobjloader only) → `noto3d_asset` (PUBLIC
  `noto3d_asset_cpu` + `noto3d_renderer` + json; PRIVATE tinyobjloader) → consumers
  (`noto3d-registry-tests` now on the CPU target; `noto3d_app` / `noto3d-editor` unchanged).
- Re-run and record the **full regression evidence** (below).
- Update CONTEXT.md (the accepted-decisions bullet for ADR-0012) and CHANGELOG.md in the same
  change, per the ADR-0008 document hierarchy.
- Re-confirm the preserved M4 contracts are intact: `AssetRegistry` sole authority; `Scene` pure
  data (assetIds only); `app::collectDrawItems` the single resolution seam; `DrawItem::model` the
  borrowed frame-local `const renderer::Model*`; escape hatches + path-predicate behaviour
  unchanged; `--asset`/`--demo`/default entry modes and their titles + `M4_INFO … entities=<N>`
  observationally identical.

## Invariants

- **Zero externally observable behaviour change** is the ticket's top-level invariant (the same
  invariant 0025 held for the M4 cleanup). Any behavioural diff here means the split leaked a
  change and must be reverted, not documented.
- `uploadToGpu()` remains the sole GPU-upload gate; the GL-owning-lifetime rule (0012) unchanged.
- ADR-0007, 0008, 0009, 0010 and 0011 are **not** amended by this chain — ADR-0012 is additive.

## Verification

- Clean dev + release build, 0 new warnings.
- **ctest 21/21 dev + release.**
- `scripts/m5-close-matrix.sh` ALL PASS in both build dirs (dev + `--release`).
- The 0024/0033 CLI exit-code matrices unchanged (`--list-assets` 0; invalid flag / `--frames=-1` /
  `--frames=abc` / `--asset=` / `--asset`+`--demo` all 1; GL entry modes 0 with `--release`).
- Link graph recorded; `noto3d-registry-tests` and the `--list-assets` path show **no** GL.
- `xprop` `WM_NAME` titles unchanged for the three entry modes
  (`m4_vertical_scene | noto3d (M4)`, `crown [placeholder] | noto3d (M4)`,
  `demo_checker [placeholder] | noto3d (M4)`).

**Rolls under spec:** ADR-0012 ②/③ — library shape and its priced consequences.
