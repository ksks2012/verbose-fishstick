# 0038 — point `tests/asset` and the `--list-assets` CLI at `noto3d_asset_cpu` (stop linking GL)

**Source:** ADR-0012 ③ (consequences) — tracer bullet 3.
**Status:** ready-for-agent
**Blocked by:** 0037 (`noto3d_asset_cpu` must exist)

## What it delivers

The concrete win the 0027 spike was opened to price: **the CPU-only asset test target and the
`--list-assets` diagnostic path no longer link GL at all.**

Today `tests/asset/registry_test.cpp` PUBLIC-links `noto3d_asset` → `noto3d_renderer` → `glad`,
even though **every** case in it exercises a CPU-only path (`loadManifestOnly()` /
`loadModelShapesOnly()`, asserting `model()==nullptr` / `externalModel()==nullptr`). 0025⑧ recorded
that reach as "expected today (0027)". This ticket ends it.

- `noto3d-registry-tests` links `noto3d_asset_cpu` (+ its test-only deps), not `noto3d_asset`.
- The `--list-assets` CLI path (0017, ADR-0009) runs against the CPU core so the pre-GL
  diagnostic is GL-free by construction, not merely "before a GL context exists" at runtime.

Both must be **behaviourally identical** — the tests must pass unchanged (same assertions, same
suite count, same CTest names), and `--list-assets` must still print the same manifest listing and
exit 0 without ever touching GL.

## Scope

- `CMakeLists.txt`: retarget `noto3d-registry-tests` to `noto3d_asset_cpu`. Its test sources stay
  where they are unless a case genuinely needs the facade — if any case needs a GPU-only entry
  point, split that case into the GL-linked test target rather than re-linking the whole suite to
  GL (a CPU suite that links GL is what this ticket removes).
- The `--list-assets` command (`src/app/editor_entry.cpp::runListAssetsCommand`, `editor_entry.cpp:28`)
  resolves its manifest listing through the CPU core. It is already a pre-GL path; this makes the
  *link* match the runtime guarantee. The `M3_ASSETS version=1` / `end count=6` output is unchanged.
- The `noto3d-editor` executable keeps linking the full GPU facade — the editor needs it. Only the
  CPU-only test/CLI paths shed the GL edge.

## Invariants

- **The registry test suite's assertions, CTest names and suite count are unchanged** (21/21
  dev + release). This is a link-graph change, not a test change.
- `--list-assets` output is byte-identical: `M3_ASSETS version=1`, the per-asset lines, `end
  count=6`, exit 0. It still never creates a GL context (it did not before either; now it does
  not even link GL).
- `noto3d-editor` and `noto3d_app` link graphs are **unchanged** (the editor genuinely needs the
  GPU facade; `noto3d_app` keeps its `collectDrawItems` seam).
- No new GL reach is introduced anywhere.

## Verification

- Clean dev + release build, 0 new warnings.
- **ctest green (21/21 dev + release)**, with `noto3d-registry-tests` now linking no GL.
- **Link-graph proof:** the `noto3d-registry-tests` link line does not contain `noto3d_renderer`,
  `glad`, or `libGL` (assert in the ticket, e.g. via `cmake --graphviz` or inspecting the
  generated link command). This is the ticket's headline acceptance.
- `--list-assets` still exits 0 with `count=6` and no GL context (run in a build dir; no display
  needed).
- `scripts/m5-close-matrix.sh` ALL PASS in both build dirs.

**Rolls under spec:** ADR-0012 ③ — priced consequence: the CPU path links no GL after the split.
