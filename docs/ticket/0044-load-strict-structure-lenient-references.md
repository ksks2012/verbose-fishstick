# 0044 — load policy: strict on structure, lenient on references

**Source:** ADR-0013 Q3 (load policy), Q10 (empty) — tracer bullet 4. PRD user stories 3, 4.
**Status:** complete
**Blocked by:** 0043 (extends the deserialize path it introduces) — **satisfied**
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

## Delivered

- **Code** (`src/serialization/scene_io.cpp`, +141/-124):
  - **Diagnostic taxonomy (Q3).** `readVec3()` names the failing vector key, the component index
    and the cause (missing key / not an array / not exactly 3 / non-numeric component);
    `readTransform()` diagnoses `transform` not being an object; `readEntity()` prefixes every
    message with `entities[N]` and states the expected shape (`must be an object`,
    `must be an integer`, `must be a string`, `exactly 3 components`). All messages carry the
    `scene_io: ` prefix.
  - **Guard helpers.** `requireField()` / `requireString()` / `requireInteger()` collapse the 11
    duplicated guard sites without changing a single diagnostic string — a diagnostic is a
    user-facing contract, not something to "improve" while refactoring.
  - **Robust `id` validation.** Read as `int64_t`, then rejected for `< 0`, `== 0`
    (reserved `InvalidEntityId`) and `> UINT32_MAX`, each with the offending value in the
    message. The upper bound is **load-bearing**: `2^32` narrows to `0` on the unchecked
    `EntityId` cast, which loads silently with the one forbidden id under NDEBUG and
    **SIGABRTs** inside `Scene::fromEntities()` in debug. Verified in both presets.
  - **Lenient references.** The serializer never consults the registry; unknown/unresolvable
    `assetId` values — including `""` — are retained verbatim and produce no diagnostic.
    Empty `name` stays legal. `entities: []` yields an empty `Scene` with `nextEntityId == 1`.
- **Tests** (`tests/serialization/scene_io_test.cpp`, +383/-124): 0043's four ad-hoc structural
  loops were **migrated into one `StructuralCase` table** — deliberately, since this repo rejects
  duplicated policy because two copies drift (ADR-0012 ③, ADR-0013 Q11), and a defect listed in
  two tables is one the other half of the suite forgets to check. Each row asserts: no `Scene`,
  a diagnostic exists, the defect is **named**, the **expectation is stated**, the **entity
  index** appears (including for nested `transform` defects), a redundant neighbour guard's
  wording is **absent** (guard isolation), and the message is not raw parser text. The three
  `version` mistakes are asserted mutually distinct (Q4).
- **Spec-review findings addressed:** (1) the missing `id` upper bound above; (2) the entity
  index is now asserted for a nested `transform` defect, so that clause of the ticket is
  enforced rather than merely implemented; (3) a fractional-`id` row was restored — the
  quoted-`"1"` row pinned string-vs-integer but left integer-vs-float unpinned, which made
  `is_number_integer()` → `is_number()` a vacuous mutant.

### Evidence

| Gate | Result |
| --- | --- |
| ctest `build/dev` | **23/23 passed** |
| ctest `build/release` | **23/23 passed** |
| `scripts/m5-close-matrix.sh build/dev` | **ALL PASS** (26 PASS / 0 FAIL) |
| `scripts/m5-close-matrix.sh build/release --release` | **ALL PASS** (35 PASS / 0 FAIL) |
| Mutation suite (`/tmp/opencode/mutate44.py`) | **27/27 caught, 0 vacuous** |
| Clean dev + release build | no errors, no warnings |
| Headless / file-free | no display, no GL, no files touched |

**On the mutant count.** It moved 31 → 27 across the review, and every remaining mutant is
*stronger*: the guard refactor deleted genuinely duplicated sites (four missing-field rows became
one `requireField` mutation, three type rows became one `requireString` mutation, and the
separate empty-`name`/empty-`assetId` mutants became one), so a maintainer now edits one site
instead of four. Dropping a mutant that duplicates another mutant's site is not lost coverage.

**Scope.** No scope creep: only `scene_io.cpp` and its test changed. The summary is in
`docs/summary/0044-load-strict-structure-lenient-references.md`.

**Commits.** Uncommitted at the user's request.
