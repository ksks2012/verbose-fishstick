# 0042 — `Scene` restore primitive + `Editor::replaceScene(Scene)`

**Source:** ADR-0013 Q1 (derived allocator), Q6 (single injection point) — tracer bullet 2.
**Status:** complete (2026-09-30) — evidence recorded below
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
    `createEntity` → the new id equals the pre-destroy maximum, i.e. the destroyed value
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

## Delivered (2026-09-30)

**Shape chosen** — `static Scene Scene::fromEntities(std::vector<Entity> entities)`
(`src/scene/scene.hpp:80`, `src/scene/scene.cpp:13`). The static factory was preferred over an
in-place variant because `Editor::replaceScene` moves a whole `Scene`, so nothing needs to
populate an existing one. The allocator is **derived, never accepted**: there is no setter and no
`Scene(entities, nextId)` overload, so "the counter is a pure function of the entity set" is a
type-level property rather than a review convention.

**Precondition choice: `assert` in debug, absent in release** (`src/scene/scene.cpp:19-27`),
following the existing `assert` precedent in `src/app/draw_list.cpp:18`. It is a contract marker,
**not** a validation or diagnostic path, and no test asserts a graceful return for it. Verified
out-of-band with a throwaway program (not committed): `id == 0` aborts on
`entities[i].id != InvalidEntityId`, duplicate ids abort on `entities[i].id != entities[j].id`.
The same program under `-DNDEBUG` runs to completion in both cases with **no diagnostic** — a
violating input is silently accepted and the resulting `Scene` breaks its own identity contract (a
duplicate id resolves to the first match; the `id == 0` entity is unaddressable). That silence is
the intended consequence of choosing a debug-only check; it is *not* enforced, and structural
validation of a *document* stays solely in 0044, so there is one validator, not two
half-validators.

**`Editor::replaceScene(scene::Scene scene) noexcept`** (`src/app/editor.hpp:86`,
`src/app/editor.cpp:161`) — by value, moved in, then `deselect()`. Taking the Scene by value is
what makes the swap atomic: the entity set and the derived allocator cross the boundary together,
so there is no window in which they disagree. Save needed no new API; `scene()` is untouched.

### Ticket defect found and corrected

The Q1 verification bullet originally read "the new id equals the pre-destroy maximum
**(`max+1`)**". That parenthetical is wrong and contradicts the same ticket's own invariant
(`nextEntityId = max(restored ids) + 1`, line 48) and ADR-0013 Q1. With survivors `{1, 2}` after
destroying id 3, the derived allocator is `2 + 1 = 3` — which **equals the destroyed value**, so
the reuse the ticket is pinning does happen; but `max+1` read as "3 + 1 = 4" predicts no reuse at
all. The first red run failed 2 assertions because the test encoded that misreading. The
implementation was right, the test was wrong, so the **test** was corrected and the ticket's
parenthetical removed. ADR-0013 `:121` ("a destroyed high-water id may reappear") and `CONTEXT.md`
(`max(restored) + 1`) were left untouched — they already agreed.

### Red-green: 7 mutations, 0 vacuous

Every new assertion was shown to fail against a deliberately wrong implementation. One line of the
implementation was replaced at a time, **both** affected targets rebuilt and **both** suites run,
so a mutation that breaks the editor suite cannot hide behind a scene-only run:

| Mutation | Caught by |
|---|---|
| A `nextEntityId_` hardcoded to `1` (max ignored) | 7 |
| B off-by-one: allocator is `max`, not `max + 1` | 7 |
| C restored set is **sorted** (creation order lost) | 6 (5 scene + 1 editor) |
| D `fromEntities` ignores its argument | 11 |
| D2 `fromEntities` drops every entity but the last | 10 |
| E `replaceScene` does not clear the selection | 5 |
| F `replaceScene` never installs the scene | 5 |

Mutation **C** is why the restore fixture uses ids `{7, 3, 11}` rather than ascending ones — a
sorting implementation cannot pass by accident.

### Two vacuous assertions found by review and fixed

The first version of the editor block asserted `selectedName() == std::nullopt` and
`!hasSelection()` after a swap. Both go through `findEntity(selection_)`, and the previously
selected id no longer resolves in the replaced scene, so they reported "nothing selected" **with or
without** `deselect()` — mutation E proved it, catching only `selectedId()`. The block could not
distinguish a real deselect from none at all.

Fixed by pinning the case that actually matters: a restored scene that **contains the same id**,
under a different name. There `findEntity` resolves, so the assertions bite — mutation E now
catches 5 assertions instead of 2, including `hasSelection()` and `selectedName()`. This is the
dangerous shape a load can actually produce, since ids come from the file.

### Verification

- `tests/scene/scene_test.cpp`: `check()` count **29 → 46** (3 new test functions —
  `testRestorePreservesEverything`, `testRestoreDerivesAllocator` and
  `testRestoreQ1ReusesDestroyedId`).
- `tests/app/editor_test.cpp`: `check()` count **62 → 76**, plus **2 `static_assert`s** pinning the
  M5 type boundary: `scene()` still returns `const Scene&`, and
  `decltype(&Editor::replaceScene)` is exactly `void (Editor::*)(Scene) noexcept`. The second form
  is deliberate — a mere callability check also passes for a `Scene&` parameter, which is the one
  spelling ADR-0011 forbids; comparing the whole signature pins the parameter type itself (and,
  since C++17, `noexcept` is part of that type, so it is pinned too). Discrimination was verified:
  rewriting the signature to `scene::Scene&` in both the header and the `.cpp` fails the build on
  that `static_assert`.
- Invariant "no public counter setter" verified by inspection: `nextEntityId_` appears only as the
  private member (`scene.hpp:96`); the sole new public method in the whole change is
  `replaceScene(scene::Scene) noexcept`, which hands out no `Scene&`, `Entity*`, or span.
- `docs/CONTEXT.md` amended in the same change: the M4 line stated `EntityId` is "never reused"
  flatly, which the pinned Q1 case deliberately contradicts. It now reads "never reused **within one
  Scene lifetime**" and carries the ADR-0013 Q1 amendment inline, matching how this file amends its
  other superseded claims. `docs/DEFINITION.md` was already correctly qualified and is untouched.
- Clean dev + release configure/build from scratch: **0 warnings, 0 errors** in both.
- `ctest`: **22/22** dev, **22/22** release (unchanged — no new CTest entry; the new assertions
  live inside the two existing suites).
- `scripts/m5-close-matrix.sh`: **ALL PASS** in both build dirs.

**Out-of-scope respected** — no JSON, no file I/O, no CLI; no undo/redo and no selection
persistence; no validation/repair of malformed entity sets; no new save API on `Editor`
(`editor.cpp` has zero deletions, so "no other `Editor` method changes" holds literally).

