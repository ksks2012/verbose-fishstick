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

**Status:** ready-for-agent

- [ ] Git-sourced deps (GLFW, GLM, ImGui, nlohmann/json) pinned by immutable commit SHA
- [ ] Where an archive source is used instead of Git, `URL + URL_HASH` provided; `URL_HASH` never
      recorded against a Git-tag fetch
- [ ] Recorded SHAs match the current fetched revisions (verified via FetchContent download)
- [ ] `GIT_SHALLOW` retained only where it works; not required unconditionally
- [ ] Fresh configure/build green on dev/release; `ctest` green; binary links
- [ ] No new system/tooling requirements; `lib/<name>` + gitignore contract unchanged

**Points to ADR:** 0003 (dependency strategy: tags now → SHA/URL_HASH later)