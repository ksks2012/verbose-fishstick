# Execution Summary: 0045-cli-scene-entry-and-save-scene

- **Date**: 2026-09-30
- **Spec/ADR Path**: `docs/adr/0013-scene-save-load-persistence.md` (Q5, Q6, Q7, Q8, Q9, Q10, Q11)
- **Ticket Path**: `docs/ticket/0045-cli-scene-entry-and-save-scene.md`
- **Branch**: `main`

---

## 📌 Overview

- **Commits**: 0 — the work is staged and **uncommitted at the user's request** (the `/commit`
  instruction was commented out). Base for the review and the diff is `41184b3` (0044).
- **Files Changed**: 13 files (+917 / -20)

## 📝 Key Changes

- **New app-layer file I/O seam** — `src/app/scene_file_io.{hpp,cpp}`: `readSceneFile()`,
  `writeSceneFileAtomic()`, `saveSceneTo()`, `loadSceneFrom()`, `sceneNameFromPath()`. It is the
  only code touching the filesystem and the only non-test consumer of `deserializeScene()`.
  `Editor` stayed I/O-free and needed **no new API**: load rides 0042's
  `Editor::replaceScene(std::move(...))`, save reads the existing `scene()`.
- **Q8 atomicity** — the temp file is created in the **destination directory** (not `/tmp`), because
  `rename()` is atomic only within one filesystem; it is named `"." + stem + ".tmp." + pid + "." +
  counter`; the destination is opened only at `rename()`, so any earlier failure leaves the prior
  good document untouched; overwrite is silent; a missing parent directory is a diagnostic and a
  non-zero exit, **never** a silent `mkdir`; the temp is removed on every failure path.
- **Q5 exclusion, pre-GL** — `--scene=` is the third mutually exclusive entry mode. One table now
  drives both the count and the diagnostic. Three committed CTest rows
  (`cli-scene-*-exclusive`) point at a **valid** document so they can distinguish "the gate fired"
  from "the load failed".
- **Q7/Q9** — `--save-scene=` is an orthogonal output option written once before the frame loop;
  a loaded scene reports its **file stem**, so the frozen `M4_INFO`/title shapes are reused unchanged.

## 🧪 Verification

- **Typecheck**: 🟢 N/A — no standalone typechecker; a clean C++ build **is** the typecheck
  (0 warnings, dev + release).
- **Tests**: 🟢 Pass — ctest **27/27** dev and release (24 → 27: the new `app-scene-file-io` suite
  plus three exclusion rows); `noto3d-scene-file-io-tests` 29 assertions; both close matrices
  **ALL PASS**; **12/12 mutations caught, 0 vacuous**; the new GL arch guard **proven to fire**.
- **By hand, both presets** — the CLI exit matrix including the write-then-reload round-trip and
  every two-mode combination, plus the unresolved-`assetId` warning.

## 🔍 Code Review Findings

`/code-review` at `41184b3`, Standards and Spec axes in parallel.

- **Standards**: ADR-0013 **Q3 was unmet** — "the app layer logs a warning" for an unresolvable
  `assetId`, and nothing warned; `--scene=` is what first made it reachable, so it is now warned
  once per id at entry time (not per frame), keeping Q3's other half (kept, skipped at render, load
  still succeeds). The **entry-mode set was duplicated** (a hand-written sum plus a separate message
  block). The new suite's **"no GL" claim was conventional** — it links `noto3d_core`, which
  PUBLIC-exports glad's include dir — so it now carries the same
  `noto3d_assert_target_reaches_no_gl()` guard as the registry/serialization suites. Also:
  de-duplicated diagnostics (one save failure logged two `M6_FAIL` lines, the second dropping the
  cause; one load failure logged three), `is_directory`'s discarded `ec` (`EACCES` was reported as
  "does not exist"), `M4_FAIL` → `M6_FAIL`, the parser promising a `.scene` extension it never
  validated, a comment claiming a "non-empty directory" where the test creates an empty one, an
  unasserted `sentinel`, and three files missing trailing newlines.
- **Spec**: Q5 exclusion had **no committed automated coverage** — the fix for a false-green row
  existed only in an uncommitted harness; it is now three CTest rows. Q10's empty document was
  written but never loaded; now tested (including `fromEntities({})` and the derived allocator on an
  empty set). The ticket's **"CLI exit matrix (automated, no display)" is only true of the exclusion
  rows** — a load/save needs the `AssetRegistry`, whose GPU cache requires a live context
  (ADR-0010 Q5 + 0025's lifetime rule) — so the over-claim is corrected in the ticket rather than
  left implying coverage that is not automated. One **justified deviation** from the ticket's
  "existing rows unchanged": a close-matrix assertion required `registry.model(` exactly twice, a
  count that is only correct with two entry modes; it now states the intent (at least one
  occurrence, each inside the resolver's `return`). No scope creep found.

## Known gaps (recorded, not papered over)

- **Three mutants are unprovable here and are not counted**: staging the temp in `/tmp` (needs a
  second filesystem), ignoring the `close()` error (needs `ENOSPC`), and losing per-attempt
  temp-name uniqueness. I had written a test for the third by seeding the exact path a fixed-name
  implementation would occupy; the review was right to reject it — it pinned the *mutant's text*
  rather than the behaviour, so it would have passed either way. A temp name is unobservable
  between creation and `rename()` without racing threads.
- **Two defects were in the verification, not the production code**, and are the ones worth
  remembering: a **false-green CLI row** (`--asset`+`--scene` asserted "exit 1" while pointing at a
  non-existent file, so it passed because the *load* failed even with the gate deleted), and a
  **dead-code mutant** (flipping a guard to `if (false)` makes the `mkdir` inside it unreachable, so
  the mutant changed nothing and "passed"). A third, `check(true, …)`, was a tautological assertion
  I wrote and deleted rather than keep to inflate the count.
- No `fsync` before `rename()`: a save is atomic but not crash-durable (Q8 asks for atomicity, not
  durability). `<unistd.h>` for `getpid()` is POSIX-specific with no precedent elsewhere in `src/`.
- The mutation harness is in `/tmp` (not a repo artifact) and was killed twice mid-run by tooling
  timeouts, leaving mutants on disk both times — it now restores from a pristine snapshot at start.

## 📦 Recent Commits

- `41184b3` `feat(serialization): split the load policy into strict structure and lenient references …` (0044, the base for this work)
- `a07a691` `feat(serialization): add the v1 scene round-trip and freeze the LoadResult contract` (0043)
