# ADR-0008 — M0 code contract: C++20, modules, errors, tests, repo

Status: Accepted

## Context
The project skeleton (M0) and its conventions gate every later milestone. Decisions here prevent
rework in module boundaries, error handling, logging, build discipline and docs lifecycle.

## Decision
- C++20 (`CMAKE_CXX_STANDARD 20`, `REQUIRED ON`, `EXTENSIONS OFF`).
- Namespaces: `noto3d::core`, `noto3d::renderer`, `noto3d::asset`, `noto3d::scene`,
  `noto3d::serialization`.
- Modules: `src/{app,core,renderer,asset,scene,serialization,ui}/`; shaders in top-level `shaders/`.
- Error handling: exceptions allowed only at module boundaries (asset/config load); render loop is
  no-throw; internal failures use `Result`/diagnostics (echoes ADR-0007 tri-state).
- Custom `noto3d::core::Log` (debug/info/warn/error), no spdlog.
- Warnings: `-Wall -Wextra -Wpedantic` always; `-Werror` behind `NOTO3D_WERROR` (default OFF).
- Build: CMake + presets (dev = Debug + ASan/UBSan + debug GL context, configurable not mandatory;
  release = optimized, no sanitizers); "Unix Makefiles" generator (no ninja).
- Testing: dependency-free runner in `tests/`; headless asset/registry/serialization tests;
  renderer/UI verified manually. Sanitizing in dev is a capability, not a forced prerequisite.
- Repository: git initialized on `main`; source/docs/assets committed; `.context/` ignored.
- Documentation: Q1–Q8 recorded as ADRs; `CONTEXT.md` (current implementation truth,
  consolidated from Accepted ADRs), `DEFINITION.md` (canonical project nouns), `CHANGELOG.md`,
  `.context/current-session.md`, per `docs/DOCUMENTATION-LIFECYCLE.md`.

## Consequences
- Foundation decision tree (Q1–Q8) is closed; subsequent questions are implementation discovery,
  not architecture speculation.

## Amendment (M2 / ticket 0010, 2026-09) — Log backend
The custom `noto3d::core::Log` spelling is built on **spdlog**: `log_init()` singles a project-wide
default logger, and the `NOTO3D_*` macros (log.hpp) are thin wrappers around spdlog's default
logger so all noto3d code uses a single, replaceable call-site spelling — the only surface other
modules use. spdlog is fetched into `lib/spdlog` at configure time and pinned to an immutable
commit SHA by ticket 0010. The "no spdlog" decision is superseded in this precise sense: no
third-party logger is passed around module APIs; the facade remains the single call-site spelling
(ADR-0003's amendment records the dependency-strategy consequence).