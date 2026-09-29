# 0042 — `Scene` restore primitive + `Editor::replaceScene(Scene)`

**Source:** ADR-0013 Q1 (derived allocator), Q6 (single injection point) — tracer bullet 2.
**Status:** ready-for-agent
**Blocked by:** — (nothing blocks this)
**Parallel with:** 0041 — the two are independent and may run concurrently, but **completing only
this one does not unblock 0043**: 0043 is gated on *both*, so if one of the pair slips the other's
"green first" value drops and the frontier stalls at 0043 rather than advancing.
**Related:** ADR-0013 Q1/Q6, `src/scene/scene.hpp` (the private `nextEntityId_` and the M6 forward
contract), `src/app/editor.hpp` (the `Editor` facade + selection), ADR-0010 Q2/Q4, ADR-0011 Q6

## What it delivers

The **destination surface** for a load, without waiting for any serialization code. Two pieces:

1. A `Scene` restore entry that builds a Scene from entities **carrying their own ids**, deriving
   `nextEntityId = max(restored ids) + 1` (empty → `1`). This is genuinely new surface:
   `nextEntityId_` is private with no setter (`scene.hpp:70-73`) and `createEntity` always allocates
   a fresh id (`scene.cpp`), so there is currently no way to inject a pre-built scene.
2. `void Editor::replaceScene(scene::Scene scene)` — by value, moved in, selection cleared. The one
   and only route by which load reaches the editor (Q6).

## Scope

- **`Scene` restore entry** — a factory or member taking `std::vector<Entity>` (creation order
  preserved). On construction it computes the derived allocator from the incoming ids. Suggested
  shape: `static Scene Scene::fromEntities(std::vector<Entity> entities)`; an in-place variant is
  **not** needed because `replaceScene` moves a whole `Scene`. Pick whichever fits the existing
  header style and state it in the delivered section.
  - **Precondition, not validation.** Ids are non-zero and unique; creation order is the vector
    order. Structural validation is the *serializer's* job (Q3, ticket 0044), so this primitive is a
    trusted low-level entry — document the precondition rather than half-implement validation here.
  - **Derive, do not accept, the allocator.** The signature must make it impossible to pass an
    arbitrary counter. No `setNextEntityId`, no `Scene(entities, nextEntityId)` overload. The value
    arrives only as a consequence of the entity set.
- **`Editor::replaceScene`** — assigns `scene_ = std::move(scene)` and calls the existing
  `deselect()` so `hasSelection()` is false and `selectedId() == InvalidEntityId` afterwards
  (selection is not persisted; Q2/Q6). No other `Editor` method changes.
- **Save needs no new API.** Do not add one — `Editor::scene()` already returns `const Scene&`
  (`editor.hpp:73`), which is all the save path needs.

## Invariants

- **No public counter setter** anywhere on `Scene` (Q1) — the allocator is only ever a function of
  the entity set.
- **The M5 type boundary holds**: `Editor` still exposes no mutable `Scene&`, `Entity*`, or span.
  `replaceScene` takes a `Scene` **by value**; it hands nothing out.
- `nextEntityId = max(restored ids) + 1` for a non-empty set, `1` for an empty set.
- Within one Scene lifetime ids remain unique; a restore starts a new lifetime, so an id that was
  destroyed before the restore *may* reappear — this is accepted (Q1), not a defect.

## Explicitly NOT in scope

- Any JSON, file I/O, or CLI (0043–0045).
- Undo/redo or selection persistence (ADR-0011 Q4; ADR-0013 Q2).
- Validation/repair of malformed entity sets (0044 owns structural failure).

## Verification

- **Headless `Scene` suite additions:**
  - restore preserves ids, creation order, names, assetIds, and transforms field-for-field;
  - after restoring a set whose max id is `M`, `createEntity` returns `M + 1`;
  - restoring an **empty** set leaves `createEntity` returning `1`;
  - **the Q1 case, pinned here**: build a Scene, destroy the highest id, restore from the survivors,
    `createEntity` → the new id equals the pre-destroy maximum (`max+1`), i.e. the destroyed value
    is
    reused. If this assertion ever changes it is a decision, not a drift.
- **`Editor` suite additions:** `replaceScene` with an in-memory `Scene` needs no file I/O; after a
  prior `select()`, `replaceScene` yields `hasSelection() == false` and
  `selectedId() == scene::InvalidEntityId`; the replaced entities are the ones supplied.
- All new assertions shown to **fail** against a deliberately wrong implementation (red-green), so
  none is vacuous (see 0040 for the failure mode this guards against).
- **Precondition-violation case (restated deliberately).** Handing the restore primitive a vector
  containing `id == 0` or duplicate ids is a **precondition violation** (undefined behaviour, or an
  `assert` in debug — pick one and say which in the delivered section), **not** a diagnostic path.
  This ticket must not add a validation/diagnostic return to handle it; structural validation of a
  *document* is 0044's job, and duplicating it here would create two half-validators. No test should
  assert a graceful return for this input — assert only that the *document* path (0044) rejects it.
- ctest green dev + release (count rises with the new cases). No GL, no display.

**Rolls under spec:** ADR-0013 Q1, Q6; PRD user stories 2, 10.
