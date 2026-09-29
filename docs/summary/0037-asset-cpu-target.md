# Execution Summary: 0037-asset-cpu-target

- **Date**: 2026-09-29
- **Spec/ADR Path**: `docs/adr/0012-asset-cpu-gpu-boundary.md` ② (library shape)
- **Ticket Path**: `docs/archive/ticket/0037-asset-cpu-target.md` (complete; archived at close of the
  0036–0039 chain, per 0039)
- **Fixed point for review**: HEAD `840ea40`
- **Branch**: `main`

---

## 📌 Overview

- **Commits**: 0 new (working tree, uncommitted per implement skill `/commit` disabled)
- **Files changed**: 7 (2 new: `src/asset/asset_cpu.{hpp,cpp}`; 3 code/CMake edits; 3 docs)
- **Behaviour change**: **none** — a relocation ticket. `AssetRegistry`'s public API is
  byte-for-byte unchanged and ctest 21/21 passes without a test edit.
- **Ticket status**: ready-for-agent → **complete** (evidence recorded in the ticket).

## 🎯 What it delivered (ADR-0012 ② — tracer bullet 2)

The CPU asset core now lives in its own GL-free CMake target, `noto3d_asset_cpu`, and
`noto3d_asset` is a facade over it.

**Moved out of the GPU facade** (`src/asset/asset_cpu.{hpp,cpp}`): `CpuObj`; `isUnsafePath()` — the
ADR-0007 path predicate, now with **a single definition shared by both sides of the split**;
`parseManifest()`; `appendCpuDiagnostics()`; `scanUnsafeTextureRefs()` (ticket 0013);
`aggregateResults()`; and `CpuCache`, the canonical-path dedup (including the deliberate
*do-not-cache-FAILED* rule, so a later load re-diagnoses rather than replaying a stale failure).

**Kept in `noto3d_asset`**: `uploadToGpu()` — still the single GPU-upload gate — the GPU dedup
caches, the `external*` members, and the per-asset status bookkeeping. The `AssetRegistry` header
diff touches only the include list, the relocated `CpuObj`, one private cache member, and the
removed private `loadCpuRecord()`. `CpuObj` stays in namespace `noto3d::asset`, so existing
consumers still compile.

**One authority, two libraries**: the registry still owns *both* the CPU and the GPU caches. The
split relocated code into a library; it did not create a second authority (ADR-0007).

## 🧱 Why the boundary is real, not just asserted

ADR-0012 ① keeps `Vertex`/`Material` as `noto3d::renderer` types in `src/renderer/`, which the CPU
parse must include — so the ticket's literal "must not resolve `noto3d_renderer` headers" was
unsatisfiable. The invariant was restated at its intent (*no GL header, no GPU-owning header*) and
enforced three ways:

1. glad's include dir is **absent** from the CPU target, because `noto3d_core` (which exports
   `lib/glad/include` PUBLIC) is neither linked nor on its include path — `spdlog` is linked
   directly instead.
2. A configure-time **link guard** rejects `noto3d_renderer` / `noto3d_core` / `noto3d_scene` /
   `noto3d_ui` / `glad` / `OpenGL::GL` / `imgui` / `glfw` / `GLFW`.
3. A configure-time **include-closure guard** runs the preprocessor over each CPU source and
   rejects any GL/windowing header, and any `src/renderer/*.hpp` outside the
   `{vertex.hpp, material.hpp}` **allowlist**. An allowlist, not a denylist: a newly added renderer
   header fails the build until someone consciously classifies it.

Both guards derive their sources and include dirs from the target's own CMake properties.

## 🐞 The bug the code review caught

The CPU target did **not** link `glm`, so it resolved a **system glm 0.9.9** while the project pins
`lib/glm` **1.0.1** — an ODR split across the very boundary this ticket creates, with the 32 B
`Vertex` static_assert validated against the wrong glm, silently bypassing ADR-0003/0010. It was
*hidden* because the first version of the closure guard hand-wrote its own `-I` list, which had
drifted from the real build. Two fixes: link `glm::glm` (PUBLIC), and derive the probe's include
dirs from the target graph so they cannot drift again.

A second regression was caught by re-running the negative tests after the rewrite: the GL check had
been changed to compare *basenames* against *path* entries, so a direct `#include <glad/gl.h>`
(basename `gl.h`) slipped through. GL headers are now matched as path substrings.

## ✅ Verification

| Check | Result |
| --- | --- |
| dev + release build | 0 new warnings (only the pre-existing third-party tinyobjloader deprecation) |
| `ctest` dev / release | **21/21** / **21/21** |
| `m5-close-matrix.sh` dev / release | **ALL PASS** / **ALL PASS** |
| `--list-assets` | exit 0, `count=6`, `ldd` shows no `libGL` |
| 0024/0033 CLI exit codes | unchanged (5 cases → 1, `--list-assets` → 0) |
| `nm -C libnoto3d_asset_cpu.a` | **0** `gl*`/`glad` symbols; only `Vertex` + `Material` of `noto3d::renderer`; `Mesh`/`Model`/`Texture`/`SubMesh`/`ShaderProgram`/`Camera` → 0 |
| target graph (`--graphviz`) | → `nlohmann_json` + `glm` (PUBLIC), `spdlog` + `tinyobjloader` (PRIVATE); no renderer/core/glad |

**Both guards proven to fire** — a guard never seen red is not a guard:

| Injected violation | Result |
| --- | --- |
| re-add `noto3d_core` to the CPU target | link guard aborts |
| `#include <glad/gl.h>` | closure guard aborts, names `glad/gl.h` |
| `#include <mesh.hpp>` | closure guard aborts on the transitive `glad/gl.h` |
| brand-new `src/renderer/brand_new_gpu_thing.hpp` | allowlist aborts — **with no `CMakeLists.txt` edit** |

## 📝 Deviations (all deliberate, recorded in ADR-0012 + the ticket)

1. CPU target also links `glm::glm` (PUBLIC) and `spdlog::spdlog` (PRIVATE) — both forced; neither
   is GL or renderer, so the decision's actual property holds.
2. `RegistryResult` aggregation split: `aggregateResults()` moved (GL-free, so ③'s CLI path needs no
   facade); per-asset status bookkeeping stayed, because in `Mode::Full` a record's status depends
   on whether its `uploadToGpu()` succeeded.
3. The include-set invariant was unsatisfiable as written; restated at its intent (see above).
4. `PRIVATE tinyobjloader` dropped from `noto3d_asset` (now a PRIVATE dep of the CPU target);
   no consumer-visible link removed.

## 🔜 Next

- **0038** — retarget `tests/asset` + the `--list-assets` path to build against `noto3d_asset_cpu`
  and prove they no longer link GL. `aggregateResults()` is the piece 0038 needs for a GL-free CLI
  result.
- **0039** — close-out: target-graph record, full regression, docs.
