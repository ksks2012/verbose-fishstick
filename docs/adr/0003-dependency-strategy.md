# ADR-0003 — Self-contained project dependency/build strategy

Status: Accepted

## Context
Environment (verified): Ubuntu 24.04, g++ 13.3.0, CMake 3.28.3, no pkg-config, no ninja, no
system GLFW/SDL, uid 1000 (no passwordless sudo). OpenGL/EGL/X11/Wayland dev headers are
installed. GLFW 3.4 can be built from source against X11 via CMake `FindX11` without pkg-config.

## Decision
- "Project-owned dependencies must build without sudo, apt, pkg-config, or Ninja, and without
  requiring system-installed GLFW, GLM, ImGui, or nlohmann/json."
- **Filesystem contract**: FetchContent dependencies are populated into repository-local
  `lib/<name>` directories via explicit `SOURCE_DIR` configuration. The exact CMake implementation
  is part of M0 and must preserve this path contract (`build/_deps` is NOT used).
  Clone layout:
  ```text
  emoji_editor/
  └── lib/
      ├── glfw/            # GLFW 3.4, X11 backend
      ├── glm/             # GLM 1.0.1
      ├── imgui/           # ImGui v1.92.9-docking (docking required for M5; NOT on master)
      └── nlohmann_json/   # nlohmann/json v3.11.3
  ```
- `lib/` is gitignored (`.gitignore`): clones are fetched at configure time, never committed.
- Non-fetchable/generated sources are committed under `lib/` with `.gitignore` exceptions:
  glad (GL 3.3 Core, generated; no upstream repo to fetch), stb_image (single header).
- tinyobjloader: deferred; decided by the M2 asset-pipeline spike.
- Pinning: tags first; immutable commit SHAs and URL_HASH later once dev build is stable.
- Machine prerequisites (Q9): the OS/toolchain packages needed to *build the fetched sources*
  are allowed to require one-time system installation, e.g. `sudo apt install libxinerama-dev
  libxcursor-dev libxi-dev` (GLFW X11; verified set, Sep 2026). Unlike project-owned
  dependencies, these are NOT covered by the "no sudo" contract.
- GLFW is configured X11-only (`GLFW_BUILD_WAYLAND=OFF`): the Wayland backend's xkbcommon probe
  goes through `pkg_check_modules`, which would require pkg-config; the X11 backend slots are
  found via `find_path`/`find_library` only, keeping pkg-config out of the build contract.

## Verification
The M0 acceptance criteria and running status live in the M0 ticket
(`docs/ticket/0001-build-environment-spec.md`, Testing Decisions). ADRs are decision records and
do not carry checklists; item-level status is tracked at ticket level.

## Consequences
- FetchContent resolves network at configure time (offline thereafter for vendored parts).
- Fresh checkout: `lib/` is missing until first configure; all clones land inside the repo tree.

## Amendment (M2 / ticket 0010, 2026-09)
- The M2 asset-pipeline spike decided the deferred **tinyobjloader** question the ADR left open:
  it was fetched (OBJ/MTL parser, `lib/tinyobjloader`, v1.0.6) as part of ticket 0008.
- **spdlog** was added as the logging backend of the custom `noto3d::core::Log` facade (see
  ADR-0008 amendment), fetched into `lib/spdlog` (v1.14.1).
- The lib/ clone layout now spans six Git deps: `glfw`, `glm`, `imgui`, `nlohmann_json`,
  `tinyobjloader`, `spdlog` (all gitignored; glad + stb_image remain committed exceptions).
- Pinning hardened by ticket 0010: all six Git deps are pinned to **immutable commit SHAs**
  (each resolves exactly to its named release tag). No archive/URL sources exist, so `URL_HASH`
  is n/a and is never recorded against a Git fetch. The original "tags now, SHA later" decision
  is superseded for the current dependency set.