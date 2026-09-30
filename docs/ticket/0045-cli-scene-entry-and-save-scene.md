# 0045 — CLI: `--scene=` entry mode, `--save-scene=` output, atomic file I/O

**Source:** ADR-0013 Q5 (entry mode), Q7 (output option), Q8 (atomic/overwrite), Q9 (scene name) —
tracer bullet 5. PRD user stories 6, 7, 8.
**Status:** complete 2026-09-30 (uncommitted at the user's request)
**Blocked by:** 0043, 0044 (both complete)
**Related:** ADR-0013 Q5/Q7/Q8/Q9, ADR-0010 Q8 (CLI entry model, amended by ADR-0013), ADR-0011 Q6,
`src/app/cli_options.hpp` / `cli_options.cpp` (pure-syntax contract), `src/app/main.cpp:267-285`
(entry-mode gating), `src/app/editor_entry.cpp:66-135` (`buildEntryScene`), 0025 (CLI parse split)

## What it delivers

The two CLI affordances and the real-files path that turns the pure serializer into a usable
round-trip. The layering stays exactly as ADR-0013 Q6 requires: **file I/O and deserialization live
in the app layer**, `Editor` remains an in-memory facade, and the frozen observability contract is
*reused* rather than extended.

## Scope

- **`Options` / `parseOptions` (pure syntax).** `cli_options.hpp:14-19` gains
  `std::optional<std::string> scenePath;  // --scene=<path>` and
  `std::optional<std::string> saveScenePath;  // --save-scene=<path>`. Parse both; reject an empty
  value (same rule as `--asset=`); unknown flags and the `--frames=` rules are unchanged. Extend
  `tests/app/cli_options_test.cpp` in its existing style. Keep the layering: **mutual exclusion is
  not** decided in the parser.
- **Third entry mode (Q5).** In `main()`'s pre-GL gate (`main.cpp:278-283`), reject **any two** of
  `--asset` / `--demo` / `--scene` with a `NOTO3D_CRITICAL` diagnostic **naming the offending
  combination** and `EXIT_FAILURE`, before any GL context. The existing `--asset`+`--demo` pair must
  still exit 1 (the close-matrix `excl` row asserts only the exit code, so the message is
  free-form).
  `--list-assets` keeps its pre-GL short-circuit; the default (`m4_vertical_scene`) is unchanged.
- **`--scene=` branch in `buildEntryScene` (Q9).** Read + deserialize the document; on failure emit
  a diagnostic and return `std::nullopt` (→ non-zero exit, no GL frame). On success:
  `entry.editor.replaceScene(std::move(scene))`; `entry.sceneName` = the **file stem**
  (`path/to/myscene.scene` → `myscene`); `entry.resolve` = the registry-backed resolver, exactly as
  the default branch uses (a loaded `assetId` that is unknown or unresolved is skipped by the M4
  seam); `entry.windowTitle` = `sceneName + " | " + kTitleSuffix`. The existing
  `M4_INFO rendering scene '<name>' entities=<N>` line (`editor_entry.cpp:131-132`) fires unchanged
  for this mode too — it is **reused**, not paralleled.
- **`--save-scene=` output (Q7).** After the entry scene is built successfully and **before the
  frame loop**, if `saveScenePath` is set, write the entry scene once via `Editor::scene()` (no
  new Editor API, Q6). It is **not** part of the exclusion set and combines with any entry mode.
  On failure:
  diagnostic + non-zero exit, still pre-render. Emit an `M6_INFO` save line (path + entity count).
- **File I/O in the app layer (Q6).** New `src/app/scene_file_io.{hpp,cpp}` wrapping the pure
  `serializeScene` / `deserializeScene` from `noto3d_serialization`, with
  `<filesystem>`/`<fstream>`.
  This is where unreadable-file handling lives (0044 deliberately had no real files).
- **Atomic, silent-overwrite save (Q8).** `writeSceneFileAtomic(path, text)`: write a temp file in
  the **destination directory**, then `rename()` into place (POSIX atomic replace). An existing
  destination is **overwritten silently** — no prompt, no `--force` gate. A **missing parent
  directory** is a diagnostic + non-zero exit; **never** `mkdir` a directory tree.
  - **The temp name must be unique per attempt** — e.g. `.<stem>.tmp.<pid>.<random>` (or an
    equivalent). A fixed `out.scene.tmp` would let two concurrent saves, or a re-entrant save,
    clobber each other's partial file and then `rename()` the wrong bytes into place. Uniqueness is
    what makes the rename atomic *in practice*; `rename()` alone only guarantees atomicity for a
    single writer.
  - **Clean up a failed temp**: on any write error, remove the temp file before returning the
    failure, so a failed save leaves **no** residue beside the previous good document.
- **New observability (Q9).** `M6_INFO` lines for the load and save events, in this exact shape so
  the 0046 close matrix has a fixed string to assert:

  ```
  M6_INFO scene-loaded path='<path>' entities=<N>
  M6_INFO scene-saved  path='<path>' entities=<N>
  ```

  (`scene-saved` is space-padded to align with `scene-loaded`; keep that alignment.) These are
  **event** lines, not scene identity — do **not** invent a parallel "rendering scene" line:
  `M4_INFO rendering scene '<name>' entities=<N>` stays the single shape asserted across all entry
  modes. On a **failed** load or save, emit a `NOTO3D_CRITICAL` diagnostic (naming the defect /
  path) and exit non-zero — never an `M6_INFO` success line for a failed operation.
  The milestone tag stays frozen at `(M4)`: the title is still `<name> | noto3d (M4)` (M5 shipped
  without bumping it; changing the tag is a separate decision, not a milestone side effect).

## Invariants

- **No in-editor file UI** (Q0): no file menu, no file picker, no dirty state, no Save-vs-Save-As.
  The CLI is the only save/open affordance in M6.
- **No new dependency** (Q0). The file I/O uses the standard library.
- **The M5 type boundary holds**: load hands the editor a `Scene` **by value**; the app never
  obtains
  a mutable `Scene&`/`Entity*`.
- **`--asset`, `--demo`, `--list-assets`, and the default scene behave exactly as before** (user
  story 7) — this is additive.
- Selection is never persisted, so a loaded scene starts with nothing selected (Q2).

## Explicitly NOT in scope

- Any in-editor Open/Save menu, keyboard shortcut, or file dialog (Q0).
- `--new` / `--empty` entry modes (Q10); a `--force` overwrite gate (Q8); `mkdir` of parent
  directories (Q8).
- Persisting selection, camera, viewport, or layout (Q2).
- Extending the close matrix with M6 rows or the manual smoke — that is 0046.

## Verification

- **CLI exit matrix:** `--scene=<valid>` → 0; `--scene=<missing file>` → 1;
  `--scene=<malformed json>` → 1; `--scene=<version 2>` → 1; `--asset`+`--scene` → 1;
  `--demo`+`--scene` → 1; `--asset`+`--demo` → 1 (unchanged); `--save-scene=<good path>` → 0 and the
  file appears; `--save-scene=<path under a missing dir>` → 1; `--save-scene=` (empty) → 1.
  **Amended during implementation: this matrix is only partly runnable without a display.** The
  *exclusion* gate is pre-GL, so all three `--scene=` combinations are CTest rows
  (`cli-scene-asset-exclusive`, `cli-scene-demo-exclusive`, `cli-scene-asset-demo-exclusive`).
  The *load* and *save* rows are not headless: an entry `Scene` needs the `AssetRegistry`, whose GPU
  cache requires a live context (ADR-0010 Q5 + 0025's lifetime rule), so those rows need a GL
  context and belong to 0046's M6 matrix. They were verified here by hand against the real binary in
  both presets. The original "automated, no display" wording was true of the M4 rows and false of
  these, and is corrected rather than left to imply coverage that does not exist.
- **`--save-scene=` combines with an entry mode** (e.g. `--demo --save-scene=out.scene` → 0) — it is
  an output option, not a mode.
- **Write-then-reload check:** save a scene, then load the produced file and confirm the entities
  and
  transforms match (a real round-trip through the CLI, not just a unit test).
- **Atomic/overwrite:** saving twice over the same path leaves exactly one complete, valid document
  (no temp files left behind in the directory); the prior good file survives a failed write (e.g.
  unwritable/missing dir leaves the previous document intact).
- **`M4_INFO` reuse:** the log line for a `--scene=` run is the *same* shape as the other entry
  modes
  with the file-stem name; the `xprop` `WM_NAME` title is `<stem> | noto3d (M4)`.
- `cli_options_test` extended and green; ctest green dev + release; `scripts/m5-close-matrix.sh` ALL
  PASS (its existing rows must be **unchanged** — only new rows arrive in 0046).

**Rolls under spec:** ADR-0013 Q5, Q7, Q8, Q9; PRD user stories 6, 7, 8.

## Evidence (2026-09-30)

- **New app-layer file I/O seam.** `src/app/scene_file_io.{hpp,cpp}` — `readSceneFile()`,
  `writeSceneFileAtomic()`, `saveSceneTo()`, `loadSceneFrom()`, `sceneNameFromPath()`. It is the
  **only** place that touches the filesystem, and it is the only consumer of
  `deserializeScene()` outside the serializer's own tests. `Editor` stays I/O-free (ADR-0013 Q6)
  and **no new `Editor` API was needed**: load goes through the 0042
  `Editor::replaceScene(std::move(...))`, save reads the existing `Editor::scene()`. The
  `noto3d_serialization` arch guard from 0041 is untouched, so the app still cannot be pulled
  toward registry/GL by the I/O path.
- **Q8 atomicity.** The temp file is created in the **destination directory** (not `/tmp`), because
  `rename()` is only atomic *within* one filesystem — a temp elsewhere would silently degrade the
  rename into a non-atomic copy. Its name is `"." + stem + ".tmp." + pid + "." + counter`, unique
  per process *and* per attempt, so a re-entrant save cannot clobber another attempt's partial
  file. The destination is opened only at `rename()` time, so **any** earlier failure leaves the
  previous good document untouched; overwrite is silent (no `--force`); a missing parent directory
  is a diagnostic + non-zero exit and **never** a silent `mkdir`; the temp is removed on every
  failure path.
- **Q9 identity.** A `--scene=` run reports the **file stem**, so `M4_INFO rendering scene '<stem>'
  entities=N` and the title `<stem> | noto3d (M4)` are reused with no new log shape — the milestone
  tag stays frozen at `(M4)` per the 0024 precedent. `M6_INFO scene-loaded` / `scene-saved` are
  space-aligned new lines for the I/O events that have no M4 analogue.
- **Q5 exclusion.** `main.cpp` now **counts** the three modes rather than pairing them, because
  with three modes a pairwise chain silently misses one pair. The gate stays **pre-GL**, names all
  the offending flags in one diagnostic, and the registry-resolver construction is now a single
  shared `registryResolver()` used by every mode — one policy, not three copies (ADR-0012 ③).
- **Verification.** ctest **24/24** dev + release (23 → 24; new headless `app-scene-file-io`
  suite, 27 assertions, dependency-free), clean builds 0 warnings, `scripts/m5-close-matrix.sh`
  **ALL PASS** dev + release, and the CLI exit matrix above re-run by hand with the write-then-
  reload round-trip through the real binary.
- **12/12 mutations caught, 0 vacuous** (red-green proof across both headless suites, the CLI
  matrix, and the new CTest exclusion rows). Three mutants are **dropped as unprovable and recorded
  as known gaps rather than counted**: staging the temp in `/tmp` (needs a second filesystem),
  ignoring the `close()` error (needs `ENOSPC`), and **losing per-attempt temp-name uniqueness**.
  The last one had a test written for it — seeding the exact path a fixed-name implementation would
  occupy — and the review deleted it, correctly: it pinned the *mutant's text* rather than the
  behaviour, so it would have passed whether or not uniqueness held. A temp name is unobservable
  between creation and `rename()` without racing threads, so the claim is recorded, not asserted.
- **Review findings fixed** (both axes, `/code-review` at `41184b3`):
  - **ADR-0013 Q3 was unmet and this change is what made it due.** Q3 (line 177) requires "the app
    layer logs a warning" for an `assetId` that is unresolvable; nothing warned. Before `--scene=`
    every Scene came from a fixture with known-good ids, so there was no trigger. `buildEntryScene`
    now warns **once per unresolved id** (not per frame in the draw path), keeping Q3's other half —
    kept as data, skipped at render, load still succeeds. Verified against a real unresolved id.
  - **The entry-mode set was duplicated.** The count was a hand-written sum of three conditionals
    *and* the message repeated the same three flags in a second block, so a fourth mode would have
    to be added twice with nothing catching a miss. One table now drives both, and the mutants
    include "an inactive mode is counted anyway".
  - **Duplicate diagnostics removed** — one save failure logged two `M6_FAIL` lines (the second
    naming only the destination and dropping the cause) and one load failure logged three.
  - **`is_directory`'s `ec` was discarded**, so `EACCES` was reported as "does not exist".
  - **The GL-free claim was conventional, not enforced.** The new suite links `noto3d_core` for the
    `NOTO3D_*` facade and `noto3d_core` PUBLIC-exports glad's include dir, while the CMake comment
    asserted no GL — the comment class 0043's review rejected. It now carries the same
    `noto3d_assert_target_reaches_no_gl()` guard as the registry/serialization suites, and the guard
    was **proven to fire** (a `#include <renderer/mesh.hpp>` probe aborts configure).
  - Small ones: `M4_FAIL` → `M6_FAIL` on the new exclusion; the parser no longer promises a
    `.scene` extension it never validated; a comment claimed a "non-empty directory" where the test
    creates an empty one; an unasserted `sentinel` is now asserted; the three new files gained
    trailing newlines.
- **Known gaps, stated rather than papered over:** no `fsync` before `rename()`, so a save is atomic
  but not crash-durable (Q8 asks for atomicity, not durability); `<unistd.h>` for `getpid()` is
  POSIX-specific with no precedent elsewhere in `src/`; the three temp-name mutants above.
- **Three defects the red run surfaced** — all in the *verification*, none in the production code:
  1. **A false-green CLI row.** `--asset`+`--scene` was asserted as "exit 1" while pointing at a
     **non-existent** file, so the row passed because the *load* failed even with the exclusion
     gate deleted — the gate was effectively untested. Both exclusion rows now use a **valid**
     document, and a dedicated assertion checks the diagnostic names `--asset`, `--demo` and
     `--scene` rather than accepting any exit 1. Verified discriminating: the mutated gate returns
     0 where the real gate returns 1.
  2. **A dead-code mutant.** "missing parent is `mkdir`'d instead of rejected" was first written by
     flipping the guard to `if (false)`, which makes the `mkdir` inside that block unreachable — the
     mutant changed nothing and "passed" vacuously. The real mutant keeps the `mkdir` and drops
     only the rejection.
  3. **A tautological assertion.** An intermediate attempt to cover cross-filesystem staging was
     written as `check(true, …)`; that is precisely the vacuous-assertion class the 0043/0044
     reviews rejected, so it was deleted rather than kept to inflate the number.
- **One deviation from the ticket text, deliberate.** The Verification section says the close
  matrix's existing rows must be *unchanged*. They are unchanged in **output and exit codes**, but
  one static assertion had to be re-expressed: it required `registry.model(` to appear **exactly
  twice**, a count that exists only while there are two entry modes, so adding `--scene=` made it
  fail for the right reason. It now asserts the *intent* — at least one call, and every occurrence
  inside the resolver's `return`. Same rows, same passes; the intent is now stated directly instead
  of encoded as a magic constant. 0046 re-verifies this script.
