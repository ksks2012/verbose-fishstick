# 0041 — create `noto3d_serialization` target (GL-free boundary + proven-fires arch guard)

**Source:** ADR-0013 Q11; PRD user stories 9, 10, 11 — tracer bullet 1.
**Status:** complete (2026-09-30) — evidence recorded below
**Blocked by:** — (nothing blocks this)
**Parallel with:** 0042 — the two are independent and may run concurrently, but **completing only
this one does not unblock 0043**: 0043 is gated on *both*, so if one of the pair slips the other's
"green first" value drops and the frontier stalls at 0043 rather than advancing.
**Related:** ADR-0013, `docs/adr/0008-m0-code-contract.md:11-13,20` (the namespace/module/test dir
reserved at M0), `docs/archive/ticket/0037-asset-cpu-target.md` and
`docs/archive/ticket/0038-asset-cpu-no-gl-link.md` (the guard discipline this ticket reuses),
`CMakeLists.txt:218-236,260-...` (the existing link + include-closure guards)

## What it delivers

A new CMake target **`noto3d_serialization`** over `src/serialization/` — the module directory
ADR-0008 reserved at M0 and that has been empty ever since. It links **`noto3d_scene` (PUBLIC) +
`nlohmann_json` (PRIVATE) and nothing else**, plus a configure-time **architecture guard** that
makes
it *impossible* for the serializer to reach the asset registry, the renderer, or GL. This is the
foundation every later M6 ticket stands on: Q3's "strict structure / lenient references" split is
only structural if the serializer physically cannot resolve assets, so the guard is the point of the
ticket, not a formality.

No Scene projection, no CLI, no file I/O yet — this ticket delivers the boundary and proves it
holds.

## Scope

- `src/serialization/scene_io.hpp` / `src/serialization/scene_io.cpp` in namespace
  `noto3d::serialization` — a minimal, real translation unit so the target and its guard have
  something to compile and police. Keep it to boundary plumbing (e.g. the document format-version
  constant as a declaration in the header and a definition in the `.cpp`, per ADR-0008's
  declaration-only-header rule). Do **not** sketch the serialize/deserialize API here; that is 0043
  and 0044.
- New target in `CMakeLists.txt`, wired as:

  ```cmake
  add_library(noto3d_serialization STATIC src/serialization/scene_io.cpp)
  target_include_directories(noto3d_serialization
      PUBLIC  ${CMAKE_SOURCE_DIR}/src/serialization
              ${CMAKE_SOURCE_DIR}/src/scene)
  target_link_libraries(noto3d_serialization
      PUBLIC  noto3d_scene
      PRIVATE nlohmann_json::nlohmann_json)
  noto3d_set_warnings(noto3d_serialization)
  noto3d_apply_sanitizers(noto3d_serialization)
  ```

  `src/scene` is PUBLIC so a consumer of the serializer sees `scene.hpp`; `nlohmann_json` is PRIVATE
  so the JSON type never leaks into a consumer's headers. **Mirror the existing
  `target_include_directories` style in this `CMakeLists.txt`** rather than inventing a shape — note
  the neighbours differ: `noto3d_asset_cpu` aligns `PUBLIC`/`PRIVATE` (`:208-211`) while
  `noto3d_scene` and `noto3d_app` put a single dir on one line (`:454-455`, `:481-482`). Either
  matches house style; just be consistent with the target you are editing near.
- **Guard 1 (link)** — mirror `CMakeLists.txt:218-236`: `noto3d_serialization` must not link
  `noto3d_asset`, `noto3d_asset_cpu`, `noto3d_renderer`, `noto3d_ui`, `noto3d_core`, `glad`,
  `OpenGL::GL`, `imgui`, `glfw`. `noto3d_core` is forbidden for the same concrete reason as in 0037:
  it PUBLIC-exports `lib/glad/include`. Inputs must be read from the target's **own** CMake
  properties (`get_target_property(... LINK_LIBRARIES)`), never a hand-written list.
- **Guard 2 (includes)** — reuse the existing
  `noto3d_assert_target_reaches_no_gl(_target _label)` function (`CMakeLists.txt:260-...`) on
  `noto3d_serialization`: no TU may reach a GL/windowing header, directly or transitively. Extend
  its
  forbidden-project-header intent to this target as well: no `src/{asset,renderer,ui}/*.hpp` outside
  any legitimate need (there should be none — the serializer sees only `scene`). If the shared
  function cannot express the project-header part, add the check incrementally rather than
  hand-rolling a parallel one.
- **Guard 3 (proven to fire)** — the guards must be shown red, not merely present: temporarily add a
  forbidden link library → configure aborts naming it; temporarily `#include <glad/gl.h>` and
  `#include "asset_registry.hpp"` → closure guard aborts. Record the evidence in the delivered
  section.

## Invariants

- `noto3d_scene` is the only project library the serializer links. In particular it does **not**
  link `noto3d_core` — so glad's include directory is absent from its closure by construction.
- The guards derive their inputs from the target's own CMake properties, so they cannot drift from
  the build they police (the 0037 lesson: a hand-written `-I` list once hid a live system-glm-vs-
  pinned-glm ODR bug).
- No existing target's link line or behaviour changes. This is an additive ticket.
- ctest stays green at its current count; no new test is required here beyond the guard's
  red-green demonstration (which is configure-time evidence, recorded in the ticket).

## Explicitly NOT in scope

- Any serialize/deserialize function (0043/0044), any file I/O (0045), any CLI (0045).
- Reusing `asset::RegistryResult` / `LoadStatus` — forbidden by the guard; the serialization-local
  result vocabulary is introduced by 0043/0044.
- Widening any existing guard's allowlist, or adding `src/serialization/` headers to the CPU
  include-closure allowlist. The serializer is a *consumer* of `noto3d_scene`, not part of the asset
  CPU core.

## Verification

- Clean dev + release configure/build, 0 new warnings.
- ctest green (currently **22/22**) in dev + release — unchanged, since this ticket adds no runtime
  behaviour.
- `scripts/m5-close-matrix.sh` ALL PASS in both build dirs (unchanged).
- **Guard 1 proven to fire**: re-adding any forbidden library to `noto3d_serialization` aborts
  configure with the guard's message.
- **Guard 2 proven to fire**: a GL header or an `src/asset/*.hpp` include in `scene_io.cpp` aborts
  configure, naming the offending header.
- `--graphviz` (or the generated link line) shows `noto3d_serialization → noto3d_scene` and no
  `noto3d_asset*` / `noto3d_renderer` / `noto3d_core` / `glad` / `libGL`.

**Rolls under spec:** ADR-0013 Q11; PRD Testing Decisions ("the arch guard is itself a test").

## Delivered (2026-09-30)

**Target** — `noto3d_serialization` (STATIC, `src/serialization/scene_io.cpp`) at
`CMakeLists.txt:509-523`; single-dir-per-line `target_include_directories` matching the
`noto3d_scene` convention; `PUBLIC noto3d_scene` + `PRIVATE nlohmann_json`; wrapped in
`noto3d_set_warnings` + `noto3d_apply_sanitizers`.

**Translation unit** — `scene_io.hpp` declares `extern const int kSceneFormatVersion`;
`scene_io.cpp` defines it (ADR-0008 declaration-only rule). `nm -C` confirms external linkage
(`R noto3d::serialization::kSceneFormatVersion`), so a consumer of the header in 0043 will not hit
an undefined-symbol link error.

**Guard 1 (link)** — forbids `noto3d_asset`, `noto3d_asset_cpu`, `noto3d_renderer`, `noto3d_ui`,
`noto3d_core`, `glad`, `glad2`, `OpenGL::GL`, `imgui`, `glfw`, `GLFW`; inputs are read back from
the target's own `LINK_LIBRARIES` so the check cannot drift from the wiring.

**Guard 2 (includes)** — the shared `noto3d_assert_target_reaches_no_gl()` was generalised with a
`POLICY` keyword so one function serves both boundaries instead of growing a per-target twin:
`interchange` (default, unchanged for `noto3d_asset_cpu` / `noto3d-registry-tests`) and `closed`
(`src/renderer/` + `src/asset/` + `src/ui/`, nothing permitted). `noto3d_serialization` uses
`POLICY closed`. Guard messages now cite the caller's own ADR (`ADR-0013 Q11` here) instead of
hard-coded `ADR-0012`.

### Defect found and fixed while implementing

The new project-header check **did not fire on the first attempt** — the first proof run passed
silently. Cause: `-M` echoes a relative include exactly as written, so the closure contained
`src/serialization/../asset/manifest_types.hpp` and the `/src/asset/` prefix match never matched.
This is the same un-normalized-path trap the existing comment records for `glad/gl.h` vs `gl.h`.
Fixed by normalising each closure path to absolute before matching, which also closes the
equivalent `../renderer/…` bypass on the pre-existing `interchange` policy.

### Guard proofs (each aborts configure)

1. `noto3d_core` added to `target_link_libraries` → `noto3d_serialization links 'noto3d_core'`
2. `#include <glad/gl.h>` → `reaches the GL/windowing header 'glad/gl.h'`
3. `#include "../asset/manifest_types.hpp"` → `reaches 'manifest_types.hpp' under src/asset/`
4. `#include "../ui/dock_layout.hpp"` → `reaches 'dock_layout.hpp' under src/ui/`

All four cite `(ADR-0013 Q11)`. Cases 3 and 4 are the *closed*-policy coverage that the first
implementation lacked.

A bonus fifth case fell out: `#include "../ui/editor_state.hpp"` (a header that does not exist) is
caught by the existing "could not compute the include closure" branch, so the guard **fails closed**
on an unresolvable include rather than passing.

### Verification

- Clean dev + release configure/build from scratch: **0 warnings, 0 errors** in both.
- `ctest`: **22/22** dev, **22/22** release (unchanged, as expected — no runtime behaviour added).
- `scripts/m5-close-matrix.sh`: **ALL PASS** in both build dirs.
- `--graphviz`: `noto3d_serialization → noto3d_scene` and `→ nlohmann_json` rendered **dotted**
  (PRIVATE, correctly not exported); **0** edges from any `noto3d_asset*` / `noto3d_renderer` /
  `noto3d_core` / `glad` node into the serializer.
- `ar t` → one member (`scene_io.cpp.o`); `nm -C` → **0** GL/glad/OpenGL symbols.

**Out-of-scope respected** — no serialize/deserialize, no CLI, no reuse of `asset::RegistryResult` /
`LoadStatus`, no existing guard allowlist widened, no `src/serialization/` header added to the CPU
allowlist.

**Not done here (by design)** — `kSceneFormatVersion` is the only symbol; the document
vocabulary, the save/restore entry points, and the editor wiring are 0042–0044.

