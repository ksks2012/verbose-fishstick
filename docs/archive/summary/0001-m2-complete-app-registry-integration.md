# Execution Summary: 0001-m2-complete-app-registry-integration

- **Date**: 2026-09-26
- **Spec/ADR Path**: `docs/adr/0007-asset-registry.md` (amended)
- **Ticket Path**: `docs/archive/ticket/0012-m1-app-renderer-separation.md` (+ 0009, 0010, 0011)
- **Branch**: `main` (uncommitted working tree vs HEAD `a30689a`)

---

## 📌 Overview
- **Commits**: 0 (implementation is an uncommitted working-tree diff vs HEAD)
- **Files Changed**: 14 files (`+597` / `-222`)

## 📝 Key Changes
* Closed the final three M2 tickets: **0012** (app/renderer separation + AssetRegistry
  integration), **0009** (E2E diffuse-texture gate), **0010** (dependency pinning).
* `noto3d-editor` now renders through the registry: `--asset=<id>` (default first renderable
  manifest asset) and `--demo` (0014 fixture, `M2_OK` log). 0011 folded items ⑦⑧⑨⑩ done
  (`#error` shader-dir, validated `--frames=`, inline cube/optional removed, uniform-location
  cache in `ShaderProgram`). GL-owning objects scoped so they are destroyed while the context is
  current (ASan SEGV fix).
* New standalone `AssetRegistry` load API (`loadModelShapesOnly`/`loadModelFile`/`externalModel`)
  with headless test `testStandaloneLoad()`; dedup, ADR-0007 path safety and the 0013 texture
  predicate shared with manifest loads via extracted `loadCpuRecord`/`appendCpuDiagnostics`/
  `scanUnsafeTextureRefs` helpers.
* 0010 verified: all six Git deps pinned by immutable SHA (each resolves to its release tag),
  `GIT_SHALLOW` retained, no archive sources → `URL_HASH` n/a.
* Docs synced: tickets 0012/0009/0010 archived (all 14 under `docs/archive/ticket/`), CONTEXT /
  README / CHANGELOG state M0–M2 complete, ADR-0007 amended for the standalone entry points.

## 🧪 Verification
- **Typecheck**: 🟢 Pass (`-Wall -Wextra -Wpedantic` clean, dev preset)
- **Tests**: 🟢 Pass — dev + release `ctest` 4/4; registry test ALL PASS
- **Runtime**: `--demo --frames=1` → exit 0 with `M2_OK` both dev + release; `--frames=abc`/
  `--frames=-1`/`--asset=nope` → exit 1 (with `ASAN_OPTIONS=detect_leaks=0` — Mesa/LSan driver
  leak in the dev build is environmental)

## 🔍 Code Review Findings
- **Standards**: findings addressed — duplicated load/diagnostic/0013-scan logic extracted to
  shared helpers; standalone path now mirrors `doLoad`'s unsupported-axis warning; ADR-0007 amended
  + CONTEXT synced for the standalone trusted-path escape hatch; dead `--frames=` catch line
  removed. Accepted as judged: `externalModel()` (mirror of `model()`, tested), `upAxis="y"`
  default, nullable `ShaderProgram` default ctor (parallels moved-from state).
- **Spec**: fully implemented — ⑦⑧⑨⑩, registry wiring, 0009 E2E (via 0014 fixture), 0010
  pinning+regression all present and verified. No scope creep (docs churn is the project's
  ticket/ADR/CHANGELOG discipline).

## 📦 Recent Commits
- `a30689a` docs(concentrate): add README.md and update CHANGELOG.md and CONTEXT.md for M2 asset pipeline progress
- `4b681ce` docs(concentrate): M0 to M2 ticket specifications for build environment, dependencies, and asset management
- `21a9579` feat(renderer): implement Renderer class with 3D pass management and submission functionality