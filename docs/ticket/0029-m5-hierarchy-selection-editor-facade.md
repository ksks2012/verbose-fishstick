# 0029 — M5 Hierarchy and selection behind an `Editor` facade that owns the Scene

**Source:** ADR-0011 M5 spec (Q3 / Q6 / Q11); second M5 slice.
**Status:** implemented — machine-verified; manual evidence pending
**Blocked by:** None (0028 landed in `8456113` / `a05e852`)

Delivers the first editing behaviour *and* the ownership model the rest of M5 — and M6 — depends
on. 0030 and 0031 both branch off this ticket, so it must land the facade, not just the list.

## What it delivers

- A **Hierarchy** panel listing the Scene's entities in creation order, with the current selection
  visible, and click-to-select.
- A selection that **survives frames and repaints** — it is session state, not per-frame state.
- The Inspector shows the selected entity's identity (name and `assetId`), so clicking a row
  visibly agrees with the 3D view about *which* entity is selected. Field editing is 0030's job.
- The Scene moves out of the render scope into an **`Editor` that owns it by value**, and the UI
  reaches the document only through **`EntityId`-keyed semantic operations**.
- Selection changes are observable in the structured log.

## Contracts this slice must honour

- **`Editor` owns the Scene by value** — not by reference. This is what makes M6's snapshot-replace a
  local operation later instead of a fight with a render-scope local (Q11).
- **The UI receives the facade and structurally cannot obtain a mutable `Scene&`, an `Entity*`, or
  a span over entities.** This is a type/interface boundary, not a convention: with a mutable
  `Scene&` in scope, writing a field through it would remain legal and unnoticed (Q11).
- **Editor state holds `EntityId` only**, never an `Entity*`, reference, or span, across a frame
  boundary or a structural mutation. Each frame re-resolves the id (Q3).
- Mutations go through **thin app-layer operations keyed by `EntityId`**; the UI never writes entity
  fields through a pointer it obtained itself (Q3).
- `EditorState` (the selected `EntityId`) is **editor-session** state, separate from the per-view
  `ViewportState` and the per-frame `InputState` introduced in 0028 (Q6).
- **Rendering is unchanged**: still `collectDrawItems(editor.scene(), resolver)`, one seam, borrowed
  frame-local `DrawItem::model`. `Editor` introduces no second render or resolution path and owns no
  asset, model, or GPU resource (Q11, Q1).
- No `Scene` revision counter, no history, no undo at this stage (Q6, Q4).

## Acceptance

- [x] Hierarchy lists every entity in the Scene in **creation order**, and the row count matches the
      entity count the runtime already logs
- [x] Clicking a row selects that entity; the row stays marked selected across subsequent frames
      with no re-click
- [x] Inspector shows the selected entity's name and `assetId`; with no selection it shows an
      explicit empty state
- [x] `Editor` owns the Scene by value and is the only route to it from UI code; a review can
      confirm no mutable `Scene&` / `Entity*` / entity span is reachable from the UI adapter
- [x] Facade operations are `EntityId`-keyed, and each frame re-resolves the id rather than
      retaining a pointer
- [x] Structural mutation through the facade leaves no dangling state in the editor (a selection
      that becomes invalid is handled explicitly, not left as a silent dangling reference)
- [x] Selection is logged in the existing structured-log format, consistent with the `M0_OK` /
      `M3_INFO` / `M4_INFO` pattern
- [x] Headless suite in the app-layer test family covers: facade operations, selection persistence
      across frames, enumeration order, and that mutations through the facade are visible to
      enumeration
- [x] M4 contracts unchanged: CLI exit-code matrix, `M4_INFO` lines, `xprop` titles, and the single
      resolution seam
- [ ] Manual evidence recorded: list contents, click-to-select, and selection surviving interaction
      with the 3D view

**Points to ADR:** 0011 (Q11 Editor owns Scene + facade-only UI access, Q3 EntityId write path, Q6
state lifetimes) · 0010 (Q1/Q2 pure-data Scene and `EntityId` rules, Q5 single resolution seam,
Q4 `clear()` keeps the id counter)

## Verification (recorded 2026-09-28)

**Machine-verified** — dev + release build 0 error / 0 warning; `ctest` **18/18** in both
(17 pre-existing + new `app-editor-facade`). `noto3d-editor-tests` runs **52 checks** and links
`noto3d_app noto3d_core` only, so the facade is covered headlessly with no GL context.

Acceptance boxes ticked against these observations:

- Enumeration order, `EntityId`-keyed mutation, lifecycle, and stale-selection handling are each
  asserted in `tests/app/editor_test.cpp` (e.g. `order: [0] is smile`, `after destroy middle:
  order is [a, c]`, `stale select: hasSelection false (entity gone)`).
- The facade boundary was checked by inspection, not just by convention: `Editor`'s public API
  exposes no mutable `Scene&`, no `Entity*`, and no entity span. `src/ui/editor_shell.cpp` uses
  only `hasSelection / hierarchyItems / select / selectedAssetId / selectedId / selectedName`, and
  the sole UI contact with the document is `panel_render.hpp`'s
  `const scene::Scene&` feeding `app::collectDrawItems()` — the single M4 seam, unchanged.
- Selection logging confirmed as `NOTO3D_INFO("M5_SEL  entity='{}' id={}")`, following the
  existing `M0_OK` / `M3_INFO` / `M4_INFO` pattern (ADR-0011 Q8).
- M4 contracts re-run: `M4_INFO` for all three entry modes (`m4_vertical_scene` / `demo_checker` /
  `smile`) and the six CLI exit-code tests unchanged.

**Still open** — the last acceptance box, manual evidence. Two reasons it is not a formality:

1. `ImGui::Selectable` click-to-select needs a real pointer, so nothing headless covers it.
2. Selecting a row makes `IsAnyItemActive()` true, which closes the 0028 camera gate for that
   frame. That interaction (select a row, then drag in the 3D view) is currently unverified, and a
   regression there would be silent.
