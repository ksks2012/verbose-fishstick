# 0037 — create `noto3d_asset_cpu` target (parse/dedup core, no GL, no renderer)

**Source:** ADR-0012 ② (library shape) — tracer bullet 2.
**Status:** ready-for-agent
**Blocked by:** 0036 (the CPU headers must be GL-free before the target is meaningfully GL-free)

## What it delivers

A new CMake target **`noto3d_asset_cpu`** holding the entire CPU asset core — `manifest_types.hpp`,
`obj_loader.{hpp,cpp}`, `CpuObj`, the OBJ/MTL parse + canonical-path dedup, the manifest entry /
diagnostic / `RegistryResult` aggregation. It links **`nlohmann_json` (PUBLIC) + `tinyobjloader`
(PRIVATE) only** — no `noto3d_renderer`, no `glad`, no `libGL`.

`noto3d_asset` (unchanged name, unchanged public contract) keeps `AssetRegistry` as a **facade
over** `noto3d_asset_cpu`: it PUBLIC-links it, keeps `uploadToGpu()` + the GPU dedup caches
(`m_modelCache`, `m_textureCache`), and keeps **all six** existing `AssetRegistry` entry points
byte-for-byte (`load` / `loadManifestOnly` / `reload` / `loadModelShapesOnly` / `loadModelFile` /
`model`) plus `externalModel()`, `findEntry()`, `entries()`, `assetStatus()`, `diagnostics()`,
`uniqueGeometryCount()`.

`AssetRegistry` remains the **sole asset authority** (ADR-0007) and remains the **sole owner of both
the CPU and the GPU caches** — the split relocates code into a library, it does not create a second
authority. No `Model*` lifetime changes (ADR-0010 Q5 borrowed frame-local pointer; registry caches
own the `Model`).

## Scope

- New target in `CMakeLists.txt` (`add_library(noto3d_asset_cpu STATIC …)`): sources
  `src/asset/manifest_types.hpp`, `src/asset/obj_loader.{hpp,cpp}`, and the parse/dedup helpers
  moved out of `asset_registry.{hpp,cpp}`. PUBLIC `nlohmann_json`; PRIVATE `tinyobjloader`.
  Include dirs wired so the CPU target cannot accidentally see renderer headers (guard the
  boundary — see Invariants).
- `noto3d_asset` gains `PUBLIC noto3d_asset_cpu`; keeps its existing PUBLIC
  `noto3d_renderer` + `nlohmann_json` and PRIVATE `tinyobjloader`. Nothing a consumer of the GPU
  facade already links is removed.
- Move `CpuObj`, `loadCpuRecord()`, and the per-path parse/dedup + entry/diagnostic aggregation
  into the CPU target. `AssetRegistry` keeps a `shared_ptr<const CpuObj>` handle per canonical
  path — the cache-key discipline (ADR-0007 dedup by canonical geometry path) is byte-identical.
- `loadManifestOnly()` and `loadModelShapesOnly()` keep their signatures and their "CPU-only,
  `model()`/`externalModel()` return null" behaviour (0025③ locked them as public production API);
  they are now thin CPU-only paths through the facade that never call `uploadToGpu()`.

## Invariants

- **`AssetRegistry` public API is unchanged** — every entry point and accessor keeps its exact
  signature and semantics. This is a *relocation* ticket, not an API ticket.
- **`uploadToGpu()` remains the single GPU-upload gate** and remains on the GPU facade. No new GL
  call site is introduced anywhere.
- **`CanonicalShape::material.texture` is always `std::nullopt`** on the CPU path (ADR-0012 ①) —
  the CPU parse writes only `diffuseTextureRef`; `uploadToGpu()` is the only writer of `texture`.
- The GL-owning-lifetime rule (ticket 0012: destroy GL objects while the context is current) is
  unchanged — every `Mesh`/`Texture` is still owned by the registry local in `renderScene()`.
- The 0013 `diffuseTextureRef` path predicate and the 0008 absolute-path-trusted escape-hatch
  contract apply identically on both sides of the split.
- The CPU target's include set must not resolve `noto3d_renderer` headers (enforced by include-dir
  wiring, not just convention).

## Verification

- Clean dev + release build, 0 new warnings.
- **ctest green (21/21 dev + release)** — including the `noto3d-registry-tests` suite, which
  exercises the relocated CPU parse code (all its cases go through `loadManifestOnly()` /
  `loadModelShapesOnly()` and must still pass unchanged).
- `scripts/m5-close-matrix.sh` ALL PASS in both build dirs; `--list-assets` still exits 0 with
  `count=6` and still creates no GL context.
- The 0024/0033 CLI exit-code matrices unchanged.
- Static check: `noto3d_asset_cpu` does not link `noto3d_renderer` / `glad`
  (e.g. `grep noto3d_asset_cpu` in the generated link line, or `cmake --graphviz`).

**Rolls under spec:** ADR-0012 ② — library shape; ① (type ownership) and ①b (lifetime) hold as
documented.
