# 0046 — M6 close: integration/regression matrix, observability evidence, archiving

## Executive summary
M6 (Save/Load) is closed with an automated two-tier verification: a new `scripts/m6-close-matrix.sh`
exercises every new observable behaviour (**71 checks**) — the pre-GL load-failure exits, the
three-way mutual exclusion, `--save-scene=` output/overwrite/cleanup, the write-then-reload
round-trip, the empty document, and an unknown `assetId` that is retained — and the existing
`scripts/m5-close-matrix.sh` is byte-for-byte unchanged with ALL PASS in both dev and release.
CTest grows 27 → 34 to cover the per-structural-failure cases. Tickets 0041–0046 are archived.

The one production change this milestone required — moving the `--scene=` load to pre-GL — was
deliberately split into its own commit (`242ce64`, `fix(app)`) so the close ticket's own diff stays
verification and documentation. See *Scope note* below.

## Scope note: why a close ticket changed production code
The ticket's Scope requires each structural failure to exit non-zero **pre-GL**, and the automated
tier to need **no display for any row**. Against 0045's code those two requirements were
unsatisfiable: the load lived in `buildEntryScene`, which runs after `glfwInit()`, so a malformed
`.scene` cost a window and a context before failing.

There were three ways to resolve that, and the choice was **not** the implementer's to make alone:
move the load pre-GL; reinterpret "no display" as "offscreen GL is fine"; or record the rows as an
unmet gap. The first was chosen and kept as a separate commit, leaving the close ticket's own diff
purely test and docs.

## Implementation
- `scripts/m6-close-matrix.sh` (new, 71 checks): the pre-GL rows, the exclusion rows, the save
  rows, the round-trip, the empty document, the unknown-`assetId` row, the headless suites, and the
  static seam contracts. Its tier-B note records the manual items as **not asserted here**.
- `tests/app/scene_file_io_test.cpp`: added the assertions that make the ticket's round-trip promise
  falsifiable — see *TDD* below.
- `CMakeLists.txt`: 7 headless-safe CTest rows, one per structural-failure case.
- `docs/CONTEXT.md`, `docs/CHANGELOG.md`, `docs/adr/0013-scene-save-load-persistence.md`: reconciled.
  The ADR change is an **appended** amendment, per DOCUMENTATION-LIFECYCLE (ADRs are
  append-and-supersede, never overwritten).
- Tickets 0041–0046 moved to `docs/archive/ticket/`.

## The bug the review found
`/code-review` caught that `buildEntryScene` **ignored its own `preloadedScene` parameter** and
re-read the file post-GL — the exact behaviour this ticket exists to remove — and that **every
matrix row still passed**, because re-reading a *valid* document is indistinguishable from having
preloaded it. The whole suite was green over a build that did not do the thing the ticket asked for.

The root cause was not the unused parameter but the **fallback branch wrapped around it**. A re-read
fallback reads as defensive programming; in practice it is where a broken preload goes to hide,
because it converts the failure mode into a *success*. Removed. The parameter is required (no
default), the scene branch fails loudly with an explicit "this is a bug, not a bad document"
diagnostic if it is not engaged, and two matrix rows assert the **mechanism** rather than the
output — `seam: the scene branch does not re-read the document itself` and `seam: preloadedScene is
required, not defaulted`. Re-injecting the reviewed bug fails the first; restoring the default
argument fails the second.

A matrix that only proves "the outputs look right" cannot catch "the work happened in the wrong
place". These rows exist to assert where the work happened.

## TDD at the pre-agreed seams
`tests/app/scene_file_io_test.cpp` gained the assertions that make "entities **and transforms**
match" falsifiable. The pre-existing "transform survives" check asserted `position.y == 0.0f` — and 0
is also what a default-constructed transform holds, so a writer that dropped every transform would
have passed it. It is replaced by a non-identity transform (position, rotation and scale) asserted
exactly, plus a save → reload → save byte-identity check. Red was proven by mutation: dropping the
position in the serializer turns the new assertion red **and** independently fails the matrix's
`NON-IDENTITY position survived` row.

## Tests
- `scripts/m6-close-matrix.sh`: **71 checks, ALL PASS** in dev and release.
- `scripts/m5-close-matrix.sh`: **unmodified**, ALL PASS in dev and release.
- CTest: **34/34** in dev and release (27 → 34).
- Clean dev and release builds, **0 new warnings**.
- Arch guard from 0041 re-confirmed **and proven to still fire**: a deliberately injected
  `renderer` include in a serialization-linked target aborts configure, and a deliberately injected
  `gl*` symbol makes the symbol check report 1.

## Mutation evidence
Each defect was injected, the matrix run, and the defect reverted:

| Injected defect | Caught by |
| --- | --- |
| load moved back post-GL | the `rejected pre-GL` rows + every stem-identity row |
| exclusion gate disabled | all `excl_*` rows |
| `M4_INFO` uses the full path instead of the stem | 6 rows |
| milestone tag drifts to `(M6)` | 2 rows |
| `<filesystem>` added to `editor.cpp` | `seam: editor.cpp contains no filesystem include` |
| a `gl*` symbol defined in the serializer | `seam: noto3d_serialization defines 0 gl*/glad symbols (got 1)` |
| serializer drops the position | `round-trip: a NON-IDENTITY position survived the save` |
| `preloadedScene` ignored, file re-read post-GL | `seam: the scene branch does not re-read the document itself` |
| `preloadedScene` given a default argument | `seam: preloadedScene is required, not defaulted` |

The last two were added *after* the review found that the first seven could not see the bug they
were meant to prevent.

## Two-tier verification
- **Automated tier**: complete, in both configs, as listed above.
- **Manual / platform-specific tier — NOT CLAIMED AS PASSED.** The remaining manual items are
  opening a saved document in a real window to confirm the entities and transforms, and the `xprop`
  `WM_NAME` titles for a `--scene=` run. **This environment has no window session**, so those two
  boxes are recorded as an environment limitation rather than asserted, following 0033's discipline.
  As at 0033, the automated tier substitutes structurally: the round-trip is proven by document
  comparison, and the title contract is asserted as a static contract plus the unchanged `M4_INFO`
  rows.

## Numbers recorded rather than restated
The ticket's invariant "CTest count rises from 22" was **already stale** — 0045 had taken it to 27
before M6 work began. The delivered count is 34, and the discrepancy is recorded in the archived
ticket's *Delivered* section instead of being quietly rewritten.

**Result:** M6 closed, with the manual tier outstanding on hardware this environment does not have.
