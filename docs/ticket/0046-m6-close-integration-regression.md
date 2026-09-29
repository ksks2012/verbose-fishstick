# 0046 — M6 close: integration/regression matrix, observability evidence, archiving

**Source:** ADR-0013 (whole spec, all Q); PRD Testing Decisions, two-tier verification. Close
ticket.
**Status:** ready-for-agent
**Blocked by:** 0045
**Related:** ADR-0010 Q8 (verification tiers), ADR-0011 Q9 (frozen title contract),
`docs/archive/ticket/0024-m4-integration-regression.md` and
`docs/archive/ticket/0033-m5-close-integration-regression.md` (the close-ticket pattern this
follows),
`scripts/m5-close-matrix.sh`

## What it delivers

M6's close: extend the automated matrix to cover every new observable behaviour, re-run it in dev
**and** release, gather the manual evidence the two-tier policy requires, reconcile the docs, and
archive the chain. A milestone is not closed because the code exists; it is closed because the
evidence exists and the existing evidence did not regress.

## Scope

- **A new `scripts/m6-close-matrix.sh`, not a grown `m5-close-matrix.sh` — this is locked.** The M5
  script has already absorbed rows from two milestones; a third would make it a monolith nobody can
  read. The new script is the M6 matrix and may **call or source shared helpers** from the M5 one,
  so pre-existing rows are not duplicated or reworded. The M5 script is otherwise **not** modified,
  which is what makes "the existing rows are unchanged" cheap to verify. (Per ADR-0013 Q9 the
  `M4_INFO` shape is *reused*, so this matrix **gains rows** rather than a new format.) Add rows:
  - a valid `--scene=` run: `M4_INFO rendering scene '<stem>' entities=<N>` reused, plus the
  `M6_INFO`
    load line;
  - each structural-failure exit (missing file, malformed JSON, `version != 1`, missing/ mistyped
    field) → non-zero, **pre-GL**;
  - the three-way mutual-exclusion exits (`--asset`+`--scene`, `--demo`+`--scene`, and the
    unchanged `--asset`+`--demo`) → 1;
  - `--save-scene=<good>` → writes a document; `--save-scene=<missing dir>` → non-zero; overwriting
    leaves one valid document and no temp residue;
  - the **write-then-reload** check (save a scene, reload it via `--scene=`, confirm entities and
    transforms match) — the end-to-end round-trip;
  - an **empty document** load → `entities=0`, exit 0;
  - a document naming an **unknown `assetId`** → opens, the entity is retained, and it is **skipped
  at
    render** by the M4 resolver (warned, not refused) — user story 4.
  - The existing M4/M5 rows and the three `xprop` `WM_NAME` titles must be **unchanged**; record
    their titles (tag still `(M4)`).
- **Two-tier verification** (ADR-0010 Q8 / ADR-0011):
  - *automated*: matrix ALL PASS in dev + release; ctest green with the new entries; the CLI
  exit-code
    matrix; no display required for any row.
  - *manual / platform-specific*: open a saved document in the real editor and confirm the entities
    and transforms match (the same "manual confirmation" standard 0024 used), plus the `xprop` title
    evidence for a `--scene=` run. Record the evidence (or the environment limitation, if no real
    window session is available — do **not** claim a manual pass you did not observe).
- **Docs reconciliation** (DOCUMENTATION-LIFECYCLE): promote any durable knowledge; ensure
  `docs/CONTEXT.md` reflects M6 as delivered (entry modes, observability, CTest count), add the M6
  milestone/architecture entry to `docs/CHANGELOG.md`, confirm `docs/DEFINITION.md`'s `scene file` /
  `Scene snapshot` / `EntityId` entries match the shipped behaviour, and append (never edit) any
  amendment to ADR-0013 if implementation forced a change.
- **Archiving:** write an execution summary (e.g. `docs/summary/0046-m6-save-load-close.md`), move
  tickets **0041–0046** to `docs/archive/ticket/`, and reconcile the ADR-0013 status line and the
  archived-ticket count in `CONTEXT.md`.

## Invariants

- **No regression in the established evidence**: every pre-existing M4/M5 matrix row, exit code, log
  line, and `WM_NAME` title is unchanged; the milestone tag stays `(M4)`.
- The **`M4_INFO` contract is the single "rendering scene" shape** across all entry modes; M6 adds
  only `M6_INFO` load/save *event* lines.
- The arch guard from 0041 is re-confirmed at close (the serializer still links no
asset/renderer/GL;
  the guard still fires on a deliberately forbidden link).
- The `noto3d_serialization` include/link closure is still GL-free (symbol check: 0 `gl*`/glad
  symbols), matching the 0037/0038 evidence style.
- CTest count rises from 22 with the new round-trip and per-failure-case entries; the new number is
  recorded in the summary and `CONTEXT.md`.

## Explicitly NOT in scope

- Screenshot / readback / image-similarity (the M3 exclusion continues) — "the entities match" is
  asserted structurally (round-trip comparison) and by manual observation, not by pixels.
- Undo/redo, scene graph, prefabs, multi-view (M4/M5 exclusions continue).
- In-editor file UI, dirty state, migration framework, `--new`/`--empty`, `--force` (ADR-0013 Out of
  Scope).

## Verification

- `scripts/m6-close-matrix.sh` ALL PASS in **both** dev and release; the unmodified
  `scripts/m5-close-matrix.sh` still ALL PASS with every pre-existing row present and unchanged.
- **ctest green in dev + release** with the final count recorded.
- Full clean dev + release build, 0 new warnings.
- The M4/M5 22-test suite and the new M6 suites all pass; the 0037/0038 guards still proven to fire.
- Manual evidence recorded honestly in the ticket (or an explicit note that no window session was
  available, as 0033 did for its four manual boxes) — never an unearned "manual pass".
- `docs/CONTEXT.md`, `docs/CHANGELOG.md`, `docs/DEFINITION.md` updated to describe **delivered** M6;
  tickets 0041–0046 archived; execution summary written; ADR-0013 marked complete (or amended, if
  the implementation diverged — appended, never edited).

**Rolls under spec:** ADR-0013 (M6 Save/Load spec, Q0–Q11); closes the milestone.
