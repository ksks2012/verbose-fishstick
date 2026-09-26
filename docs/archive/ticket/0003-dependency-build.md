# 0003 — Clean configure + dependency build

**What to build:**

Build gate: from a clean tree, `cmake --preset dev` configures completely and `cmake --build
--preset dev` compiles and links every dependency plus the bootstrap binary. This closes the
"Dependency build" half of the M0 acceptance (the resolution half already passed; lib/glad +
lib/stb remain the committed non-fetchable exceptions per ADR-0003).

Pinned set (ADR-0003): GLFW 3.4 (X11 backend), GLM 1.0.1, ImGui `v1.92.9-docking` with the
GLFW/OpenGL3 backends, nlohmann/json v3.11.3, plus committed glad (GL 3.3 Core) and stb_image.
Clones must land in `lib/<name>` via FetchContent `SOURCE_DIR` (never `build/_deps`); all four
clones stay gitignored. `noto3d-editor` (the bootstrap) must compile and link.

Any configure or compile defect against the ADR-0003 dependency contract is fixed within this
ticket — its deliverable is a green dependency build, not runtime behavior.

**Blocked by:** 0002

**Status:** ready-for-agent

- [x] A fresh configure completes with all clones landing in `lib/<name>` at their pinned tags
- [x] GLFW 3.4 (X11) builds
- [x] ImGui STATIC + GLFW/OpenGL3 backends build
- [x] GLM, nlohmann/json, glad, and stb wires compile
- [x] The `noto3d-editor` binary is produced by the build
- [x] `lib/glad`, `lib/stb` are committed (`d033f50`); the four fetched clones remain
      gitignored (ADR-0003 filesystem contract holds)