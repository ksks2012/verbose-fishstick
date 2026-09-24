# 0010 — Pin dependencies by commit SHA + URL_HASH (ADR-0003)

**What to build:**

ADR-0003 watch item: once the dev build is stable, replace the current GIT_TAG pins with immutable
commit SHAs and URL_HASHes for the four FetchContent deps (GLFW 3.4, GLM 1.0.1, ImGui
v1.92.9-docking, nlohmann/json v3.11.3). Reproducible, sponsor-recorded dependency set.

Contract:
- Each `FetchContent_Declare` entry records the resolved commit SHA (+ `GIT_SHALLOW` where
  applicable) and a `URL_HASH` so upstream tag movement or source edits are detected, not silently
  absorbed.
- SHAs pin exactly the current fetched revisions; the four clones remain in `lib/<name>` and stay
  gitignored (filesystem contract unchanged).
- No new apt/pkg-config/ninja requirements; presets and `-Wall -Wextra -Wpedantic` unchanged.
- Gate: from a clean tree, `cmake --preset dev` and `--build` fetch-pin green; `ctest` green;
  `noto3d-editor` links.

**Blocked by:** none (may run anytime after M0; dev build is stable)

**Status:** ready-for-agent

- [ ] All four deps pinned by commit SHA (+ `URL_HASH`), replacing bare GIT_TAG pins
- [ ] Recorded SHAs match the current fetched revisions (verified via FetchContent download)
- [ ] Fresh configure/rebuild green on dev/release; `ctest` green; binary links
- [ ] No new system/tooling requirements (no sudo/apt/pkg-config/ninja)

**Points to ADR:** 0003 (dependency strategy: tags now → SHA/URL_HASH later)