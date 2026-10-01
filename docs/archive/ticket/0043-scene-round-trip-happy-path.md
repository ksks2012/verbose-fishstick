# 0043 — serialize `Scene` ↔ v1 document (happy-path round-trip)

**Source:** ADR-0013 Q2 (document shape), Q4 (version gate + unknown keys), Q10 (empty document),
Q1 (reuse pinned through the real save/load path) — tracer bullet 3. PRD user stories 1, 2, 5, 12.
**Status:** complete (2026-09-30) — evidence recorded below
**Blocked by:** 0041 (target + guards), 0042 (`Scene` restore primitive)
**Related:** ADR-0013 Q2/Q4/Q10, ADR-0007 (`manifest.json` stays the single source of truth),
`src/asset/asset_cpu.cpp:53-56,62-114` (the JSON parse + unknown-key-ignore precedent)

## What it delivers

The **pure** projection both ways for a *valid* v1 document: `Scene` → JSON text, and JSON text →
`Scene`. No file I/O, no CLI, no failure-policy breadth yet — this ticket makes the round trip
correct and float-exact; 0044 hardens the read side against malformed input.

Keeping the core string-based is deliberate: it is what lets the round-trip test run with no files,
no display, and no GL (PRD "Testing Decisions"; user story 9). File I/O — including atomicity — is
0045.

## Scope

- **Document shape (Q2), frozen here.** A single self-contained JSON object:

  ```json
  {
    "version": 1,
    "entities": [
      { "id": 1, "name": "cube", "assetId": "noto_cube",
        "transform": { "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1] } }
    ]
  }
  ```

  `entities[]` is in **creation order**. `transform` vectors are `[x,y,z]`; `rotation` is **radians
  YXZ** — the Scene's own unit, never degrees (the degrees conversion is presentation-only, ADR-0011
  Q6). Only `id`, `name`, `assetId`, `transform` are written; the allocator, selection, camera, and
  viewport are **not** (Q2).
- **Write side:** `std::string serializeScene(const scene::Scene& scene)` in
  `src/serialization/scene_io.{hpp,cpp}` (namespace `noto3d::serialization`). Always emits
  `"version": 1`. Entities serialized in `Scene::entities()` order.
- **Read side (happy path):** `LoadResult deserializeScene(std::string_view text)`.
  `LoadResult` is **defined once, here, and not reinvented in 0044** — it is the serialization-local
  vocabulary (the `Result`/diagnostics convention of ADR-0008, local because the shared one lives in
  `noto3d_asset_cpu`, which the 0041 guard forbids reaching). The name and shape are frozen in this
  ticket so 0043 and 0044 cannot each invent half a contract:

  ```cpp
  // src/serialization/scene_io.hpp — namespace noto3d::serialization
  struct LoadResult {
      std::optional<scene::Scene> scene;              // engaged iff the load succeeded
      std::vector<std::string>     diagnostics;       // empty on a clean success
  };
  ```

  Invariants of the type itself: `scene.has_value() == diagnostics has no structural-failure entry`;
  a **structural** failure always yields `scene == std::nullopt` **and** at least one diagnostic
  (0044 adds the other failure paths, but this contract is fixed now); a success with a *reference*
  problem (unknown/empty `assetId`, see 0044) keeps `scene` engaged. `deserializeScene` never throws
  and never returns partially-populated state. The `save` side returns plain
  `std::string`/`void`-style output (no result type) unless a write failure needs reporting — that
  is 0045's I/O layer, not this one.
- A valid document is turned into a `Scene` through the ticket-0042 restore primitive, so the
  derived allocator (`max(restored ids) + 1`, empty → `1`) comes for free and no counter is ever
  trusted from the file.
- **Version gate (Q4):** accept only integer `1`. Reading a document whose `version` is missing, a
  non-integer, or anything other than `1` is a **structural failure** (its diagnostics/`nullopt`
  behaviour is completed in 0044; here it must at least not silently succeed).
- **Unknown keys ignored (Q4):** an extra key at the envelope level *or* the entity level loads
  successfully and is discarded. Match the `asset_cpu.cpp` `contains()` style.
- **Empty document (Q10):** `{"version":1,"entities":[]}` is valid → empty `Scene`,
  `nextEntityId == 1`, rendering nothing. M6 adds no flag that *creates* an empty scene.

## Invariants

- **Assets are referenced by `assetId` only.** No geometry, path, or hash is embedded —
  `manifest.json`
  stays the single source of truth (ADR-0007). The serializer never touches the registry (enforced
  by
  the 0041 guard, not by convention).
- **Determinism.** The same `Scene` serializes to byte-identical text; entity order is creation
order.
- **Float-exactness.** `float → JSON → float` must round-trip exactly for every transform component
  (values live in `glm::vec3` as `float`; JSON numbers are read back as `double`, whose shortest
  round-trip form restores the same `float`). This is asserted, not assumed.
- The serializer holds no editor/session state; it is a pure function of the `Scene`.

## Explicitly NOT in scope

- Structural-failure cases beyond the version gate, and the unresolvable-reference contract (0044).
- File read/write, atomic save, exit codes (0045).
- Any migration framework — no older version exists (Q4).

## Verification

- **The round-trip is the central test, and it must be a real round-trip** (build → write → read →
  compare), not decode-only: a `Scene` with ≥2 entities, non-identity names/assetIds, and
  non-identity transforms; compare **every** field of every entity including `id` and
  float-exact transform values, and the entity order.
- **The Q1 consequence, pinned through the real save/load path (Q1):** build a Scene, **destroy the
  highest id**, serialize, deserialize, then `createEntity()` → the new id is `max(restored)+1`
  and **equals the destroyed value** (the accepted reuse). Without the destroy step the test
  would pass trivially, so the destroyed high-water id is essential. If this assertion ever changes
  it is a decision, not a drift.
- Empty document loads to an empty `Scene` with `nextEntityId == 1` (asserted by a following
  `createEntity() → 1`).
- An unknown envelope key and an unknown entity key each load successfully and are ignored.
- `version` missing / `"1"` (string) / `2` fail (no `Scene`; diagnostic); the full failure shape is
  exercised in 0044, but these must already fail here.
- Serializing the same `Scene` twice yields identical text (determinism).
- All new assertions shown to **fail** against a deliberately wrong implementation (red-green).
- ctest green dev + release; the new suite runs with **no display, no GL context, and no files**.

**Rolls under spec:** ADR-0013 Q1, Q2, Q4, Q10; PRD user stories 1, 2, 5, 12.

## Delivered (2026-09-30)

`serializeScene(const scene::Scene&)` and `deserializeScene(std::string_view)` in
`src/serialization/scene_io.{hpp,cpp}`, plus the frozen `LoadResult` vocabulary. New headless
suite `tests/serialization/scene_io_test.cpp` (`serialization-scene-round-trip`), **81
assertions**. ctest **23/23 dev + 23/23 release** (22 → 23; a new CTest entry), clean builds 0
warnings, both close matrices ALL PASS.

### Document shape, emitted exactly as Q2 froze it

A one-entity document serializes to (whitespace inserted here for width only — the real output
has none):

    {"version":1,"entities":[{"id":1,"name":"cube","assetId":"noto_cube",
    "transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]}

One entity per line, `entities[]` in creation order,
`transform` vectors `[x,y,z]`, rotation in **radians YXZ** (the `Transform` unit, never degrees).
The allocator, selection, camera and viewport are absent by construction: there is no code path
that could write them. `nlohmann::ordered_json` is used rather than the default `json` so key order
is insertion order, making the output match the frozen shape instead of `json`'s alphabetical
ordering (`entities` before `version`), which would be deterministic but unreadable.

### Decisions the implementation had to make

- **`scene.hpp` is included, not forward-declared.** `LoadResult` holds
  `std::optional<scene::Scene>`, and `std::optional` requires a complete type at instantiation, so
  a forward declaration does not compile. Including it costs a glm dependency and keeps the include
  closure inside `src/scene`, which the 0041 `POLICY closed` guard verifies.
- **Float exactness is nlohmann's default, and it is asserted with `==`, not a tolerance.** Floats
  are stored as JSON doubles and dumped with nlohmann's shortest round-trip form, so
  `float → double → text → double → float` is the identity. Proven over the full range including
  `1e-45f` (denormal), `±3.4028235e38f` (near `FLT_MAX`) and `1/3`.
  **`-0.0f` is asserted separately via `std::signbit`**: it is the one value `==` cannot pin, since
  `-0.0f == 0.0f` is true, so a serializer emitting `"0.0"` for a negative zero would have passed
  every other assertion in the suite.
- **Duplicate and `id == 0` rejection lives here, not in 0044, for a concrete reason.**
  `Scene::fromEntities` asserts uniqueness as a *debug-only precondition* (0042), so a document
  carrying duplicate ids **aborts the process** (verified: `SIGABRT`) rather than returning
  anything. That would break this ticket's own contract that `deserializeScene` "never throws and
  never returns partially-populated state" — an abort is worse than an exception. The serializer
  must therefore never hand `fromEntities` data it cannot accept. 0044 still owns the richer
  diagnostics (which entity, which id) and the unresolvable-reference contract.
- **`id` is read as `int64` before narrowing.** `EntityId` is `uint32_t`, so a negative id in a
  file would wrap to a huge unsigned value and pass a post-cast check. The two rejections are not
  equally motivated and the code says so: `id == 0` is `InvalidEntityId` and is abort-prevention;
  a negative or fractional id is a malformed value that would otherwise become a nonsense identity.
- **Structural validation breadth is 0044's; only the no-throw minimum is here.** Every type check
  in `readEntity`/`readVec3` is load-bearing for "never throws" — `get<std::string>()` on a
  non-string and `get<float>()` on a non-number both throw `nlohmann::type_error`, which would
  escape the function. They are a minimum to uphold this ticket's contract, not a partial
  implementation of Q3; the messages are deliberately generic ("malformed entity entry") because
  0044 owns the diagnostic taxonomy.

### Red-green: 20 mutations, 0 vacuous

Every mutation patches one decision, rebuilds, and re-runs the suite; all 20 are caught. Five are
caught by **abort** (the suite exits non-zero with no `FAIL` line) because the mutation lets an
exception or a precondition assert escape — a stronger signal than an assertion failure, and
recorded as `caught(abort)`:

| # | Mutation | Caught by |
|---|---|---|
| A | `version` hardcoded to 2 in output | 6 assertions |
| B | version gate accepts anything | 2 |
| C | version gate ignores non-integer (accepts `"1"`) | abort |
| D | entities emitted in reverse order | 6 |
| E | position/rotation swapped | 14 |
| F | `transform` omitted from output | 6 |
| G | `assetId` dropped from output | 6 |
| H | float precision truncated to 3 decimals | 5 |
| I | allocator trusted from file instead of derived | 16 |
| J | `id == 0` accepted | abort |
| K | empty document treated as structural failure | 10 |
| L | structural failure returns no diagnostic | 13 |
| M | vector length 2 accepted | 3 |
| N | non-numeric transform component silently 0 | abort |
| O | malformed JSON escapes the catch | abort |
| P | duplicate-id check removed | abort |
| Q | transform lookup ignores the nested object | 8 |
| S | missing-version message conflated with malformed | 2 |
| T | id check narrowed to `id == 0` only | 3 |
| R | empty document rejected | 3 |

Mutations C, J, N, O and P are the ones that prove the *no-throw* contract is load-bearing rather
than decorative: each one aborts the suite, so the guards they remove are the only thing standing
between a malformed document and a crash.

### Defects found and fixed during this ticket

- **`readTransform` was handed the entity object instead of the nested `transform` object.** The
  first red run failed 7 assertions with a confusing "malformed entity entry" on documents the
  serializer had itself just written. `readTransform` looks for `position`/`rotation`/`scale` as
  direct children of its argument, and it was being passed the entity, where they do not exist.
- **`!parent[key].size() == 3` was a no-op.** The `!` binds to the `bool` conversion, so the guard
  read `(!size) == 3`, which is false for every array — a length-2 position would have been
  accepted and read `a[2]` out of bounds. Caught by mutation M once it existed; written as
  `parent[key].size() != 3`.
- **A dead `const char* t` fragment in the test** (declared to dedupe the transform literal, never
  used, silenced with `(void)t`) — found in review, deleted.

### Review findings (both axes, run against 55379e6)

- **Standards — 1 hard:** the new CMake comment claimed the suite links "noto3d_serialization
  ONLY … keeping the suite off `noto3d_core`", while the link line right below it pulls
  `noto3d_core` in for the log facade. The comment documented a design the code did not implement.
  Corrected to state the real trade (the same one `noto3d-registry-tests` makes) and to explain
  that the arch guard probes the include *closure* precisely because `noto3d_core` contributes
  glad's PUBLIC include dir.
- **Standards — 1 judgement call:** the `fromEntities`-asserts rationale is restated at four
  sites (header, implementation, two tests). Left as-is: the repo's convention is dense per-site
  rationale, and the restatements are not word-for-word.
- **Spec — missing:** no recorded red-green evidence, though both predecessors append a Delivered
  section. Fixed by the table above.
- **Spec — wrong:** Q4 requires a diagnostic that "distinguishes an unsupported version from a
  malformed one". The first implementation emitted one shared message for both a missing
  `version` and a `"1"` string, so the two were indistinguishable. Now three distinct messages
  (missing / malformed / unsupported), pinned by `testVersionDiagnosticsAreDistinct` and by
  mutation S.
- **Spec — mis-justified:** the `id <= 0` guard was documented as abort-prevention, but only
  `id == 0` is (a negative id wraps through `uint32_t`). Comment corrected; mutation T pins the
  narrower check.
- **Spec — scope:** the reviewer flagged the entity-field type checks as 0044 creep. Judged
  **in scope as a minimum**, for the reason recorded above: every one of them prevents a `get<T>()`
  that would throw, and this ticket's line 58 requires that it never throws. Removing them would
  break the ticket's own contract; 0044 extends the diagnostics rather than creating the validator,
  which is the 0042 "one validator, not two half-validators" rule honoured in the other direction.

### Verification beyond the ticket's list

- **Headless claim verified, not assumed:** `ldd` shows no GL/GLFW/X11 library on the suite's link
  line, it passes with `DISPLAY` unset, and `strace` shows no `.scene`/`.json` file opened.
- **`-0.0f` sign preservation** checked out-of-band as well as in-suite.
- Release build re-runs the whole suite green: the structural checks are ordinary code, not
  `assert`s, so they behave identically under `NDEBUG` (unlike 0042's precondition, which is
  deliberately absent there).
