# 0044 — load policy: strict on structure, lenient on references

**Source:** ADR-0013 Q3 (load policy), Q10 (empty) — tracer bullet 4. PRD user stories 3, 4.
**Status:** ready-for-agent
**Blocked by:** 0043 (extends the deserialize path it introduces)
**Related:** ADR-0013 Q3, PRD Testing Decisions ("each structural-failure case gets its own case"),
`src/asset/asset_cpu.cpp` (diagnostic style), the M4 "unresolved → skip" contract
(`collectDrawItems`)

## What it delivers

The complete read-side policy: **every structural defect hard-fails the load** (no `Scene` returned,
a diagnostic explaining what was wrong), while an **unresolvable `assetId` is not a defect at all**
—
it is kept verbatim and the existing M4 resolver skips it at render. This is the split that makes a
shared scene usable on a machine that lacks an asset without weakening the promise that a corrupt
file is refused rather than silently misread.

## Scope

- **Structural failures (hard fail → no `Scene`, diagnostic each):**
  - malformed JSON (unparseable text);
  - `version` missing, not an integer, or `!= 1` (Q4);
  - `entities` missing, or not an array;
  - an element of `entities` that is not an object;
  - an entity missing a required field (`id`, `name`, `assetId`, `transform`);
  - a field of the wrong type (e.g. `id` not an unsigned integer, `name`/`assetId` not a string);
  - `id == 0` (reserved `InvalidEntityId`);
  - a duplicate `id` within the document;
  - a malformed `transform` (missing `position`/`rotation`/`scale`, wrong element count, or a
    non-numeric component).
- **Lenient references (load succeeds; entity kept):**
  - an `assetId` naming an asset absent from the local manifest (or whose geometry failed to
  resolve)
    is **not** a structural error. The serializer cannot and must not consult the registry (the 0041
    guard guarantees it), so it simply retains the entity. The M4 resolution seam
    (`collectDrawItems`
    → resolver, unresolved → skip) is what drops it at draw time and is where the warning is logged
    —
    unchanged M4 behaviour, not new serializer behaviour.
  - an **empty-string** `assetId` is likewise a lenient reference (present and a string is the only
    structural rule); it is retained and skipped at resolution.
- **Failure shape:** a load that fails returns **no `Scene`** through the ticket-0043 `LoadResult`
  (`std::optional` has no value) plus at least one diagnostic. A load that succeeds with a
  sourceless
  reference returns a `Scene` containing that entity.
- **Message quality:** diagnostics name the defect (which field, which entity index/id, what was
  expected) so a hand-editor can fix the document (user story 5). No raw parser stack text leaked as
  the whole message.

## Invariants

- **No half-loaded Scene.** A structural failure never returns a partially-populated `Scene`; the
  caller gets `nullopt`. Loading is all-or-nothing on structure.
- **An empty-string `assetId` is legal input, not a structural defect.** `"assetId": ""` passes
  the structural gate (present, a string), the entity is **retained** in the `Scene`, and it is
  **skipped at render** by the existing M4 unresolved-reference seam. Only a *missing* or
  *non-string* `assetId` is structural. This mirrors the "no manifest coupling" rule below: the
  serializer never judges whether an id can resolve, so it certainly never judges whether it is
  empty.
- **The serializer never resolves an `assetId`.** It does not import the registry, does not open the
  manifest, and does not mark an unknown id as invalid. (If a future need genuinely requires
  resolution here, the correct response is to supersede Q3 deliberately, not to widen the guard —
  ADR-0013 Further Notes.)
- **Empty document stays valid** (`{"version":1,"entities":[]}` → an empty `Scene` with
  `nextEntityId == 1`).
- Unknown keys at any level remain ignored (Q4) — they are not structural failures.

## Explicitly NOT in scope

- Unreadable-file / missing-file handling and exit codes — that is I/O, i.e. 0045.
- App-level logging of an unresolvable reference (already M4; re-verified by the 0046 matrix).
- Any repair, coercion, or "best effort" load. A defect is refused.

## Verification

- **Each structural case gets its own case**, asserting a diagnostic **and** that no `Scene` comes
  back:
  malformed JSON · missing `version` · `version: "1"` · `version: 2` · missing `entities` ·
  `entities` not an array · entity not an object · missing each required field · wrong field type
  (`id` string, `name` number, `assetId` array) · `id: 0` · duplicate `id` · malformed `transform`
  (missing key, 2-element vector, string component).
- **Lenient case:** a document naming an `assetId` not in the manifest loads successfully and the
  entity is present in the returned `Scene` with that `assetId` intact. (The "skipped at resolution"
  half is asserted at the app/CLI level in 0045/0046 over the existing M4 seam.)
- Empty document → empty `Scene`, `createEntity() → 1`.
- Unknown envelope/entity keys still load successfully (guards against over-strict validation
  replacing the Q4 rule).
- Every failure assertion shown to **fail** against a deliberately lax implementation (red-green),
  so
  none is vacuous and no case is silently accept-everything.
- ctest green dev + release; still no display, no GL, no files.

**Rolls under spec:** ADR-0013 Q3, Q4, Q10; PRD user stories 3, 4.
