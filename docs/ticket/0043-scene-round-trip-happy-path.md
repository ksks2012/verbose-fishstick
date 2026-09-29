# 0043 — serialize `Scene` ↔ v1 document (happy-path round-trip)

**Source:** ADR-0013 Q2 (document shape), Q4 (version gate + unknown keys), Q10 (empty document),
Q1 (reuse pinned through the real save/load path) — tracer bullet 3. PRD user stories 1, 2, 5, 12.
**Status:** ready-for-agent
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
