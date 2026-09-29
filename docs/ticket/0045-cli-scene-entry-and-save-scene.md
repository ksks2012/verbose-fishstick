# 0045 — CLI: `--scene=` entry mode, `--save-scene=` output, atomic file I/O

**Source:** ADR-0013 Q5 (entry mode), Q7 (output option), Q8 (atomic/overwrite), Q9 (scene name) —
tracer bullet 5. PRD user stories 6, 7, 8.
**Status:** ready-for-agent
**Blocked by:** 0043, 0044 (the pure serializer it drives)
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

- **CLI exit matrix (automated, no display):** `--scene=<valid>` → 0; `--scene=<missing file>` → 1;
  `--scene=<malformed json>` → 1; `--scene=<version 2>` → 1; `--asset`+`--scene` → 1;
  `--demo`+`--scene` → 1; `--asset`+`--demo` → 1 (unchanged); `--save-scene=<good path>` → 0 and the
  file appears; `--save-scene=<path under a missing dir>` → 1; `--save-scene=` (empty) → 1.
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
