# Execution Summary: 0032-m5-entity-create-destroy

- **Date**: 2026-09-29
- **Spec/ADR Path**: `docs/adr/ADR-0011-m5-editor-architecture.md` (Q3/Q4/Q6/Q7/Q11)
- **Ticket Path**: `docs/ticket/0032-m5-entity-create-destroy.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 new (working tree, uncommitted per implement skill `/commit` disabled)
- **Files Changed**: 11 files (8 tracked + 3 new: `src/app/entity_edit.{hpp,cpp}` + `tests/app/entity_edit_test.cpp`)

## 📝 Key Changes
- **Seam A — `src/app/entity_edit.{hpp,cpp}`**: `commitCreateEntity(Editor&, const AssetChoice&)` and
  `commitDestroySelected(Editor&)`, both through the `Editor` facade with `M5_ENTITY` logging.
  `commitCreateEntity` is **type-enforced** to take a picker `AssetChoice` (drawable-by-construction
  is a signature property, not a comment); no undo, silent no-op destroy.
- **Seam B — facade-level tests**: id monotonicity across a destroy/create cycle, `clear()` retaining
  the counter, no-op destroys (`InvalidEntityId`, unknown, already-destroyed) appended to
  `tests/app/editor_test.cpp`.
- **UI (`src/ui/editor_shell.cpp`)**: Hierarchy gains a Create combo (fed from `picker.choices`) +
  Create button + Destroy-selected button; unique `###<entityId>` Selectable ids; create-choice state
  held in the window's ImGui storage (ADR-0011 Q6), choice object resolved at click time.
- **Log assertion**: `tests/app/entity_edit_test.cpp` attaches a spdlog capture sink and asserts the
  emitted `M5_ENTITY created/destroyed` format and the silent no-op. CMake wires the new source +
  `noto3d-entity-edit-tests` suite (ctest 20→21).

## 🧪 Verification
- **Typecheck/Build**: 🟢 Pass — clean dev + release rebuilds, **0 warnings** each
- **Tests**: 🟢 Pass — ctest **21/21** dev + release; M4 CLI matrix intact
  (`--list-assets`=0, bad flag/`--asset`+`--demo`/`--frames=-1`=1); release smoke `--demo --frames=15`
  → `M4_INFO` + `M1_DONE`, exit 0

## 🔍 Code Review Findings
- **Standards**: Fixed — process-global `static int createAssetIndex` (ADR-0011 Q6) → window-scoped
  ImGui storage; `commitCreateEntity(Editor, string, string)` accepted a bare asset id → takes typed
  `const AssetChoice&`; duplicated choice-label text → `choiceLabel()` helper; stale "ONLY code path"
  claim in `asset_picker.hpp` reworded (create is a second picker-derived write path).
- **Spec**: Fixed — (1) unbalanced `BeginDisabled/EndDisabled` after a successful Destroy (ImGui stack
  assert) → capture `canDestroy` once, pair unconditionally; (2) `Selectable()` id collisions when two
  entities share a name → `###<entityId>` suffix; (3) on-screen `%zu entity(s)` widget was un-asked-for
  scope creep hiding when the list is empty → removed (count stays in the `M5_ENTITY` log line);
  (4) unverified log-format boxes → capture-sink assertions; (5) `xprop` wording tightened (manual item).

## 📦 Recent Commits
- No new commits this session by design (`/commit` commented out in the implement skill)
- Before this session: `289fffa` feat(editor-shell): add frame-rate readout to the inspector panel ·
  `c6ce618` feat(asset-picker): implement asset picker functionality and related tests