# 0040 — ADR-0012 close-out residue: the two ①/③ invariants nothing enforces

**Source:** found by ticket 0039's close-out audit of ADR-0012 (2026-09-29).
**Status:** ready-for-agent
**Blocked by:** — (0039 complete; nothing blocks this)
**Related:** ADR-0012 ① and ③, `docs/archive/ticket/0038-asset-cpu-no-gl-link.md` (finding 2 was first
raised there and deferred), `docs/archive/ticket/0039-asset-cpu-split-closeout.md`

## What it delivers

Two ADR-0012 invariants that the ADR states as contracts but that **nothing in the repository
mechanically checks** — and, in the first case, a test that *looks* like it checks one of them but
provably cannot fail. The split itself is done and verified; these are the residue of
"verified by reading", not of unfinished implementation. Cheap, and both belong in the CPU test
target that 0038 made GL-free.

## Scope

1. **ADR-0012 ①: assert `CanonicalShape::material.texture` is always `std::nullopt`.**
   The ADR is explicit that this is "a stronger, **checkable** contract than 'usually null'" — the
   only writer of that field is `uploadToGpu()` at upload time. Today it is true in practice, and
   **one test appears to check it but cannot fail.** `tests/asset/registry_test.cpp:400` in
   `testReferenceImageNeverTexture()` reads:

   ```cpp
   check(reg.model(e.id) == nullptr || true,  // model is null in ManifestOnly
         (e.id + ": ManifestOnly model is null (reference_image not uploaded)").c_str());
   ```

   The `|| true` makes the condition unconditionally true, so the assertion is vacuous — and its
   surrounding comment (lines 385-392) claims exactly the guarantee ADR-0012 ① makes: *"After
   loadManifestOnly(), all textures are null AND diffuseTextureRef is unset."* Nothing verifies
   that, and the test reads as though it did. `grep -rn 'nullopt' tests/ src/asset/` finds no
   other assertion covering the invariant; `obj_loader.cpp` only ever records `diffuseTextureRef`.

   Fix both halves: drop the `|| true` so the check can fail, and assert the invariant the comment
   actually claims — `material.texture == std::nullopt` — over a fixture whose MTL **does** declare
   `map_Kd`. Without that fixture the assertion is still vacuous, because a CPU parse that wrongly
   populated the field would go unnoticed on OBJs that have no texture to begin with. Show the
   restored check failing against a deliberately broken parse (red-green), so it is provably not
   another `|| true`.

2. **ADR-0012 ③: test the observable consequences of the GPU-dedup ordering, not just the
   precedence.** 0038 pinned the *ordering rule* (the dedup probe must run **after**
   `fallback_geometry` resolution — see 0038's review section, which caught a real regression
   against it). What remains untested is what the ordering *buys*:
   - an entry hitting the dedup fast path is counted by `uniqueGeometryCount()` without a second
     parse — currently only asserted indirectly, via ManifestOnly-mode deduplication;
   - `assetStatus(id) == Success` with no per-asset diagnostics on a fast-path hit;
   - `Origin::AlreadyUploaded` means the entry's `cacheKey` is the *canonical* key even when
     `fallback_geometry` was resolved, which is the one case where the effective key differs from
     the manifest's `geometry` field.
   All of these need `loadManifestCpu()` with an injected probe, exactly as
   `testGpuDedupOrderingKeepsFallbackWarning` already does.

## Why these were not done in 0039

0039 is scoped "**(ideally) no new code**" — it records and re-runs, it does not add. Both items are
new assertions. Filing them here keeps 0039's "zero observable change" claim honest instead of
quietly widening it.

## Explicitly NOT in scope

- The `renderer::Model` alias duplication (0038's deferred finding) stays deferred. It is
  compile-safe: `asset_registry.cpp` includes `model.hpp`, so a divergent alias is a
  conflicting-declaration error in that TU, not silent drift. Fixing it means adding a
  `src/renderer/model_fwd.hpp`, which is an ADR-0012 ① classification decision and would require
  amending the include-closure guard's `src/renderer/` allowlist. Worth doing on its own ticket if
  ever wanted; it is not a defect.

## Verification

- ctest green dev + release. Folding the restored `testReferenceImageNeverTexture` assertions into
  the existing entry keeps the count at **22/22**; adding a new CTest entry makes it 23. Either is
  acceptable — what is not acceptable is dropping or weakening a pre-existing assertion, and in
  particular not leaving a vacuous one behind.
- `scripts/m5-close-matrix.sh` ALL PASS in both build dirs.
- The new assertion must be shown to **fail** if the invariant is deliberately broken (red-green),
  so it is not a test that cannot fail.
