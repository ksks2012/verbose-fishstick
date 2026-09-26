# 0010 — Harden dependency pinning (ADR-0003)

**What to build:**

ADR-0003 watch item: once the dev build is stable, harden dependency pinning so upstream tag
movement or source edits are detected, not silently absorbed. The pinning mechanism must match
each dependency's acquisition mode.

Contract (ADR-0003):
- For **Git-based** FetchContent dependencies (GLFW, GLM, ImGui, nlohmann/json): pin
  `GIT_TAG = immutable commit SHA` (not a moving tag).
- For any dependency fetched as a **URL/archive**: supply `URL + URL_HASH` (SHA256) as the
  integrity check.
- Use the mechanism appropriate to each dependency's acquisition mode; **do not assume
  `URL_HASH` applies to Git fetches** — a Git `GIT_TAG` pin is a commit SHA, an archive pin is
  `URL + URL_HASH`. Never record a `URL_HASH` as a companion to a Git commit SHA.
- `GIT_SHALLOW` where it remains available; a shallow clone may not be possible for arbitrary
  commit SHAs on some upstreams — do not require `GIT_SHALLOW TRUE`.
- Clones stay in `lib/<name>` and remain gitignored (filesystem contract unchanged); no new
  system/tooling requirements (no sudo/apt/pkg-config/ninja).
- Gate is regression-only: `ctest` green; `noto3d-editor` links; presets and
  `-Wall -Wextra -Wpedantic` unchanged.

This is an infrastructure-hardening ticket on the M0→parallel track, not a runtime dependency of
the M1/M2 chain.

**Blocked by:** M0 completion

**Status:** completed

- [x] Git-sourced deps pinned by immutable commit SHA — verified **all six** (GLFW, GLM, ImGui,
      nlohmann/json, tinyobjloader, spdlog): `lib/<name> HEAD == CMake GIT_TAG` for each
- [x] No archive/URL sources exist → `URL + URL_HASH` is n/a; `URL_HASH` never recorded against
      a Git-tag fetch
- [x] Recorded SHAs match the current fetched revisions (verified via `git rev-parse HEAD` in
      each `lib/<name>`) AND each resolves exactly to its named release tag (`3.4`, `1.0.1`,
      `v1.92.9-docking`, `v3.11.3`, `v1.0.6`, `v1.14.1`)
- [x] `GIT_SHALLOW` retained where it works — kept as `TRUE` for all (fetches build green)
- [x] Fresh configure/build green on dev/release; `ctest` green (4/4 both); binary links
- [x] No new system/tooling requirements; `lib/<name>` + gitignore contract unchanged

**Points to ADR:** 0003 (dependency strategy: tags now → SHA/URL_HASH later)