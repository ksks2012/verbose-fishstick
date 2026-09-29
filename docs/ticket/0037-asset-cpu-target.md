# 0037 — create `noto3d_asset_cpu` target (parse/dedup core, no GL, no renderer)

**Source:** ADR-0012 ② (library shape) — tracer bullet 2.
**Status:** complete (2026-09-29) — evidence below
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

  > **Amended during 0037 implementation.** As originally written this invariant is *unsatisfiable*
  > next to ADR-0012 ①, which requires `Vertex`/`Material` to stay `noto3d::renderer` types in
  > `src/renderer/` (same namespace, no rename/alias, no duplicate) — and the CPU parse names both.
  > Per the doc hierarchy the ADR wins, so the invariant is restated at its actual intent:
  > **the CPU target must resolve no GL header and no GPU-owning header.** That is enforced three
  > ways: (a) glad's include dir is absent because `noto3d_core` is neither linked nor on its
  > include path; (b) a configure-time link guard forbids `noto3d_renderer`/`noto3d_core`/`glad`/
  > `OpenGL::GL`/`imgui`/`glfw`; (c) a configure-time **include-closure** guard computes each CPU
  > source's transitive closure with the preprocessor and rejects both any GL/windowing header and
  > any `src/renderer/*.hpp` outside the `{vertex.hpp, material.hpp}` allowlist. The allowlist is
  > deliberately an allowlist, so a *newly added* renderer header fails the build until someone
  > consciously classifies it. Both guards derive their inputs from the target's own CMake
  > properties so they cannot drift from the build they police.

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

---

## Delivered (2026-09-29)

**New files:** `src/asset/asset_cpu.hpp` / `src/asset/asset_cpu.cpp` — `CpuObj`, `isUnsafePath()`
(ADR-0007 predicate, **one definition shared by both sides of the split**), `parseManifest()`,
`appendCpuDiagnostics()`, `scanUnsafeTextureRefs()` (0013), `aggregateResults()`, and `CpuCache`
(the canonical-path dedup, incl. the "do not cache FAILED" rule).

**`AssetRegistry` public API is byte-for-byte unchanged** — the header diff touches only the
include list, the relocated `CpuObj`, the private cache member and the removed private
`loadCpuRecord()`. `CpuObj` stays in namespace `noto3d::asset`, so existing consumers still compile.

### Deviations from this ticket's literal text (all deliberate, recorded in ADR-0012)

1. **The CPU target also links `glm::glm` (PUBLIC) and `spdlog::spdlog` (PRIVATE).** Both are
   forced: `Vertex`/`Material` are glm types (①), and `obj_loader.cpp` uses `NOTO3D_DEBUG`.
   spdlog is linked *directly instead of via `noto3d_core`* because `noto3d_core` PUBLIC-exports
   `lib/glad/include`, which would undo the whole split. The no-GL/no-renderer property — the
   decision's actual point — holds.
2. **`RegistryResult` aggregation split in two.** `aggregateResults()` is CPU-only and moved;
   the per-asset status bookkeeping stays in the facade because in `Mode::Full` a record's status
   depends on whether its `uploadToGpu()` succeeded.
3. **The include-set invariant** was unsatisfiable as written; restated at its intent and enforced
   three ways (see the Amendum above).
4. **`PRIVATE tinyobjloader` dropped from `noto3d_asset`** — `asset_registry.cpp` no longer reaches
   `tiny_obj_loader.h`; it is a PRIVATE dep of the CPU target. No consumer-visible link removed.

### Bug found and fixed by the code review

The CPU target did **not** link `glm`, so it resolved a **system glm 0.9.9** while the project pins
`lib/glm` **1.0.1** — an ODR split across the boundary, with the 32 B `Vertex` static_assert
validated against the wrong glm, silently bypassing ADR-0003/0010. It was hidden because the first
version of the closure guard hand-wrote its own `-I` list, which had drifted from the real build.
Both guards now derive their include dirs from the target's own CMake properties.

### Verification

- **Clean dev + release build, 0 new warnings** (only the pre-existing third-party tinyobjloader
  `cmake_minimum_required` deprecation notice).
- **ctest 21/21 dev + release** — `asset-registry-contract` (which exercises the relocated CPU
  parse code through `loadManifestOnly()`/`loadModelShapesOnly()`) passes unchanged.
- **`scripts/m5-close-matrix.sh` ALL PASS in both build dirs.**
- `--list-assets` exits 0, still logs `count=6`, and `ldd` shows **no `libGL`** on the binary.
- 0024/0033 CLI exit-code matrix unchanged: `--frames=-1` / `--frames=abc` / `--bogus` /
  `--asset=nope` / `--asset=nope --demo` → 1; `--list-assets` → 0.
- **Symbol-level proof of the split:** `nm -C libnoto3d_asset_cpu.a` → **0** `gl*`/`glad` symbols;
  the only `noto3d::renderer::` types present are `Vertex` and `Material`; `Mesh`/`Model`/
  `Texture`/`SubMesh`/`ShaderProgram`/`Camera` → 0.
- **Target graph** (`--graphviz`): `noto3d_asset_cpu` → `nlohmann_json` + `glm` (PUBLIC),
  `spdlog` + `tinyobjloader` (PRIVATE). No `noto3d_renderer`, no `noto3d_core`, no `glad`, no `glm`
  leak from `noto3d_core`, no `libGL`.
- **Both arch guards proven to fire** (a guard never seen red is not a guard): re-adding
  `noto3d_core` to the CPU target → link guard aborts; `#include <glad/gl.h>` → closure guard
  aborts naming `glad/gl.h`; `#include <mesh.hpp>` → closure guard aborts on the transitive
  `glad/gl.h`; and a **brand-new** `src/renderer/brand_new_gpu_thing.hpp` → allowlist aborts with
  **no CMakeLists edit**, confirming the allowlist is self-maintaining.
- `/code-review` run on both axes; the `glm` ODR bug, the `noto3d_core` hole in the link guard, the
  hand-written source list, the denylist-vs-allowlist trap and the partial aggregation all came
  from it and are fixed above.

