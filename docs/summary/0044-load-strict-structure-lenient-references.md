# Execution Summary: 0044 — Load strict structure, lenient references

- **Ticket**: `docs/ticket/0044-load-strict-structure-lenient-references.md`
- **Date**: 2026-09-30
- **Branch**: `main`
- **HEAD**: `a07a691` (feat(serialization): add the v1 scene round-trip and freeze the LoadResult contract)
- **Commits**: Uncommitted (staged+modified: `src/serialization/scene_io.cpp`, `tests/serialization/scene_io_test.cpp`)

## What was done

### Code
- **Diagnostic taxonomy hardened (Q3).** `readVec3()` now reports *which* vector component and *why* (missing / wrong array / wrong count / non-numeric). `readTransform()` explicitly diagnoses `transform` not being an object. `readEntity()` emits field-scoped messages with the entity index (`entities[N]`) and states the expected shape (`must be an object`, `must be an integer`, `must be a string`, `exactly 3 components`). The message prefix `scene_io: ` is enforced everywhere.
- **Guard isolation.** Structural checks are ordered to produce the correct diagnostic when multiple failures are possible (e.g., `root` vs `missing 'version'`, `missing 'entities'` vs wrong type). The entity loop propagates the failing entity's reason verbatim and never collapses diagnostics to a generic string.
- **Robust id validation.** After reading `id` as `int64_t`, we reject `id < 0`, `id == 0` (reserved `InvalidEntityId`), and `id > UINT32_MAX` (added from spec review) with explicit messages (including the offending value / upper bound). This prevents both debug aborts inside `Scene::fromEntities()` and silent narrowing/wrapping in release.
- **Lenient references.** Serializer never consults the registry; unknown/unresolvable `assetId` values are **retained verbatim** (including `""`) and do not produce diagnostics. Empty `name` remains legal. `entities: []` loads an empty `Scene` with `nextEntityId == 1`.

### Tests (consolidated, one shared table)
- Replaced 0043's four ad-hoc structural loops with a single `StructuralCase` table (`tests/serialization/scene_io_test.cpp`) covering malformed JSON, non-object root, version cases (missing/non-int/unsupported), missing/wrong-type `entities`, entity shape, all id cases (fractional/negative/zero/overflow), duplicate ids (adjacent/non-adjacent), string fields, transform shape, and the index prefix for nested transform failures (added per review). 
- Added explicit assertions for guard isolation (a guard's wording must appear and the neighbour's must not), for "expectation stated" (`must be a ...` / arity / the specific value), for the entity index appearing on nested failures, and for `no partial scene` on trailing defects.
- `entities: []`, unknown keys ignored, determinism, and float-exactness (`-0.0f` signbit) remain green. All tests use `check(cond, name)` and the suite reports `ALL PASS` with the full assertion set.

### Refactor (quality, non-behavioural)
- Extracted tiny helpers (`requireField`, `requireString`, `requireInteger`) to DRY the 11 guard sites while preserving every diagnostic string byte-for-byte (diagnostics are a user-facing contract). Added rationale comments citing ADR-0013 Q3/Q4.

## Verification

- **Unit suite**: `ctest --test-dir build/dev` → **23/23 passed**; `ctest --test-dir build/release` → **23/23 passed**.
- **M5 close matrix (tier A)**: `scripts/m5-close-matrix.sh build/dev` → **ALL PASS** (26 PASS/0 FAIL); `scripts/m5-close-matrix.sh build/release --release` → **ALL PASS** (35 PASS/0 FAIL).
- **Mutation testing (0044 harness)**: `/tmp/opencode/mutate44.py` → **27/27 caught, 0 vacuous** (single-site helpers prevent redundant mutations).
- **Build cleanliness**: `dev` and `release` clean (no warnings/errors).
- **Regression**: 0043 round-trip still green; serialization suite assertions: **206** (clean, no tautological `check(true)`).

## Files changed (uncommitted delta)

- `src/serialization/scene_io.cpp` — +141/-124 lines (diagnostics + id upper bound + helpers + rationale)
- `tests/serialization/scene_io_test.cpp` — +383/-124 lines (consolidated structural table, isolation/expectation/index assertions, overflow + transform-index cases)

## Close-out notes

- Satisfies ticket 0044 acceptance: structural errors are rejected with actionable diagnostics (including entity index), references are lenient (unknown/empty `assetId` retained), no partial loads, and version diagnostics remain distinct. The two review findings were addressed (upper bound on `id`, and asserting the entity index for transform defects). No scope creep.
