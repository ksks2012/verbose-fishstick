# M0 — Build & environment baseline (spec)

Status: completed (M0 delivered — configure/build/verify green on dev, dev-clean, release;
ticket archived from active queue once M0 is done)
Type: task

> Spec for milestone M0, written per the to-spec format. ADRs (0001–0008) and
> `docs/DEFINITION.md` remain the authoritative decisions/vocabulary; this ticket carries the
> implementation scope, user stories, and the verification checklist. Checkboxes live here, not
> in ADRs.

## Problem Statement

The repo has an agreed architecture (ADRs 0001–0008), a dependency contract (ADR-0003: external
libs live in `lib/<name>`, cloned at configure time; glad + stb_image committed), and a bootstrap
`main.cpp`. But the environment has never reached a green configure+build: GLFW 3.4's X11
configure aborts until `libxinerama-dev`/`libxcursor-dev` are installed (Q9 machine
prerequisite), and the full build + dev verification has not been run. Developers need a
single-command, verified baseline before any M1 work.

## Solution

A reproducible build and environment baseline: one configure → build → run flow via CMake
presets; all pinned dependencies resolved into `lib/` (never `build/_deps`); a window/context
bootstrap that creates a 3.3 Core + sRGB + MSAA 4 context, loads GL through glad, and asserts the
capability **numerically**; and a **non-interactive** `--frames=N` verification mode that prints
`M0_OK`/`M0_FAIL` markers for automated checking. M1 renderer work starts only after the M0
acceptance matrix below is fully checked.

## User Stories

1. As a developer, I want to configure the whole project with `cmake --preset dev`, so that a
   fresh checkout reaches a buildable state with known options.
2. As a developer, I want the pinned third-party libs to resolve into `lib/<name>` via
   FetchContent `SOURCE_DIR`, so that the repo layout matches ADR-0003 (never `build/_deps`).
3. As a developer, I want glad and stb_image to remain committed under `lib/` as non-fetchable
   exceptions, so that the build stays reproducible without extra generator steps.
4. As a developer, I want a **loud, actionable configure failure** when X11 headers are missing,
   so that the Q9 machine-prerequisite step is obvious instead of a cryptic CMake assertion.
5. As a developer, I want GLFW + GLM + ImGui + nlohmann/json to build as part of the default
   build, so that no manual per-dependency step is needed.
6. As a developer, I want the dev preset to run Debug + optional ASan/UBSan + an optional debug
   GL context, so that M1 development gets traceable failures.
7. As a developer, I want the bootstrap to assert OpenGL 3.3 Core via
   `GL_MAJOR_VERSION`/`GL_MINOR_VERSION` + `GL_CONTEXT_PROFILE_MASK`, so that capability gating
   never depends on parsing `glGetString(GL_VERSION)` (ADR-0004/DEFINITION.md).
8. As a developer, I want context hints for sRGB, MSAA 4, and depth 24, so that the renderer
   baseline is in place from the first frame (ADR-0004/0005).
9. As a developer, I want the debug GL context used only in Debug builds, and only after
   verifying `GL_KHR_debug` exists, so that the debug callback is never assumed (ADR-0004).
10. As a developer, I want `noto3d-editor --frames=5` to run a bounded non-interactive
    verification loop and print `M0_OK`/`M0_FAIL` markers, so that verification is automatable
    without window interaction. (Requires a display server / window system, e.g. `DISPLAY=:0`;
    this is **not** offscreen/EGL headless.)
11. As a developer, I want `-Wall -Wextra -Wpedantic` on our own code with `-Werror` opt-in, so
    that warnings surface early (ADR-0008).
12. As a developer, I want logging through `noto3d::core::Log`, so that diagnostics are uniform
    from the first milestone (ADR-0008).

## Implementation Decisions

- Dependency wiring reuses the `../reimagined-meme` pattern: `FetchContent_Declare(... GIT_TAG ...
  GIT_SHALLOW TRUE SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/<name>)`. Pin set is fixed by ADR-0003
  (tags today; commit SHA + URL_HASH is a watch item, not this milestone).
- Builds use the "Unix Makefiles" generator via CMakePresets; no Ninja, no pkg-config.
  Presets: `dev` (Debug + sanitizer + debug-GL capability), `release` (optimized, no sanitizers).
  Sanitizers/debug-GL are options, not forced prerequisites (ADR-0008).
- The bootstrap executable owns: window → context → glad load → numeric capability validation →
  required baseline GL state (clear color / depth / blend / cull per ADR-0005) → optional
  non-interactive frame loop. No renderer/scene/asset code exists yet (M1 is where the renderer
  subsystem starts; M1/M2 seams are declared, not implemented).
- A small `noto3d::core::Log` is introduced with the bootstrap; module directories
  `src/{app,core,renderer,asset,scene,serialization,ui}` remain placeholders until used.
- CLI `--frames=N` selects non-interactive verification; default run opens the configured window.

## Testing Decisions

- **What a good M0 test is**: external behavior — configure succeeds, build succeeds, a bounded
  non-interactive run prints `M0_OK`, the capability check rejects below-minimum context
  capability, and the app exits cleanly. No assertions on internal implementation details.
- **Capability validation is structured as a pure function**
  (`supportsRequiredContext(major, minor, profileMask)`), so it is testable dependency-free
  without fabricating a real sub-3.3 context. Test matrix:
  - 3.3 core → `true`; 4.6 core → `true`
  - 3.2 core → `false`; 3.3 compatibility → `false`
- **Modules tested now**: the bootstrap (non-interactive marker), capability pure-function tests,
  dependency resolution, and preset behavior. Renderer/UI remain manual verification per ADR-0008.
- **Prior art**: `../reimagined-meme` uses the same FetchContent-into-`lib/` wiring and pairs it
  with small specialized validation executables; adopt the same discipline.
- **M0 acceptance matrix** (status tracked here, per docs lifecycle):

  Dependency resolution:
  - [x] GLFW 3.4 tag resolves → clones into `lib/glfw`
  - [x] GLM 1.0.1 tag resolves → clones into `lib/glm`
  - [x] ImGui `v1.92.9-docking` tag resolves → clones into `lib/imgui`
  - [x] nlohmann/json `v3.11.3` tag resolves → clones into `lib/nlohmann_json`
  - [x] `lib/glad` + `lib/stb` committed as non-fetchable exceptions (clones gitignored)

  Dependency build (blocked by Q9 machine prerequisite):
  - [x] GLFW 3.4 builds
  - [x] ImGui STATIC + GLFW/OpenGL3 backends build

  Environment / bootstrap:
  - [x] `cmake --preset dev` + build succeeds from a clean configure
  - [x] capability pure-function tests pass (3.3/4.6 core → OK; 3.2 core / 3.3 compat → fail)
  - [x] `noto3d-editor --frames=5` prints `M0_OK … 3.3+ core profile` capability check
  - [x] sRGB + MSAA 4 + depth 24 hints are applied on the created context
  - [x] debug GL context is off in release; on in dev with `GL_KHR_debug` verified before use
  - [x] default run opens the configured window and closes cleanly

## Out of Scope

- M1 minimal renderer, M2 asset pipeline, M3+ scene/UI — downstream milestones only.
- Converting `GIT_TAG` pins to commit SHA / `URL_HASH` (watch item in CONTEXT.md).
- Adding a real issue tracker / GitHub remote (this ticket mirrors as a markdown file until then).
- Any `find_package`-based system deps or installing packages as part of the build flow.

## Further Notes

- First configure requires the one-time machine prerequisite
  `sudo apt install libxinerama-dev libxcursor-dev` (Q9).
- When the repo later gains a remote tracker, this spec should be mirrored with the
  `ready-for-agent` triage label per the to-spec skill.