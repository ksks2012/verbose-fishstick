# 0039 — asset CPU/GPU split close-out: full graph verification + regression re-run

**Source:** ADR-0012 — tracer bullet 4, and the close of the 0027 spike chain (0036–0039).
**Status:** complete (2026-09-29) — evidence recorded below
**Blocked by:** 0038 (satisfied)

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
  change and must be reverted, not documented. **Held** — see "Preserved M4 contracts" and
  "Evidence" below: the chain touched no M4 machinery file, the `public:` section of
  `AssetRegistry` is byte-identical, and every re-run observation matches.
- `uploadToGpu()` remains the sole GPU-upload gate; the GL-owning-lifetime rule (0012) unchanged.
  **Held** — `src/asset/` still has no direct GL call.
- ADR-0007, 0008, 0009, 0010 and 0011 are **not** amended by this chain — ADR-0012 is additive.
  **Held** — only ADR-0012 was touched; its amendments are 0037 (library shape) and 0038 (the
  header/TU split), both recorded in place.
- **Amended (0038, carried here): the suite is 22/22, not 21/21.** 0038 moved `reload()`'s two
  cases into a GL-linked target of their own and added one CTest entry, plus the
  `testGpuDedupOrderingKeepsFallbackWarning` assertions. No pre-existing assertion or CTest name
  was removed. This ticket's own Verification line below inherited the stale 21/21 and is
  corrected to 22/22.
- **Amended (0037, carried here): the Scope line's "json + tinyobjloader only" is superseded.**
  ADR-0012 ②'s 0037 amendment added `glm::glm` (PUBLIC) and `spdlog::spdlog` (PRIVATE) to
  `noto3d_asset_cpu`, and `aggregateResults()` to its contents. The property the decision turns on —
  no `noto3d_renderer`, no `noto3d_core`, no `glad`, no `libGL` — is unchanged and is what the
  recorded graph above shows.

## Verification

- Clean dev + release build, 0 new warnings. **PASS** (fresh build dirs, 0 warnings under
  `-Wall -Wextra -Wpedantic`).
- **ctest 22/22 dev + release.** **PASS** (was written as 21/21; see the amendment above).
- `scripts/m5-close-matrix.sh` ALL PASS in both build dirs (dev + `--release`). **PASS**.
- The 0024/0033 CLI exit-code matrices unchanged (`--list-assets` 0; invalid flag / `--frames=-1` /
  `--frames=abc` / `--asset=` / `--asset`+`--demo` all 1; GL entry modes 0 with `--release`).
  **PASS**.
- Link graph recorded; `noto3d-registry-tests` and the `--list-assets` path show **no** GL. **PASS**.
- `xprop` `WM_NAME` titles unchanged for the three entry modes
  (`m4_vertical_scene | noto3d (M4)`, `crown [placeholder] | noto3d (M4)`,
  `demo_checker [placeholder] | noto3d (M4)`). **PASS** (executed; see the tier-B table above).

**Rolls under spec:** ADR-0012 ②/③ — library shape and its priced consequences.

---

## Recorded final target graph (2026-09-29)

Read out of CMake itself, not copied from the ADR: a `cmake_language(DEFER …)` probe writing
`LINK_LIBRARIES` / `INCLUDE_DIRECTORIES` / `COMPILE_DEFINITIONS` per target, run against a fresh
configure. Visibility keywords are as declared in `CMakeLists.txt`.

| target | type | PUBLIC | PRIVATE | include dirs | verdict |
| --- | --- | --- | --- | --- | --- |
| `noto3d_asset_cpu` | STATIC | `nlohmann_json::nlohmann_json` `glm::glm` | `tinyobjloader` `spdlog::spdlog` | `src/asset`, `src/renderer`, `src/core` | **no GL, no renderer, no `noto3d_core`** |
| `noto3d_asset` | STATIC | `noto3d_asset_cpu` `noto3d_renderer` `nlohmann_json::nlohmann_json` | — | `src/asset` | facade; adds exactly one new edge (the CPU core) |
| `noto3d_renderer` | STATIC | `noto3d_core` `glm::glm` `glad` | `stb_header` | `src/renderer` | unchanged |
| `noto3d_core` | STATIC | `spdlog::spdlog` | — | `src/core`, **`lib/glad/include`** | unchanged; this is the glad-include leak the 0037 amendment routes around |
| `noto3d_app` | STATIC | `noto3d_scene` `noto3d_renderer` `noto3d_asset` | — | `src/app` | unchanged |
| `noto3d-registry-tests` | EXE | — | `noto3d_asset_cpu` `noto3d_core` | (defs: `NOTO3D_ASSETS_DIR`) | **GL-free** |
| `noto3d-registry-reload-tests` | EXE | — | `noto3d_asset` `noto3d_core` | (defs: `NOTO3D_ASSETS_DIR`) | GL-linked **by design** (0038) |
| `noto3d_scene` | STATIC | `glm::glm` | — | `src/scene` | unchanged |
| `noto3d_ui` | STATIC | `noto3d_app` | `glfw` `glad` `glm::glm` `imgui` | `src/ui` | unchanged |
| `noto3d-editor` | EXE | — | `glfw` `glad` `glm::glm` `imgui` `noto3d_ui` `noto3d_app` `noto3d_asset` | (defs: `NOTO3D_SHADER_DIR`, `NOTO3D_ASSETS_DIR`, `NOTO3D_DEBUG_GL`) | unchanged |

**Scope of the table.** These are the 10 targets in the transitive closure of the asset boundary —
every target reachable from `noto3d_asset_cpu` and every target that reaches it. The build defines
23 targets in total; the rest are the `tests/asset`, `tests/core`, `tests/renderer`, `tests/scene`
and `tests/app` executables, the vendored third-party targets (`glad`, `glm::glm`, `spdlog`,
`nlohmann_json`, `tinyobjloader`, `stb_header`, `imgui`, `glfw`) and the demo `_demo` fixture. None
of them is in the asset CPU/GPU dependency chain, and none was touched by the chain.
`noto3d_scene` and `noto3d_ui` are listed even though the split did not involve them, so that
"`noto3d_app` / `noto3d-editor` unchanged" is **shown here** rather than merely asserted.

The graph is exactly what ADR-0012 ② specifies, including the 0037 amendment: `noto3d_asset_cpu`
resolves `glm` explicitly (so the CPU half cannot silently bind the system glm 0.9.9 instead of the
pinned `lib/glm` 1.0.1 — an ODR split across the boundary) and links `spdlog` **directly** rather
than via `noto3d_core`, because `noto3d_core` exports `lib/glad/include` PUBLIC and would put glad's
headers back on the CPU target's include path.

**Reading the CPU test's link line correctly.** `noto3d-registry-tests` resolves
`libnoto3d_asset_cpu.a libnoto3d_core.a libglm.a libtinyobjloader.a libspdlog.a`. The
`tinyobjloader` / `spdlog` entries are CMake's `$<LINK_ONLY:>` propagation of `noto3d_asset_cpu`'s
PRIVATE deps, which static linking requires; they carry no include dirs and no defines. The
meaningful assertion is the absence of `noto3d_renderer`, `glad` and `libGL` — which is also what
the configure-time guards enforce, so it cannot rot silently.

## Preserved M4 contracts — re-confirmed

Two tiers: **structural** (what the chain could not possibly have touched) and **behavioural**
(what was re-run).

### Structural: the chain never touched the M4 machinery

`git diff --stat 1a57a3f..` (the ADR-0012 decision commit → worktree) touches **13 files** — 12
under `src/`, `tests/`, `scripts/`, `assets/`, plus `CMakeLists.txt` itself. Every one is either the
asset CPU/GPU split or the 0036 `Vertex` relocation:

```
CMakeLists.txt                    src/asset/asset_cpu.{hpp,cpp}      (new, 0037)
src/app/editor_entry.cpp          src/asset/asset_registry.cpp
src/asset/obj_loader.hpp          src/asset/asset_registry.hpp
src/renderer/mesh.hpp             src/asset/asset_registry_cpu.cpp   (new, 0038)
src/renderer/texture.cpp          tests/asset/registry_test.cpp
src/renderer/vertex.hpp           tests/asset/registry_reload_test.cpp (new, 0038)
```

**`src/scene/`, `src/app/draw_list.{hpp,cpp}`, `src/app/editor.{hpp,cpp}` and `src/ui/` are not in
that list.** So the M4 contracts below are intact *structurally* — the split had no opportunity to
move them — not merely still passing a test:

- `scene::Scene` remains pure data: `name` + `assetId` strings, no `Model`/`Texture`/renderer
  reference (`src/scene/scene.hpp:43-44`).
- `app::collectDrawItems(scene, Resolver)` is still the single resolution seam, defined once in
  `src/app/draw_list.cpp:8`; the one call site is `src/ui/panel_render.cpp:53`.
- `DrawItem::model` is still `const renderer::Model*` — borrowed, frame-local, no ownership
  (`src/app/draw_list.hpp:24`), with the ADR-0010 Q5 lifetime contract restated verbatim.
- `AssetRegistry` remains the sole asset authority: the single change to `src/app/editor_entry.cpp`
  is confined to `runListAssetsCommand()`, which no longer constructs a registry at all. No entry
  mode, no title, and no resolver lambda was touched.

### Public API surface: byte-identical

Diffing the public declarations of `src/asset/asset_registry.hpp` between `1a57a3f` and the close
of the chain is **empty** — every line from `public:` up to (and including, unchanged) the
`private:` label is identical, and no private member is in the compared range:

```
for r in 1a57a3f HEAD; do
  git show $r:src/asset/asset_registry.hpp | sed -n '/^public:/,/^private:/p' \
    | grep -vE '^\s*//' | grep -vE '^\s*$' > /tmp/pub-$r.txt
done; diff /tmp/pub-1a57a3f.txt /tmp/pub-HEAD.txt   # empty
```

All six entry points survive with identical signatures — `load`,
`loadManifestOnly`, `reload`, `loadModelShapesOnly`, `loadModelFile`, `model` — plus `externalModel`,
`uniqueGeometryCount`, `findEntry`, `entries`, `diagnostics`, `assetStatus`. The only header change
on the private side is the intended one: `doLoad(path, mode)` became `doLoadFull` +
`doLoadManifestOnly` over a shared `runManifestPipeline`.

### Escape hatches and the path predicate: logic unchanged

- `isUnsafePath` moved from an anonymous-namespace helper in `asset_registry.cpp` to
  `asset_cpu.cpp` (0037) so the CPU library owns it. The body is identical; only the parameter
  narrowed `const std::string&` → `std::string_view` and it became `noexcept`:
  `p.find("../") != npos || (!p.empty() && (p[0] == '/' || p[0] == '\\'))`. It is applied at
  exactly the same three sites: manifest `geometry`, manifest `fallback_geometry`, and the 0013
  `diffuseTextureRef` scan.
- `src/asset/` still contains **no direct GL call** (`grep` for `glGenTextures` /
  `glGenVertexArrays` / `glDeleteTextures` / `glBufferData` → no match), so `uploadToGpu()` remains
  the sole GPU-upload gate and the 0012 GL-owning-lifetime rule is untouched.

## Evidence — full regression re-run

**Clean** dev + release build from scratch (fresh build dirs, not incremental), both 0 errors and
**0 warnings** under `-Wall -Wextra -Wpedantic`.

- `ctest`: **22/22** in both clean build dirs.
- `scripts/m5-close-matrix.sh`: **ALL PASS** on the clean dev dir, and **ALL PASS** on the clean
  release dir with `--release`.
- CLI exit-code matrix unchanged: `--list-assets` → 0; `--bogus`, `--frames=-1`, `--frames=abc`,
  `--asset=nope`, `--asset=nope --demo` → 1.
- `--list-assets` still creates **no GL context**: its output contains zero `M0_INFO` / `M0_OK` /
  `M1_OK` / shader lines. `M3_ASSETS version=1`, 6 asset lines, `end count=6`.
- Entry modes and structured log lines observationally identical:
  `M4_INFO rendering scene 'm4_vertical_scene' entities=3` (default),
  `M3_INFO selected asset 'crown' geometry=placeholder reference=…` +
  `M4_INFO rendering scene 'crown' entities=1` (`--asset=crown`),
  `M4_INFO rendering scene 'demo_checker' entities=1` (`--demo`).

### `xprop` `WM_NAME` titles (tier B, actually executed)

`m5-close-matrix.sh` keeps the `xprop` check in its manual tier B, so it was run by hand here — and
it passed for all three entry modes against the clean release build on a real display
(Weston, llvmpipe GL 4.5):

| mode | expected `WM_NAME` | result |
| --- | --- | --- |
| default | `m4_vertical_scene \| noto3d (M4)` | **PASS** |
| `--asset=crown` | `crown [placeholder] \| noto3d (M4)` | **PASS** |
| `--demo` | `demo_checker [placeholder] \| noto3d (M4)` | **PASS** |

Reproduction note, because two obvious approaches fail here: the box has **no EWMH window manager**
(`_NET_CLIENT_LIST` does not exist), and Weston **reparents** the GLFW toplevel, so it is not a
direct child of the root window. A working check walks the whole tree and selects the window whose
name contains `noto3d`:
`xwininfo -root -tree | grep -oE '0x[0-9a-f]+ "[^"]*noto3d[^"]*"' | xargs -0 xprop -id <id> WM_NAME`.
Matching the wrong window silently yields the literal string `WM_NAME: not found.`, which is not a
title and must not be accepted as one.

### GL-free proof (re-run on the clean build)

- `noto3d-registry-tests` link line: `libnoto3d_asset_cpu.a libnoto3d_core.a libglm.a
  libtinyobjloader.a libspdlog.a` — zero occurrences of `noto3d_renderer`, `glad` or `libGL`.
- `nm -C --defined-only` and `--undefined-only libnoto3d_asset_cpu.a`, filtered with
  `grep -E '\b(gl|glad_gl)[A-Z][A-Za-z0-9_]*\s*\('`: **0** and **0**. (Case sensitivity is
  load-bearing — see the 0038 note about `-i` matching `glm`.)
- The configure-time guards still bite: injecting `#include <glad/gl.h>` into
  `asset_registry_cpu.cpp` makes a fresh `cmake -S . -B …` **exit 1** naming the offending header.
  The file was restored byte-for-byte afterwards (`diff -q` clean).

## Close-out audit — residue found (filed as 0040, not fixed here)

The audit looked for unfinished work, refactor debt and warnings left by the chain. It found **no
warnings and no partial implementation**, but two ADR-0012 invariants that the ADR states as
contracts while nothing in the repository checks them. They are recorded in
**`docs/ticket/0040-asset-cpu-invariant-assertions.md`** rather than fixed here, because this
ticket is scoped "(ideally) no new code" and both fixes are new assertions — quietly adding them
would have widened the "zero observable change" claim this ticket exists to make.

1. **ADR-0012 ① — `CanonicalShape::material.texture` is always `std::nullopt`.** The ADR calls this
   "a stronger, *checkable* contract than 'usually null'", but `grep -rn 'nullopt' tests/ src/asset/`
   finds no assertion covering it, and `obj_loader.cpp` only ever records `diffuseTextureRef`. The
   invariant holds in practice and is verified by nothing. 0040 adds a real check, with an MTL
   fixture that declares `map_Kd` so the assertion cannot be vacuous.
2. **ADR-0012 ③ — the *consequences* of the GPU-dedup ordering, as distinct from the ordering
   rule.** 0038 pinned the rule itself (and caught a real regression against it), but not what it
   buys: fast-path hits counting toward `uniqueGeometryCount()` without a second parse, `Success`
   with no per-asset diagnostics, and `cacheKey` being the *canonical* key even when
   `fallback_geometry` resolved — the one case where the effective key differs from the manifest's
   `geometry` field.

**Audited and found clean, no ticket needed:**

- **Compiler warnings:** 0 in clean dev and clean release builds under `-Wall -Wextra -Wpedantic`.
  The only configure-time notice is a pre-existing `cmake_minimum_required` deprecation from the
  vendored `lib/tinyobjloader/CMakeLists.txt:5` — third-party, outside this chain's scope.
- **`renderer::Model` alias duplication** (deferred by 0038): left as-is, and 0040 records why.
  It is compile-safe — `asset_registry.cpp` includes `model.hpp`, so a divergent alias is a
  conflicting-declaration error in that TU, not silent drift. Removing it means adding a
  `src/renderer/model_fwd.hpp`, i.e. an ADR-0012 ① classification decision plus an amendment to the
  include-closure guard's `src/renderer/` allowlist.
- **Doc-hygiene gap, now closed:** the `CONTEXT.md` / `CHANGELOG.md` updates for **0036, 0037 and
  0038 were never committed** — they sat uncommitted in the working tree across three tickets,
  which breaks DOCUMENTATION-LIFECYCLE's "both are updated in the same change" rule for the ADR /
  CONTEXT pair. They are landed by this ticket's change.
- **Tier-B `xprop` is still unscripted** by design (0033 keeps it manual, platform-specific). The
  working incantation is recorded above. Scripting it is a 0033 concern, not an ADR-0012 one.
- **Pre-existing dead doc links, left alone (out of scope).** A sweep of every
  `docs/{ticket,summary}/NNNN-*.md` reference in `docs/` found **five** that name
  `docs/ticket/…` for a file already under `docs/archive/ticket/`:
  `docs/summary/0032-m5-entity-create-destroy.md:5`; `docs/adr/0003-dependency-strategy.md:40`
  (ticket 0001); `docs/CHANGELOG.md:458` and `:496` (ticket 0001); and
  `docs/archive/ticket/0025-m4-code-cleanup.md:272` (ticket 0027). All five are present at `HEAD`
  and none is an ADR-0012 artefact, so fixing them here would widen this ticket past its boundary.
  Recorded here so they are not rediscovered from scratch. (An earlier draft of this note
  mis-attributed these to the 0025/0027 *summaries*; `docs/summary/0025:5` is a legitimate
  "was `docs/ticket/…`" historical note and `docs/summary/0027:5` already points at the archive.)
