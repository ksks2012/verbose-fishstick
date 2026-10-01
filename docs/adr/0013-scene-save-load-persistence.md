# ADR-0013 — M6 Save/Load: `.scene` snapshot persistence over the `Editor` facade

Status: Accepted — implemented by 0041–0046 (all archived); M6 closed 2026-10-01

> Per DOCUMENTATION-LIFECYCLE: **specs (PRDs) live at the ADR layer**; `docs/ticket/` holds
> execution tickets derived from accepted ADRs. This ADR embeds the M6 milestone spec (Problem /
> Solution / User Stories / Testing / Out of Scope) below; the M6 execution tickets will roll
> under it. M3's spec is embedded in **ADR-0009**, M4's in **ADR-0010**, M5's in **ADR-0011**.

## Context

M4 (0020–0025) and M5 (0028–0034) are closed, and the M6 gate that ADR-0012 opened is now shut
(0027 decided the ownership model; 0036–0039 delivered it, and the close-out verified that the whole
chain changed no externally observable behaviour). M6 is the next milestone on the roadmap: let the
user **keep** the scene they build, and **reopen** it later.

The shape M6 inherits is unusually favourable, and it is worth stating exactly what it is:

- **`Scene` is pure data** (ADR-0010 Q1) — `{ EntityId, name, assetId, Transform }` and nothing
  else. It never names a `Model`, GPU object, renderer state, or logger. A serialized Scene is
  therefore a direct projection of the data model, not a new schema invented for persistence.
- **`Editor` owns the `Scene` by value** and is the only write path; no caller can obtain a mutable
  `Scene&`, `Entity*`, or span (ADR-0011 Q11). ADR-0011 called this "M6's ready-made
  snapshot-replace boundary", and it is the reason the load-injection question below has a cheap
  answer.
- **There is exactly one resolution seam**, `app::collectDrawItems(scene, resolver)`, and an
  unresolved `assetId` is **skipped at render and never invalidates the Scene** (ADR-0010 Q5).
- **A forward contract already exists** in `DEFINITION.md`'s **`Scene snapshot`** entry: a Scene file
  is a *complete snapshot*; loading *replaces* the whole Scene including id-allocation state;
  `nextEntityId = max(restored ids) + 1`. (This ADR sharpens and re-words that entry — see **Q1** —
  so the entry is amended in the same change; the *decision* it recorded stands.) ADR-0011 explicitly
  deferred "Save/load, scene serialization, `--scene=`" to M6
  (`docs/adr/0011-m5-editor-architecture.md:402`).
- **Persistence was anticipated by ADR-0008, not invented by M6.** The namespace
  `noto3d::serialization` (`docs/adr/0008-m0-code-contract.md:11-12`), the module directory
  `src/serialization/` (`:13`), and "headless asset/registry/**serialization** tests" (`:20`) were all
  reserved at M0. The directory exists and has **always been empty** — no code has ever been written
  to it. ADR-0008 also fixed the failure convention this milestone follows: "internal failures use
  `Result`/diagnostics (echoes ADR-0007 tri-state)" (`:15`), which is why load reports diagnostics
  rather than throwing, and why the render loop stays no-throw.

Four facts were established by inspection before the decision session began, and are recorded here
so that no ticket re-opens them. They are **not** decisions:

- **Format is JSON.** `DEFINITION.md`'s pre-M6 `Scene snapshot` entry already called the document a
  "Scene **JSON** file";
  nlohmann/json `v3.11.3` is already a pinned dependency (ADR-0003); `manifest.json` is JSON; and
  `asset_cpu.cpp:53-56` already parses JSON with it. A binary format would be a second codec with no
  consumer.
- **There is no file-writing code in `src/` at all today.** `grep` for `ofstream`/`.dump(` finds it
  only in `tests/`. Save is genuinely new machinery, not an extension of an existing writer.
- **The CLI→Scene entry model is a frozen contract**: `--list-assets` short-circuits pre-GL, then
  `--asset` + `--demo` exits 1 (`main.cpp:275-283`), and `CONTEXT.md` records "**no `--scene=`**"
  as part of M4's close.
- **The observability contract is frozen too**: the window title is `<name> | noto3d (M4)` and
  `M4_INFO rendering scene '<name>' entities=<N>` is asserted per entry mode by
  `scripts/m5-close-matrix.sh:77-100`.

What did **not** exist was any decision about: what a saved document *is*; what a failure means;
how the id allocator survives a round-trip; how a file is versioned; how load reaches the Editor
without breaking its encapsulation; when and how a write happens; and where the code lives in the
target graph. Each of those is a fork that the first ticket to touch it would otherwise settle by
accident, which is what this ADR prevents.

> **Decision identifiers** are the grilling session's, kept verbatim for traceability:
> **Q0–Q11**. They are in the order asked. Nothing is missing: two questions from the pre-session
> draft list turned out to be already-decided facts (the JSON format, and `EntityId` persistence),
> one turned out to be half-decided (unresolved `assetId` — the render-time half was locked, the
> load-time half was not), and the `undefined-empty-scene` question was folded into **Q10**.

## Decision (locked 2026-09-29, grilling Q0–Q11)

### Q0 — M6 delivers a persistence core and a CLI round-trip, not an in-editor file UI

M6's user-reachable surface is: load a document from the command line, and write a document from the
command line, both over a headless serialization library. M6 ships **no** in-editor Open/Save menu,
no keyboard shortcut, and no file picker.

- **Rejected:** a full in-editor document editor. It forces a file-picker decision immediately —
  `lib/` contains no file-dialog library and there is none in `CMakeLists.txt` — so it would drag
  in a new dependency (ADR-0003 constrains new deps) plus dirty-state tracking and a Save-vs-Save-As
  distinction, none of which the format needs in order to be frozen. That is a ticket of its own,
  not a prerequisite.
- **Rejected:** library + Editor API with no CLI. It would give M6 no user-reachable feature and
  would contradict ADR-0011's explicit deferral of `--scene=` to M6.

This also fixes the project's own precedent: M4 and M5 both closed on *headless-testable semantics +
an automated matrix + a manual smoke tier*, and M6 keeps that two-tier shape instead of becoming the
first milestone whose meaning depends on a display.

**Explicitly out of M6** (consequences of this answer): in-editor file menu, shortcuts, file picker,
dirty-state tracking, Save vs Save-As, and any new dependency.

### Q1 — The allocator is *derived* from the restored entity set; id reuse after a round-trip is accepted

Load sets `nextEntityId = max(restored ids) + 1` (empty file → `1`), exactly as `DEFINITION.md`'s
`Scene snapshot` entry already states. The file records `id` per entity and **does not record the allocator**.

The consequence is real and is accepted deliberately: create A(1), B(2), C(3) → destroy C → save
(`ids {1,2}`) → load (`next = 3`) → create D gets `id 3`, repeating the value C once held. The
argument for accepting it:

- "Never reused" is a property of **one Scene lifetime**. A load constructs a new `Scene`, so no id
  is reused *within any single Scene's lifetime*; the object that owned id 3 no longer exists.
- Nothing observes the repeat. Selection is not persisted, there is no undo/redo (ADR-0011 Q4), and
  no structure outside the `Scene` holds an `EntityId`.
- It keeps the file a **pure function of its entity set** — the strongest property available for
  diffing, hand-editing, and golden-file tests. The alternative makes two files with identical
  entities differ depending on how that session's history ran, which is exactly the kind of
  history-dependent state persistence should not carry.

**Rejected:** persisting an explicit `nextEntityId`. It costs a schema field, a `Scene` API able to
set the counter, a `next > max(restored)` validation and a second code path, to buy a guarantee
nothing in M6 consumes. **Rejected:** an optional field with a `max+1` fallback — v1 has no existing
files to be compatible with, so an optional field buys ambiguity for free.

**Required wording fix (same change).** Before this ADR, `DEFINITION.md` said `EntityId` is "the
single identity (runtime = persistent) for M6", and that loading replaces the Scene "including
**id allocation state**". Both overstate what the file records. They are amended to state that the
identity is stable *within a Scene lifetime* and that the allocator is **recomputed** from the
restored set, so a destroyed high-water id may reappear after a load. A round-trip test must pin
this behaviour explicitly (destroy the highest id → save → load → create) so it is a decision rather
than an accident.

### Q2 — One self-contained `.scene` file; assets are referenced by id, never embedded

A document is a single JSON file, extension `.scene`, shaped as a direct projection of the data
model:

```json
{
  "version": 1,
  "entities": [
    {
      "id": 1,
      "name": "smile",
      "assetId": "smile",
      "transform": {
        "position": [0.0, 0.0, 0.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale":    [1.0, 1.0, 1.0]
      }
    }
  ]
}
```

- `rotation` is **radians, YXZ** — the `Scene`'s own unit and order (ADR-0010 Q3). Degrees are a
  presentation-layer conversion (ADR-0011 Q13) and never appear in the file.
- `assetId` is a plain string resolved at render time through the existing registry seam. No
  geometry, path, sha256, or `geometry_source` is copied into the document.
- Not recorded: `nextEntityId` (Q1), asset metadata, selection, camera, `ViewportState`.

**Rejected:** a bundle that embeds referenced asset metadata. It would create a second source of
truth for asset facts, contradicting ADR-0007 ("`manifest.json` is the single source of truth"), and
it would turn "load a scene" into "load assets that are not in the manifest" — re-opening the
escape-hatch and path-predicate surface (ADR-0007's amendment) that M6 has no reason to touch.
**Rejected:** multi-file/folder project layouts, archives, or any custom container: the document is
the Scene, and the Scene is one flat list.

### Q3 — Load policy: strict on structure, lenient on references

Two independent axes, deliberately different, because they answer different questions.

**Structural errors hard-fail the whole load** — no `Scene` is produced, a diagnostic is emitted,
and the CLI exits non-zero:

- file cannot be opened / read
- malformed JSON
- `version` missing, not an integer, or not `1` (Q4)
- `entities` missing or not an array
- an entity missing a required field, or a field of the wrong JSON type
- `id == 0` (`InvalidEntityId`), or a duplicate `id` within the document
- a `transform` component that is not a 3-element array of numbers

**Semantic problems do not fail the load.** An `assetId` absent from the manifest, or whose
geometry is unresolved / `FAILED`, is kept in the `Scene` as pure data; the app layer logs a
warning; render skips it per the M4 contract (ADR-0010 Q5).

The axis split is the layering: `src/serialization/` may depend on `noto3d_scene` and nlohmann/json
and **nothing else** (Q11), so it is not in a position to know whether an asset resolves — and should
not be. "Is this a valid v1 document?" is a question about the file. "Does this asset exist on this
machine?" is a question about the environment, and a scene legitimately shared between two machines
with different manifests must still load.

- **Rejected:** failing the load on unresolvable references. It would force the serializer (or its
  caller) to depend on the registry, break cross-environment sharing, and contradict "an unresolved
  `assetId` never invalidates the Scene".
- **Rejected:** skipping malformed entities and loading the rest. It silently mutates the user's
  document, makes round-trip lossy without saying so, and is the "silent fallback" shape ADR-0007
  rejects for assets.

### Q4 — Version gate is exact; unknown keys are ignored

- `version` is **required** and must be the integer `1`. Anything else — missing, `2`, `0`, a
  string — is a structural hard-fail (Q3), with a diagnostic that distinguishes an unsupported
  version from a malformed one.
- **Unknown keys at any level are ignored**, never rejected.

The second rule keeps the format open: a future v2 that only *adds* fields can still be read for its
v1 subset by a v1 reader. This is the manifest parser's precedent — `asset_cpu.cpp:62-114` uses
`.contains()` throughout and never rejects extras — so the whole repository parses documents in one
style rather than having `manifest.json` lenient and `.scene` strict.

**Forward rule, binding on any future version:** v2+ may **add** fields; it may never rename or
repurpose a v1 field, nor change a v1 field's meaning. That is what makes "ignore unknown keys" safe.
No migration framework is built in M6 — there is nothing to migrate from.

- **Rejected:** rejecting unknown keys. It closes the format so that no field can ever be added
  without breaking every older reader, and it is hostile to hand-editing.
- **Rejected:** no version gate. A v2 file that renamed a field would be silently misparsed — the
  silent-wrong-answer class this project rejects repeatedly.
- **Rejected:** `version <= supported` plus a migration hook. Speculative machinery for a version
  that does not exist.

### Q5 — `--scene=<path>` is a third, mutually exclusive entry mode

It joins `--asset` and `--demo` in the existing exclusion set: **any two together exit 1, pre-GL**,
reusing the current rule and diagnostic style so the check stays headless-testable in the CLI matrix
(ADR-0010 Q8).

| invocation | resulting Scene |
|---|---|
| no flag | `m4_vertical_scene` (3 entities) — unchanged |
| `--asset=<id>` | single-entity scene — unchanged |
| `--demo` | `demo_checker` demo-only scene — unchanged |
| `--scene=<path>` | the loaded document |
| any two of the three | diagnostic + exit 1, before any GL context |
| `--list-assets` | existing pre-GL short-circuit; not combinable |

- **Rejected:** letting `--scene=` compose with `--asset`/`--demo`. "Load a file and append an
  entity" makes "the entry scene" ambiguous and weakens the M4 guarantee that each entry mode
  produces one deterministic, assertable `Scene`.
- **Rejected:** making `--scene=` lowest-precedence with no error. A typo like `--scene=x --aset=y`
  would quietly do the wrong thing instead of failing loudly.

**Required amendment (same change).** `CONTEXT.md` and ADR-0010 record "**no `--scene=`**" as a
frozen M4 statement; that clause is superseded here (append an amendment to ADR-0010 — ADRs are
append-and-supersede, never overwritten) and `CONTEXT.md` is updated in the same change.

### Q6 — Load injects through one new `Editor` method: `replaceScene(Scene)` by value

`Editor` gains exactly one member:

```cpp
void replaceScene(scene::Scene scene);  // by value, moved in
```

- It takes a fully-formed `Scene` **by value** and moves it in, then **clears the selection** —
  exactly as `destroyEntity`/`clear` already do, so a selection can never dangle into a document
  that no longer contains it.
- The allocator needs **no public setter**: the serializer has already derived `nextEntityId`, and
  the whole value carries it in.
- It returns nothing. Errors are the app layer's business and are settled before this call.

**File I/O and deserialization stay in the app layer** (`editor_entry` / `main`): it reads the
file, deserializes, handles the diagnostic and the exit code, and only then hands over a value. So
`Editor` stays an in-memory semantic facade — no `std::filesystem`, no nlohmann, no path — and stays
headless-testable with an in-memory `Scene` and no real files.

**Save needs no new `Editor` API at all.** `Editor::scene()` already returns
`const scene::Scene&` for the render seam, which is exactly the signature a serializer wants, so it
is reused rather than duplicated.

- **Rejected:** `Editor::loadScene(path)` doing the I/O internally. It grows a filesystem + JSON
  dependency into the facade, destroys the "pure semantic object" property, and forces the Editor's
  headless tests to create real files.
- **Rejected:** a free `loadSceneInto(Editor&, path)`. Installing a whole `Scene` into an `Editor`
  that exposes no mutable `Scene&` requires either a friend/back-door (breaking encapsulation) or
  entity-by-entity rebuild — and rebuilding **cannot restore the persisted ids**, which defeats the
  purpose. This is the one option that would have silently destroyed Q1's whole point.

### Q7 — `--save-scene=<path>` is an output option, not a fifth entry mode

It does **not** join the mutual-exclusion set, so it combines with any entry mode. It writes the
entry scene **once, immediately after the entry scene is built and before the render loop**, so the
result never depends on `--frames` or on the window surviving.

```bash
noto3d-editor --scene=in.scene --save-scene=out.scene --frames=1   # pure round-trip
noto3d-editor --save-scene=default.scene --frames=1                # dump the default scene
noto3d-editor --asset=smile --save-scene=smile.scene --frames=1    # dump a single-asset scene
```

A write failure is a diagnostic plus a non-zero exit, and happens **before** rendering, so a failed
save never leaves a half-run session claiming success.

- **Rejected:** making it a second pre-GL subcommand like `--list-assets`. Maximally headless, but
  it adds a second early-exit path to maintain and cannot express "load a scene, re-save it
  elsewhere" in one invocation.
- **Rejected:** implicit save on exit. It makes saving a side effect of quitting, which is the
  opposite of what "Save" means, and is awkward to test.

### Q8 — Save is atomic and overwrites silently

- Write to a **temporary file in the destination's own directory**, then `rename()` it into place.
  On POSIX, same-filesystem `rename` is atomic, so an interrupted or failed write can never leave a
  truncated document where a valid one used to be.
- An **existing target is overwritten silently**. The path was named explicitly on the command line;
  adding a `--force` gate adds CLI surface to defend against a mistake the user cannot make by
  accident. (Rejected: refusing to overwrite — it makes the repeated `in → out` round-trip workflow
  require a manual `rm` between every run.)
- A **missing parent directory is a diagnostic + non-zero exit**. The app never `mkdir`s on the
  user's behalf.

### Q9 — A loaded scene reports its file stem; the milestone tag stays frozen

`--scene=path/to/myscene.scene` → `sceneName = "myscene"`. Therefore the two frozen contract lines
keep their exact shape:

```
M4_INFO rendering scene 'myscene' entities=3
title: myscene | noto3d (M4)
```

so the close matrix gains a row rather than a parallel format. The stem is short enough for a title
bar, stable, and distinguishes two different files.

The **milestone tag stays `(M4)`**. M5 shipped without bumping it and its titles were verified
unchanged, so "the tag is frozen, not tracked" is the established precedent. Changing it is a
separate decision, not an M6 side effect.

- **Rejected:** the full path as `sceneName` — unbounded, machine-specific, unreadable in a title
  bar, ungreppable.
- **Rejected:** a fixed literal like `scene`/`loaded` — two different documents would be
  indistinguishable in both the log and the title, weakening the identity line M3/M4 established.

### Q10 — An empty document is valid; M6 adds no way to *create* one from nothing

`{"version": 1, "entities": []}` is a valid document. It loads to an empty `Scene` with
`nextEntityId = 1`, renders nothing, and logs `entities=0`.

M6 adds **no** flag that produces an empty scene. With no flags the default entry mode remains
`m4_vertical_scene`, unchanged. An empty scene is reachable only by saving a scene the user emptied
(possible via `clear()`), not by asking for one on the command line — adding a `--new`/`--empty`
mode would be a fourth entry mode for no user need this milestone.

### Q11 — A new `noto3d_serialization` target, with the layering made machine-checkable

```cmake
add_library(noto3d_serialization STATIC src/serialization/scene_io.cpp)
target_link_libraries(noto3d_serialization PUBLIC noto3d_scene PRIVATE nlohmann_json)
```

The namespace is `noto3d::serialization` and the suite lives in `tests/serialization/` — both
already reserved by ADR-0008 (`:11-13`, `:20`), so M6 delivers the layer the project planned rather
than inventing a home for it.

plus a **configure-time arch guard**, in the discipline ADR-0012 established, asserting that
`noto3d_serialization` links **none** of `noto3d_asset`, `noto3d_asset_cpu`, `noto3d_renderer`,
`noto3d_core`, `glad`, or `libGL`. The guard derives its inputs from the target's own CMake
properties, not a hand-written list (a hand-written list once drifted and hid a live ODR bug during
0037).

The guard is the point. Q3's whole correctness argument is that the serializer *cannot* see the
asset registry; without the guard that is a convention, and conventions are exactly what the 0036
chain was created to replace. A guard that fires is worth more than a comment that says "keep it
pure".

**Consequence — a serialization-local result vocabulary.** `asset::RegistryResult` / `LoadStatus`
(`manifest_types.hpp`) cannot be reused: they live in `noto3d_asset_cpu`, which the guard forbids.
`noto3d::serialization` therefore defines its own minimal load/save result — a success flag plus a
diagnostic list — which is the `Result`/diagnostics convention ADR-0008 already fixed (`:15`), not a
new idea; only the vocabulary is local. Note deliberately: `noto3d_core` is **also** forbidden — it
carries glad's include directory — so even an apparently harmless shared header there would break
the guard. Errors surface as diagnostics, never as exceptions crossing into the render loop.

## Rejected alternatives

- **In-editor Open/Save with a file dialog** (Q0) — no file-dialog library exists in `lib/`; it
  forces a new dependency plus dirty state and Save-vs-Save-As, before the format is even frozen.
- **Binary format** (pre-session fact) — a second codec with one consumer; `DEFINITION.md` already
  names the document JSON.
- **Persisting the allocator / optional `nextEntityId`** (Q1) — a schema field, a counter setter, a
  validation branch and a second test path, to guarantee nothing in M6 consumes, at the cost of the
  file no longer being a pure function of its entities.
- **Failing the load on unresolvable assets** (Q3) — breaks cross-environment sharing, contradicts
  ADR-0010 Q5, and forces the registry dependency the guard forbids.
- **Skipping malformed entities** (Q3) — silent document mutation, lossy round-trip.
- **Closed schema rejecting unknown keys** (Q4) — freezes the format; diverges from the manifest
  parser; hostile to hand-editing.
- **Version tolerance + migration hooks** (Q4) — speculative.
- **Composable / precedence-based `--scene=`** (Q5) — ambiguity, and a silently wrong result on
  typos.
- **`Editor::loadScene(path)`** (Q6) — filesystem + JSON in a semantic facade; forces file-based
  tests.
- **Free function installing into `Editor`** (Q6) — needs a back-door, or loses the persisted ids.
- **Pre-GL save subcommand** / **implicit save on exit** (Q7) — a second early-exit path; or saving
  as a side effect of quitting.
- **Refusing to overwrite** (Q8) — friction on the round-trip workflow, no real hazard.
- **Full path or a fixed literal as `sceneName`** (Q9) — unreadable in a title bar, or
  indistinguishable documents.
- **A `--new`/`--empty` entry mode** (Q10) — a fourth entry mode for no current need.
- **Folding serialization into `noto3d_scene`/`noto3d_app` with no guard** (Q11) — keeps the
  layering as a convention, contradicting the discipline 0036–0039 just established.

## Consequences

- **`DEFINITION.md` must be amended in this change** (Q1, Q2, Q4, Q10): the `EntityId` "runtime =
  persistent" wording, the "id allocation state" claim, the `Scene snapshot` entry's
  `max(restored ids) + 1` rule (now with its accepted consequence), and the new v1 document shape.
- **`CONTEXT.md` must be updated in this change** (Q5, Q9): the **current-milestone M4 summary** line
  "no `--scene=`" becomes "`--scene=<path>` is a third mutually exclusive entry mode". The other two
  occurrences — the ADR-0010 decision summary and the archived 0022 per-ticket record — are
  deliberately **left as history** (append-and-supersede, same rule as ADR-0010's own Q8 text); the
  ADR-0013 entry directly below the ADR-0010 summary records the supersession. ADR-0010 also gains an
  **amendment section** (never an in-place edit), and the "Next on the roadmap: M6" line becomes the
  locked M6 spec.
- **The frozen observability contract is preserved, not extended**: `--scene=` reuses the existing
  `M4_INFO` line shape and title shape, so `m5-close-matrix.sh` gains rows rather than a new format.
- **A new CLI matrix dimension is required**: valid `--scene=`, each structural-failure exit, the
  mutual-exclusion exits, and the `--save-scene=` write-then-reload check.
- **`Scene` gains a bulk-restore/replace capability.** `nextEntityId_` is private with no setter
  (`scene.hpp:70-73`) and `createEntity` always allocates fresh ids, so a restore path is genuinely
  new surface on `Scene` — it must take entities *with* their ids. It must **not** grow a public
  arbitrary counter setter (Q1); the derived value arrives with the entities.
- **Selection is not persisted** (Q2), so a load always starts with nothing selected, and
  `replaceScene` clears selection (Q6) to keep that true even for a future in-session load.
- **A `demo_checker` id in a saved document will not render.** That fixture is out-of-manifest and
  resolved only by the `--demo` resolver closure (`editor_entry.cpp:71-95`); a document loaded
  through the normal registry resolver reports it as an unresolvable reference and is skipped per
  Q3. This is correct behaviour, and worth stating so it is not mistaken for a bug later.
- **CTest count rises** from 22 with new round-trip and per-failure-case entries.

## Spec: M6 Save/Load (PRD)

### Problem Statement

The runtime can compose a scene, and the editor can now edit it, but nothing the user builds
survives the process. Closing the app discards every entity, every transform, and every
asset assignment; the only way to reproduce a composition is to redo it by hand through the UI, and
the only way to share one is to describe it in prose. `Scene` is already pure data with stable
`EntityId`s, and the `Editor` already owns it by value behind a facade that cannot be bypassed —
so the data model is settled and the milestone is purely about giving it a durable, restorable form
and a way in and out of the running editor.

### Solution

Add a headless serialization layer that projects `Scene` to and from a single versioned `.scene`
JSON document, and wire it into the runtime through two orthogonal CLI options: `--scene=<path>` as
a third mutually exclusive entry mode, and `--save-scene=<path>` as an output option that writes
the entry scene before the render loop. Load reaches the editor through a single new by-value
`Editor::replaceScene(Scene)`, preserving the facade's guarantee that no caller can obtain a mutable
`Scene&` or `Entity*`. Structural document errors fail loudly with a diagnostic and a non-zero exit;
unresolvable asset references are warned about and skipped, continuing the M4 contract that an
unresolved `assetId` never invalidates the Scene. Saves are atomic (temp file + `rename`) and
overwrite silently.

### User Stories

1. As a user, I want to save the scene I have built, so that closing the app does not throw my
   work away.
2. As a user, I want to reopen a saved scene and get back exactly the entities, names, asset
   assignments, and transforms I saved, so that a composition is reproducible.
3. As a user, I want a corrupt or unsupported file to be refused with a clear message and a
   non-zero exit, not silently loaded as something else, so I never mistake a failed load for a
   successful one.
4. As a user, I want a scene that references an asset this machine does not have to still open —
   with that entity skipped rather than the whole file refused — so a scene I can share with
   someone else is not unusable on my machine.
5. As a user, I want the document to be readable and hand-editable JSON, so I can inspect it, diff
   it, and fix a mistake in it without the app.
6. As a user, I want an interrupted save to leave my previous file intact, so a crash mid-write
   never destroys a good document.
7. As a user, I want `--scene=` and my existing `--asset` / `--demo` shortcuts to keep working
   exactly as before, so nothing I already do changes.
8. As a user, I want a wrong combination of flags to fail immediately and loudly instead of quietly
   doing something else, so a typo never costs me an edit.
9. As a developer, I want the serializer testable with no display, no GL context, and no real files,
   so M6 closes on automated evidence like M4 and M5 did.
10. As a developer, I want the editor facade to still be incapable of handing out a mutable `Scene&`
    or `Entity*`, so persistence does not quietly turn the M5 type boundary into a convention.
11. As a developer, I want the serializer physically unable to reach the asset registry, so the
    "structure is strict, references are lenient" split cannot rot into a half-AssetRegistry.
12. As a maintainer, I want the format versioned from the first document so a future v2 has a
    defined relationship to v1, even though no migration is needed yet.

### Testing Decisions

- **What good looks like** — external behaviour only: the bytes of a written document, whether a
  round-trip reproduces the `Scene` exactly, whether each structural failure is refused, and the CLI
  exit-code/log-line matrix. No readback, no screenshot, no image similarity (the M3/M4/M5 exclusion
  continues).
- **Round-trip is the central test**, and it must be a genuine round-trip, not a decode-only test:
  build a `Scene` → write → read → compare every field of every entity, including ids and
  float-exact transform values.
- **The Q1 consequence must be pinned by a test, not left implicit**: destroy the highest
  id → save → load → create → assert the new id is `max+1` and that it may equal the destroyed
  value. If that assertion ever changes, it is a decision, not a drift.
- **Structural-failure cases each get their own case**: unreadable file, malformed JSON, missing
  `version`, `version != 1`, non-array `entities`, missing entity field, wrong field type, `id == 0`,
  duplicate `id`, malformed `transform`. Each asserts a diagnostic **and** that no `Scene` comes
  back.
- **Forward-compatibility cases**: a document with an extra unknown key at both the envelope and
  entity level loads successfully and is ignored (Q4); a document with `entities: []` loads to an
  empty `Scene` with `nextEntityId == 1` (Q10).
- **Unresolvable-reference case**: a document naming an `assetId` not in the manifest loads
  successfully, keeps the entity in the `Scene`, and is skipped at resolution (Q3).
- **Seams** — `noto3d_serialization` links `noto3d_scene` + nlohmann/json only, so its suites run
  headlessly with no display and no GL context. `Editor::replaceScene` is tested with an in-memory
  `Scene`, needing no file I/O (Q6). `cli_options` parsing is extended for `--scene=` /
  `--save-scene=` in the existing `tests/app/cli_options_test.cpp` shape.
- **The arch guard is itself a test**: like 0037/0038, the guard must be *proven to fire* (a
  deliberately forbidden link fails configure) and not merely present.
- **Process-level verification is two-tier** (continuing ADR-0010 Q8 / ADR-0011):
  - **Automated (dev + release)**: CTest green with the new entries in the count; the extended CLI
    exit-code and structured-log matrix (including `M6_INFO` load/save lines and the reused
    `M4_INFO` line for a loaded scene); a write-then-reload check on the CLI matrix; the existing
    M4/M5 matrix rows and `xprop` titles **unchanged**.
  - **Manual / platform-specific evidence (recorded in the ticket)**: opening a saved document in
    the real editor and confirming the entities and transforms match, i.e. the same "manual
    confirmation" standard 0024 used, not a rendering-correctness test.
- **Prior art** — the `check()` / `ALL PASS` runner; 0025's `cli_options_test.cpp`; 0021's
  `tests/app/` seam suite; 0017's exit-code + `WM_NAME` verification; 0037/0038's proven-fires arch
  guards and their red-green regressions.

### Out of Scope

- **In-editor Open/Save menu, keyboard shortcuts, and any file picker** (Q0) — no file-dialog
  library exists in `lib/`, and ADR-0003 constrains new dependencies. A follow-up milestone may add
  UI on top of the format frozen here.
- **Dirty-state tracking** ("unsaved changes" indication) and **Save vs Save-As** (Q0) — both
  presuppose an in-editor save affordance.
- **New dependencies of any kind** (Q0).
- **Undo/redo** (ADR-0011 Q4) — unchanged; there is still no command history, so a save is a
  snapshot of current state and nothing else.
- **Multi-file / folder project layouts, archives, and custom containers** (Q2).
- **Embedding asset metadata, geometry, paths, or hashes in a document** (Q2) — ADR-0007 keeps
  `manifest.json` the single source of truth.
- **Persisting the id allocator** (Q1), and therefore cross-session monotonic ids.
- **Persisting editor state**: selection, camera, `ViewportState`, panel layout, or docking (Q2).
- **A migration framework** (Q4) — no older version exists to migrate from.
- **`--new` / `--empty` entry modes** (Q10).
- **A `--force` overwrite gate** (Q8).
- **Screenshot/readback/image-similarity** (the M3 exclusion continues).
- **Scene graph, prefabs, ECS, and multi-view layouts** (M4/M5 exclusions continue).

### Further Notes

- The **arch guard is the most valuable single line in this ADR**. Everything in Q3 rests on the
  serializer being unable to see the registry; a guard that fires converts that from a convention
  into a build failure. If a future need genuinely requires the serializer to resolve assets, the
  correct response is to supersede Q3 deliberately, not to widen the guard's allowlist.
- **Q1 is the decision most likely to be re-opened**, because "ids are identities, why can one
  repeat?" is a fair objection on first reading. The answer is the lifetime argument plus the
  no-consumer argument; both are recorded in Q1 and the behaviour is pinned by a test, so a future
  reader who disagrees must change a named test rather than discover the behaviour by accident.
- **The format is additive-only by rule** (Q4). That single rule is what makes "ignore unknown keys"
  safe, and it is the constraint any future v2 must respect before the version gate can be relaxed.
- **`M4_INFO` is deliberately reused rather than superseded.** M6 adds `M6_INFO` lines for load and
  save events but does not invent a parallel "M4_INFO rendering scene" line, so the log contract
  stays one shape that the close matrix can assert uniformly across all entry modes.
- **`src/serialization/` has existed and been empty since ADR-0008.** Delivering the milestone into
  the directory the project already reserved for it is the cheapest possible structural signal that
  the layering was the plan all along.

## Amendment — none yet

This ADR is Accepted as written. Any later milestone needing a different document format,
persistence boundary, or editor-ownership convention **supersedes this ADR** rather than editing it
(DOCUMENTATION-LIFECYCLE: ADRs are append-and-supersede, never overwrite). ADR-0010, ADR-0011,
`CONTEXT.md` and `DEFINITION.md` are amended in the same change as this ADR to match Q1, Q2, Q4, Q5,
Q9 and Q10.

---

## Amendment — 0046 close: the load happens PRE-GL

Appended, not edited, per DOCUMENTATION-LIFECYCLE. This ADR is **Accepted as written**: the
implementation matched Q0–Q11. This note exists because the tempting place for a silent change was
the **load's position relative to GL**, and the change landed in a *ticket* rather than here.

**What the tickets claimed, and why it was wrong.** 0045 recorded that the load *had* to happen
after context creation "because an entry Scene needs the `AssetRegistry`, whose GPU cache requires
a live context". Nothing in this ADR required that. `Scene` is pure data; the registry is only
needed **later**, to resolve `assetId`s into `DrawItem`s. The constraint conflated "the entry Scene
needs resolving" with "the entry Scene needs loading".

**The corrected rule, now asserted by tests:**

- **Load PRE-GL** — `main()` calls `loadSceneFrom()` before GLFW is initialised and hands the
  result to `buildEntryScene` as a separate argument, not through `Options` (0025 froze `Options`
  as pure syntax; a deserialized `Scene` is runtime state).
- **Resolve POST-GL** — resolution still needs the registry, so it stays where the context exists.

This is the only arrangement that satisfies this ADR's Testing Decisions ("no display required for
any row") while keeping Q5's three-way exclusion a cheap pre-GL gate. A structural failure is a
*user error*, not a rendering error, so it must not cost a window or a context.

**Why `preloadedScene` is an `optional`, and required, with no fallback.** A valid
`{"entities": []}` document is a real preloaded Scene, so the parameter must distinguish "absent"
from "empty". It is **not** defaulted, and the scene branch **fails loudly** if it is not engaged,
because an earlier revision of that function kept a re-read fallback: it ignored its own parameter,
re-read the file post-GL, and **every matrix row still passed** — re-reading a *valid* document is
indistinguishable from having preloaded it. The fallback was the bug's hiding place, not a safety
net.

**Also noted at close:** the 0046 ticket's "CTest count rises from 22" invariant was already stale
when M6 started (0045 had taken it to 27); the count is now 34. Ticket-level numbers are recorded
where they are observed, not asserted from this ADR.
