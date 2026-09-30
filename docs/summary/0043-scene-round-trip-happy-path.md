# Execution Summary: 0043-scene-round-trip-happy-path

- **Date**: 2026-09-30
- **Spec/ADR Path**: `docs/adr/0013-scene-save-load-persistence.md` (Q1/Q2/Q4/Q10; Q11 vocabulary)
- **Ticket Path**: `docs/ticket/0043-scene-round-trip-happy-path.md`
- **Branch**: `main` (uncommitted — the user asked for no commit)

---

## 📌 Overview

- **Commits**: 0 — work left staged but uncommitted, as requested
- **Files Changed**: 6 files (`+723` / `-14`) vs fixed point `55379e6`
- **Ticket status**: `ready-for-agent` → `complete (2026-09-30)`, evidence recorded
- **Unblocks**: **0044** (strict structure / lenient references)
- **Test count**: ctest **22 → 23** (one new headless suite), **81 assertions** in it

Tracer bullet 3 of M6, and the first code to touch untrusted input anywhere in the project. The
runtime surface is small; the substance is the `LoadResult` contract, which the ticket freezes here
precisely so 0043 and 0044 cannot each invent half of one.

## 📝 Key Changes

1. **`serializeScene` / `deserializeScene` + the frozen `LoadResult` vocabulary** in
   `src/serialization/scene_io.{hpp,cpp}`. `LoadResult` is deliberately
   *serialization-local* — the shared `Result`/diagnostics type lives in `noto3d_asset_cpu`, which
   the 0041 guard forbids the serializer from reaching (ADR-0013 Q11). Entities are written in
   `Scene::entities()` order and rebuilt through 0042's `fromEntities`, so the derived allocator
   arrives for free and no counter is ever trusted from the file.

2. **The document shape is Q2 verbatim, and its absence is structural.** The allocator, selection,
   camera and viewport are not merely omitted — no code path could write them. `nlohmann::ordered_json`
   replaces the default `json` so key order is insertion order: with the default, `entities` would
   sort before `version`, which is deterministic but does not match the shape the ticket froze.

3. **Duplicate and `id == 0` rejection lives in 0043, not 0044** — the one place I deliberately
   read past the ticket's stated scope boundary. `fromEntities` asserts uniqueness as a *debug-only*
   precondition (0042), so a duplicate-id document **aborts the process**; verified `SIGABRT`, exit
   134. That breaks this ticket's own line-58 contract ("never throws and never returns
   partially-populated state") harder than an exception would. `id` is read as `int64` **before**
   narrowing, because `EntityId` is `uint32_t` and a negative id would otherwise wrap to a huge
   unsigned value and sail through.

4. **Float exactness asserted with `==`, plus `signbit` for `-0.0f`.** Exactness is nlohmann's
   shortest-round-trip dump, checked across `1e-45f`, `±FLT_MAX` and `1/3`. `-0.0f` gets its own
   assertion because it is the single value `==` cannot pin — `-0.0f == 0.0f` is true, so a
   serializer emitting `"0.0"` for a negative zero would have passed every other assertion in the
   suite.

## 🧪 Verification

- **Typecheck**: 🟢 Pass — no standalone typechecker; the compiler is the check. Clean dev **and**
  release builds from scratch, **0 warnings / 0 errors** under `-Wall -Wextra -Wpedantic`.
- **Tests**: 🟢 Pass — ctest **23/23 dev + 23/23 release**, both close matrices **ALL PASS**.
- **Red-green: 20 mutations, 0 vacuous.** Five are caught by **abort** (suite exits non-zero with no
  `FAIL` line) because the mutation lets an exception or a precondition assert escape — a stronger
  signal than an assertion failure, and the direct proof that the no-throw contract is load-bearing
  rather than decorative: those five guards are all that stand between a malformed document and a
  crash. The largest single catch is 16 assertions (allocator trusted from the file instead of
  derived), the most surgical is 2 (conflating two version diagnostics).

| Group | Mutations | Caught by |
|---|---|---|
| version gate | A, B, C, S | 6, 2, abort, 2 |
| output shape | D, E, F, G, Q | 6, 14, 6, 6, 8 |
| float exactness | H | 5 |
| allocator | I | 16 |
| id validity | J, P, T | abort, abort, 3 |
| empty document | K, R | 10, 3 |
| diagnostics | L | 13 |
| field typing | M, N | 3, abort |
| no-throw | O | abort |

- **Headless claim verified rather than assumed**: `ldd` shows no GL/GLFW/X11 on the suite's link
  line; it passes with `DISPLAY` unset; `strace` shows no `.scene`/`.json` file opened.
- **Release parity**: the structural checks are ordinary code, not `assert`s, so they behave
  identically under `NDEBUG` — unlike 0042's precondition, which is deliberately absent there.

## 🔍 Code Review Findings

Run as two parallel axes against `55379e6`. **Both axes found real defects; all were fixed and
re-verified** (mutations S and T were added afterwards to pin the fixes).

- **Standards** — 🟠 1 hard, 1 judgement call
  - **The CMake comment described a design the code did not implement** (hard). It claimed the suite
    links "`noto3d_serialization` ONLY … keeping the suite off `noto3d_core`", while the link line
    directly below pulls `noto3d_core` in for the log facade. Corrected to state the real trade —
    the same one `noto3d-registry-tests` makes — and to explain that the arch guard probes the
    include *closure* precisely *because* `noto3d_core` contributes glad's PUBLIC include dir.
  - A dead `const char* t` fragment in the test (declared to dedupe the transform literal, never
    used, silenced with `(void)t`) — **deleted**.
  - *Not actioned:* the `fromEntities`-asserts rationale is restated at four sites. Left as-is; the
    repo's convention is dense per-site rationale and these are not word-for-word.
- **Spec** — 🟠 1 missing, 2 wrong, 1 scope
  - **Q4's diagnostic requirement was only half met** (wrong). Q4: a diagnostic that
    "distinguishes an unsupported version from a malformed one". The first implementation emitted
    *one* shared message for both a missing `version` and a `"1"` string, so a user who typo'd the
    key could not tell that apart from one running a v2 file. Now three distinct messages
    (missing / malformed / unsupported), pinned by a new test and by mutation S.
  - **The `id <= 0` guard was documented as abort-prevention, which is wrong** (wrong). `EntityId`
    is `uint32_t`, so a negative id wraps and *passes* `fromEntities` silently; only `id == 0` is
    abort-prevention. Comment corrected, and mutation T pins the narrower check.
  - **No recorded red-green evidence**, though both predecessors append a `## Delivered` section
    (missing). Now recorded, with the full mutation table.
  - **Scope: the entity-field type checks were flagged as 0044 creep** (scope). Judged **in scope as
    a minimum**, because every one of them prevents a `get<T>()` that would throw —
    `get<std::string>()` on a non-string and `get<float>()` on a non-number both raise
    `nlohmann::type_error`, which would escape the function and break the ticket's own contract.
    The messages stay deliberately generic because 0044 owns the taxonomy; 0044 therefore
    *extends* this validator rather than creating a second one, which honours 0042's
    "one validator, not two half-validators" rule in the direction that matters.

## 🐛 Defects the red run itself caught

Both were found by the suite failing against a correct-looking implementation, which is the whole
point of running red first:

- **`readTransform` was handed the entity object, not the nested `transform` object.** Seven
  assertions failed with a misleading "malformed entity entry" — on documents the serializer had
  itself just written. `readTransform` looks for `position`/`rotation`/`scale` as direct children
  of its argument, and the entity object does not have them. (Mutation Q now pins this.)
- **`!parent[key].size() == 3` was a silent no-op guard.** The `!` binds to the `bool` conversion,
  so the expression read `(!size) == 3` — false for every array. A length-2 position would have been
  accepted and read `a[2]` out of bounds. Rewritten as `size() != 3`, and mutation M pins it.

## 📦 Recent Commits

- *(uncommitted)* feat(serialization): v1 scene round-trip — `serializeScene`/`deserializeScene` + `LoadResult`
- `55379e6` docs(0042): add the execution summary for the Scene restore primitive
- `a5bbfee` feat(scene): add the Scene restore primitive and Editor::replaceScene
- `da17fea` feat(serialization): create `noto3d_serialization` target with architecture guard and versioning

## ➡️ Next

0044 is unblocked. Note for it: it **extends** the validator 0043 had to build rather than creating
one, so its first job is replacing the generic `"malformed entity entry"` message with the Q3
diagnostic taxonomy, plus the unresolvable-`assetId` contract (kept as data, warned, skipped at
render — the M4 contract). One open judgement call for 0044: this ticket's field checks are
minimum-viable, and the sharper question of *how much* structure to validate, versus deferring, was
resolved here as "enough to guarantee no-throw".
