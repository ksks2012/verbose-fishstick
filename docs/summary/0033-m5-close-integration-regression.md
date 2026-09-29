# Execution Summary: 0033-m5-close-integration-regression

- **Date**: 2026-09-29
- **Spec/ADR Path**: `docs/adr/ADR-0011-m5-editor-architecture.md` (Q8 Testing Decisions / close)
- **Ticket Path**: `docs/archive/ticket/0033-m5-close-integration-regression.md` (archived on close)
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 new (working tree, uncommitted per implement skill `/commit` disabled)
- **Files Changed**: 3 (1 new: `scripts/m5-close-matrix.sh`; 1 edited: `tests/app/editor_test.cpp`;
  docs: ticket 0033 + CONTEXT + CHANGELOG + archive moves)

## 📝 Key Changes
- **Close matrix — `scripts/m5-close-matrix.sh <build-dir> [--release]`**: mirrors 0024's M4-close
  shape (ADR-0010 Q8 / ADR-0011 Q8). Two tiers:
  - *CLI entry modes + structured logs*: pre-GL rows assert exit codes on both configs
    (`--list-assets`=0, invalid flag / `--frames=-1` / `--frames=abc` / `--asset=` / `--asset`+`--demo`=1);
    GL rows assert exact exit codes on release (`--frames=8` default / `--asset=smile` / `--demo` → 0)
    and log-line content + ordering on dev as well (`M0_OK` → `M3_INFO`/`M2_OK` → `M4_INFO entities=N`
    → `M1_DONE`), since dev GL exits 1 on the documented Mesa llvmpipe LeakSanitizer leak while still
    emitting correct logs.
  - *Headless M5-op log assertions*: runs the four app suites and requires `ALL PASS`.
  - *Static contract checks*: `collectDrawItems` sole render entry + resolver-only `registry.model(`
    calls (2); `DrawItem::model` still a borrowed pointer; `noto3d_app` links neither ImGui nor GLFW.
- **M5_SEL coverage closed**: `tests/app/editor_test.cpp` attaches a spdlog capture sink (same pattern
  as the other app suites) and asserts `M5_SEL entity='smile' id=N` on selection **and** that deselect
  emits no second `M5_SEL` line — the one M5 op previously without a structured-log assertion.
- **Milestone close**: ticket 0033 updated (all automated + milestone-close boxes checked; the four
  manual smoke boxes left open and recorded as a follow-up requiring a real window session);
  CONTEXT.md + CHANGELOG.md updated in the same change (M5 complete, 0028–0034 archived, 0026 recorded
  as M5-adjacent follow-up waiting on 0028, 0027 an M6 gate, 0035 independent); 0028–0034 moved to
  `docs/archive/ticket/`.

## 🧪 Verification
- **Typecheck/Build**: 🟢 Pass — dev + release rebuilds, **0 warnings** in project code (only a
  pre-existing configure-time deprecation from vendored `lib/tinyobjloader`)
- **Tests**: 🟢 Pass — ctest **21/21** dev + release (app suites: editor/transform-edit/asset-picker/
  entity-edit)
- **Close matrix**: 🟢 Pass — release **30/30** checks (`scripts/m5-close-matrix.sh build/release
  --release`); dev all log-line/static/headless checks pass. `xprop` `WM_NAME` titles re-verified
  for all three entry modes, **unchanged from M4**: `m4_vertical_scene | noto3d (M4)`,
  `crown [placeholder] | noto3d (M4)`, `demo_checker [placeholder] | noto3d (M4)`.

## 🔍 Open Items (recorded, not blocked)
- The four **manual / platform-specific** smoke boxes in ticket 0033 need a real windowed session:
  docking move/resize with correct non-square aspect, the full user edit pass, interaction-ownership
  checks, and no-gizmo/no-undo confirmation. Not automatable in this environment; ticket 0033 stays
  in-progress on those four boxes only.
- ADR-0011 Q5 deviation (separate `noto3d_ui` library) carried from 0028, still recorded, unresolved.
- 0026 (M5-adjacent, waiting on 0028), 0027 (M6 gate), 0035 (code-quality) remain open in
  `docs/ticket/`.

## 📦 Recent Commits
- No new commits this session by design (`/commit` commented out in the implement skill)
- Before this session: `289fffa` feat(editor-shell): add frame-rate readout to the inspector panel ·
  `c6ce618` feat(asset-picker): implement asset picker functionality and related tests