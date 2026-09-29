# Execution Summary: 0042-scene-restore-and-editor-replace-scene

- **Date**: 2026-09-30
- **Spec/ADR Path**: `docs/adr/0013-scene-save-load-persistence.md` (Q1 derived allocator, Q6 single injection point)
- **Ticket Path**: `docs/ticket/0042-scene-restore-and-editor-replace-scene.md`
- **Branch**: `main`

---

## 📌 Overview

- **Commits**: 1 commit (`a5bbfee`)
- **Files Changed**: 8 files (`+432` / `-9`)
- **Ticket status**: `ready-for-agent` → `complete (2026-09-30)`
- **Unblocks**: with 0041 already complete, **0043 is now unblocked** (chain was `0041‖0042 → 0043`)

This is tracer bullet 2 of M6. It delivers the *destination* for a load with no serialization
code in sight — the surface 0043 will write into. Runtime code is deliberately tiny (a factory and
a one-line assignment); the substance is the two contracts it makes structural.

## 📝 Key Changes

1. **`Scene::fromEntities(std::vector<Entity>)` — the only restore entry that exists**
   Before this there was *no* way to inject a Scene carrying pre-existing ids: `nextEntityId_` is
   private with no setter, and `createEntity` always mints a fresh id. The allocator is **derived
   (`max(restored) + 1`, or `1` when empty), never accepted** — no setter, no `(entities, nextId)`
   overload, no in-place variant — so "the counter is a pure function of the entity set" is a
   type-level property rather than a review convention. Creation order is the input vector order,
   including unsorted input.

2. **Precondition as a debug-only contract marker**
   Non-zero, unique ids are checked with `assert` (matching the existing `draw_list.cpp:18`
   precedent) and **absent in release**. Deliberately *not* a validation path: structural
   validation of a *document* belongs to 0044, so the chain has one validator rather than two
   half-validators. No test asserts a graceful return for that input, as the ticket forbids.

3. **`Editor::replaceScene(Scene) noexcept` — the single injection point (Q6)**
   Takes the Scene **by value** and moves it in, so the entity set and the derived allocator cross
   the boundary together and there is no window in which they disagree; then `deselect()`, because
   selection is session state and is never persisted. It hands nothing out, so the M5 type boundary
   is unchanged. Save needed no new API — `Editor::scene()` already returns `const Scene&`.

## 🧪 Verification

- **Typecheck**: 🟢 Pass — no separate typechecker in this C++ project, so the compiler is the
  typecheck. Clean dev + release builds from scratch, **0 warnings / 0 errors** in both
  (`-Wall -Wextra -Wpedantic` via `noto3d_set_warnings`).
- **Tests**: 🟢 Pass — `ctest` **22/22 dev + 22/22 release** (unchanged count: the new assertions
  live inside the two existing suites, so no new CTest entry), `scripts/m5-close-matrix.sh`
  **ALL PASS** in both build dirs. Single suites were run individually throughout.
- **Assertion growth**: scene suite `check()` **29 → 46**; editor suite **62 → 76**, plus
  **2 `static_assert`s** on the type boundary.
- **Red-green**: **7 mutations, 0 vacuous**. One implementation line is replaced at a time, and
  every mutation rebuilds *both* targets and runs *both* suites so a scene-only run cannot hide an
  editor-suite break.

| Mutation | Caught by |
|---|---|
| A `nextEntityId_` hardcoded to `1` (max ignored) | 7 |
| B off-by-one: allocator is `max`, not `max + 1` | 7 |
| C restored set is sorted (creation order lost) | 6 (5 scene + 1 editor) |
| D `fromEntities` ignores its argument | 11 |
| D2 `fromEntities` drops every entity but the last | 10 |
| E `replaceScene` does not clear the selection | 5 |
| F `replaceScene` never installs the scene | 5 |

- **Precondition verified out-of-band** (throwaway program, deliberately not committed): `id == 0`
  and duplicate ids both abort in debug; under `-DNDEBUG` both run to completion with no
  diagnostic, which is the documented consequence of a debug-only check.

## 🔍 Code Review Findings

Run as two parallel axes (Standards / Spec) against `da17fea`. **Both axes found real defects in
the first draft; all were fixed and re-verified.**

- **Standards** — 🟠 3 hard, 3 judgement calls
  - **Vacuous assertion** (hard): `selectedName() == std::nullopt` after a swap resolves through
    `findEntity(selection_)`, and the old id no longer exists, so it returned nullopt *with or
    without* `deselect()`. Same defect already recorded for `!hasSelection()`. **Fixed** by pinning
    the case a real load can actually produce — a restored scene that *contains* the selected id
    under a different name. Mutation E now catches 5 assertions instead of 2.
  - **Wrong evidence counts** (hard): docs claimed 3 `static_assert`s; there are 2 (a `grep -c` had
    counted a comment). **Fixed** in both the ticket and `CONTEXT.md`.
  - **Stale claim in an edited doc** (hard): `CONTEXT.md:164` stated `EntityId` is "never reused"
    flatly, contradicting the reuse this ticket pins. **Fixed** to "never reused **within one Scene
    lifetime**" with the ADR-0013 Q1 amendment inline; `DEFINITION.md` was already correctly
    qualified and is untouched.
  - *Judgement calls not actioned, deliberately:* the `modelMatrix` assert is redundant but
    falsifiable (mutation C catches it); the `es.size() == 3 &&` guard is what stops the test reading
    `es[1]` out of bounds when the size is wrong, so safety outweighs the file's idiom.
- **Spec** — 🟢 code fully implemented; 4 unsupported *claims* in my own write-up
  - Confirmed **no missing/partial requirements, no scope creep, no wrong implementation**
    (`fromEntities` is the only new public `Scene` method; `editor.cpp` has zero deletions, so "no
    other `Editor` method changes" holds literally).
  - **The `static_assert` did not pin by-value-ness** — a callability check also passes for
    `Scene&`/`const Scene&`, precisely the spelling ADR-0011 forbids, while the message claimed
    "by value". **Fixed** by comparing the whole signature,
    `decltype(&Editor::replaceScene) == void (Editor::*)(Scene) noexcept`; discrimination proven by
    rewriting the signature to `Scene&` in both header and `.cpp`, which fails the build on that
    assert. (It also pins `noexcept`, which is part of the type since C++17.)
  - **Mutation counts were undercounted** — C was 5, actually 6 (5 scene + 1 editor). **Fixed** by
    rebuilding and running both suites per mutation; D was 8, actually 11.
  - **"release behaviour is genuinely undefined" overstated it** — for `id == 0` the release path is
    deterministic (`maxId` stays 0 → `nextEntityId_ = 1`). **Reworded**: the checks are absent, so a
    violating input is silently accepted and the resulting `Scene` breaks its own identity contract.

## 🐛 Spec defect found and corrected

The ticket's own Q1 verification bullet read "the new id equals the pre-destroy maximum
**(`max+1`)**". That parenthetical contradicts the ticket's own Invariants
(`nextEntityId = max(restored ids) + 1`, line 48) and ADR-0013 Q1 (`:121`, "a destroyed high-water id
may reappear"). With survivors `{1, 2}` after destroying id 3, the derived allocator is `2 + 1 = 3`,
which *is* the destroyed value — so the reuse being pinned does occur, but `max+1` read as `3 + 1`
predicts no reuse at all. The first red run failed 2 assertions because the test encoded that
misreading; **the implementation was right and the test was wrong**, so the test was corrected and
the ticket's parenthetical removed. ADR-0013 and `CONTEXT.md` were left untouched — they already
agreed.

## 📦 Recent Commits

- `a5bbfee` feat(scene): add the Scene restore primitive and Editor::replaceScene
- `da17fea` feat(serialization): create `noto3d_serialization` target with architecture guard and versioning
- `c4b9766` docs(adr): lock M6 Save/Load in ADR-0013 and write the 0041-0046 execution chain

## ➡️ Next

0043 (v1 round-trip happy path) is unblocked and is the first ticket to consume this surface. Note
for 0043: it will be the first code to build a `Scene` from *untrusted* input, and it must not
bypass `fromEntities` — the derive-don't-accept property is what keeps the allocator honest.
