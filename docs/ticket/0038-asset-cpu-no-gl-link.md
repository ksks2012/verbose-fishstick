# 0038 — point `tests/asset` and the `--list-assets` CLI at `noto3d_asset_cpu` (stop linking GL)

**Source:** ADR-0012 ③ (consequences) — tracer bullet 3.
**Status:** complete (2026-09-29) — verified; one invariant amended, see below
**Blocked by:** 0037 (`noto3d_asset_cpu` must exist)

## What it delivers

The concrete win the 0027 spike was opened to price: **the CPU-only asset test target and the
`--list-assets` diagnostic path no longer link GL at all.**

Today `tests/asset/registry_test.cpp` PUBLIC-links `noto3d_asset` → `noto3d_renderer` → `glad`,
even though **every** case in it exercises a CPU-only path (`loadManifestOnly()` /
`loadModelShapesOnly()`, asserting `model()==nullptr` / `externalModel()==nullptr`). 0025⑧ recorded
that reach as "expected today (0027)". This ticket ends it.

- `noto3d-registry-tests` links `noto3d_asset_cpu` (+ its test-only deps), not `noto3d_asset`.
- The `--list-assets` CLI path (0017, ADR-0009) runs against the CPU core so the pre-GL
  diagnostic is GL-free by construction, not merely "before a GL context exists" at runtime.

Both must be **behaviourally identical** — the tests must pass unchanged (same assertions, same
suite count, same CTest names), and `--list-assets` must still print the same manifest listing and
exit 0 without ever touching GL.

## Scope

- `CMakeLists.txt`: retarget `noto3d-registry-tests` to `noto3d_asset_cpu`. Its test sources stay
  where they are unless a case genuinely needs the facade — if any case needs a GPU-only entry
  point, split that case into the GL-linked test target rather than re-linking the whole suite to
  GL (a CPU suite that links GL is what this ticket removes).
- The `--list-assets` command (`src/app/editor_entry.cpp::runListAssetsCommand`, `editor_entry.cpp:28`)
  resolves its manifest listing through the CPU core. It is already a pre-GL path; this makes the
  *link* match the runtime guarantee. The `M3_ASSETS version=1` / `end count=6` output is unchanged.
- The `noto3d-editor` executable keeps linking the full GPU facade — the editor needs it. Only the
  CPU-only test/CLI paths shed the GL edge.

## Invariants

- **The registry test suite's assertions and CTest names are unchanged.** This is a link-graph
  change, not a test change. **Amended (0038): the suite count is 22/22, not 21/21.** `reload()`
  cannot be implemented in a GL-free translation unit — it must repeat whichever of `load()` /
  `loadManifestOnly()` ran last, and Full mode needs `uploadToGpu()` — so its two cases moved to
  a new GL-linked target (`noto3d-registry-reload-tests` / CTest `asset-registry-reload`), which is
  this ticket's own prescribed remedy for a facade-needing case. **No assertion was dropped**: the
  7 `check()` calls are the same 7, and 21/21 became 22/22 by adding one CTest entry, not by moving
  or deleting cases. Kept CPU-suite-GL-free was judged the higher priority; the alternative
  (relinking the whole suite to GL) is the thing this ticket exists to remove.
- **Amended (0038, review): no *existing* assertion or CTest name changed; the suite gained new
  ones.** The original wording above was too strong — it read as "the suite is untouched", which
  the added `asset-registry-reload` entry and the new pipeline tests below both contradict. What is
  actually held: all 21 pre-existing CTest names still exist, no pre-existing `check()` was
  removed or weakened, and 0038 *adds* 6 assertions (`testGpuDedupOrderingKeepsFallbackWarning`,
  which pins a real behaviour the pre-split code had and the first cut of this ticket lost) plus
  the one CTest entry.
- `--list-assets` output is byte-identical: `M3_ASSETS version=1`, the per-asset lines, `end
  count=6`, exit 0. It still never creates a GL context (it did not before either; now it does
  not even link GL).
- `noto3d-editor` and `noto3d_app` link graphs are **unchanged** (the editor genuinely needs the
  GPU facade; `noto3d_app` keeps its `collectDrawItems` seam).
- No new GL reach is introduced anywhere.

## Verification

- Clean dev + release build, 0 new warnings.
- **ctest green (22/22 dev + release)**, with `noto3d-registry-tests` now linking no GL.
- **Link-graph proof:** the `noto3d-registry-tests` link line does not contain `noto3d_renderer`,
  `glad`, or `libGL` (assert in the ticket, e.g. via `cmake --graphviz` or inspecting the
  generated link command). This is the ticket's headline acceptance.
- `--list-assets` still exits 0 with `count=6` and no GL context (run in a build dir; no display
  needed).
- `scripts/m5-close-matrix.sh` ALL PASS in both build dirs.

**Rolls under spec:** ADR-0012 ③ — priced consequence: the CPU path links no GL after the split.

---

## Evidence (0038, complete 2026-09-29)

**Link-graph proof — the ticket's headline.** `noto3d-registry-tests` link line, dev:

```
libnoto3d_asset_cpu.a libnoto3d_core.a _deps/glm-build/glm/libglm.a
_deps/tinyobjloader-build/libtinyobjloader.a _deps/spdlog-build/libspdlogd.a
```

No `noto3d_renderer`, no `glad`, no `libGL`. Identical in release. `noto3d_core` is a **test-only**
dep for the `log.hpp` facade (the same trade `noto3d-manifest-tests` makes) and links no GL — the GL
edge in this repo runs `noto3d_renderer` → `noto3d_core`, never the reverse. Because it *does*
contribute `lib/glad/include` to the include path, a new configure-time guard (guard 3, the same
shared function that polices the CPU library) asserts the test's include closure reaches no GL
header and no non-allowlisted `src/renderer/` header. Proven to fire:
`#include <glad/gl.h>` and `#include <model.hpp>` in the suite, and `#include <model.hpp>` in
`asset_registry_cpu.cpp`, all fail configure.

**`--list-assets` byte-identical.** Diffed against the pre-change implementation (old CLI body
stashed in place, same libraries) — identical output modulo the spdlog timestamp, both on the
clean manifest and with a deliberately warning-producing 7th entry (`axis_data.up_axis = "q"`),
where both print `M3_WARN [warn_probe] unsupported up_axis 'q'; no axis correction applied`.
That second run is the point: the output includes per-asset diagnostics, so a `parseManifest()`-only
CLI would have looked identical on today's clean manifest and silently dropped warnings later.
`M3_ASSETS version=1`, 6 asset lines, `end count=6`, exit 0, no GL context.

**GL symbols in the CPU archive.** `nm -C --defined-only libnoto3d_asset_cpu.a | grep -E '\b(gl|glad_gl)[A-Z][A-Za-z0-9_]*\s*\('`
and the `--undefined-only` equivalent both return nothing: 0 GL or glad function symbols, defined
or referenced. (Case-sensitivity is load-bearing: a `-i` flag makes `[A-Z]` match `glm`'s `m` and
reports ~100 false hits on `glm::vec` templates. Use the exact filters above.)
The registry's CPU entry points are
in the CPU library (`loadManifestOnly`, `loadModelShapesOnly`, all accessors,
`doLoadManifestOnly`, `runManifestPipeline`) and the GPU half is in the facade (`load`,
`loadModelFile`, `reload`, `doLoadFull`) — no symbol is defined twice (the one duplicate
`externalModel()` was caught by the linker and fixed). `SubMesh`/`Texture` appear only inside
mangled STL plumbing over *incomplete* types; see the ADR-0012 amendment on reading this boundary.

**Unchanged, as required:** `noto3d-editor` and `noto3d_app` still link the full facade
(`libnoto3d_asset.a`, `libnoto3d_renderer.a`, `libglad.a`); the `collectDrawItems` seam and the
borrowed-`DrawItem::model` static checks pass. All 21 pre-existing CTest names are unchanged.

**Also:** dev + release builds 0 new warnings; ctest 22/22 both; `scripts/m5-close-matrix.sh` ALL
PASS in both build dirs; CLI exit-code matrix unchanged (`--list-assets` 0; `--frames=-1`,
`--frames=abc`, `--bogus`, `--asset=nope`, `--asset=nope --demo` all 1). The frozen manifest fixture
was restored bit-for-bit (`git diff assets/manifest.json` empty; `asset-manifest-integrity` passes).
A duplicated `target_link_libraries(noto3d_asset ...)` call left by 0037 was removed.

### Code review (2026-09-29, two axes, fixed point `db5db54`)

Run on both axes: Standards (repo `docs/standards/`) and Spec (this ticket). Findings and outcomes:

- **Spec, real bug — fixed.** The first cut put the GPU dedup probe *ahead* of the `fallback_geometry`
  resolution. Pre-split (`git show db5db54:src/asset/asset_registry.cpp`, lines 156-174) resolved
  `fallback_geometry` **before** the GPU cache check, so an entry that hit the dedup fast path *and*
  had an unresolvable `fallback_geometry` still emitted `fallback path resolution error` and degraded
  `overall`. The reordering silently dropped that diagnostic. Unreachable through the frozen manifest
  (no MVP asset sets `fallback_geometry`), reachable through `load()` on a hand-written manifest, and
  invisible to the suite because the probe only answers true in Full mode. **Fixed** by restoring the
  original order, and **pinned** by `testGpuDedupOrderingKeepsFallbackWarning` — 6 new assertions that
  call `loadManifestCpu()` directly with a probe that always hits. Verified red-green: the new test
  fails against the pre-fix ordering and passes after.
- **Spec, doc inaccuracy — fixed.** The suite invariant still claimed "assertions and CTest names are
  unchanged", which the added CTest entry and the new tests contradict. Re-worded: no *existing*
  assertion or name changed; 0038 adds.
- **Standards, hard — fixed.** ADR-0012:223 still said the suite is 21/21, contradicting CHANGELOG and
  CONTEXT. Now 22/22.
- **Standards, judgement call — fixed.** The "one pipeline" claim was overstated: the standalone
  `loadModelShapesOnly()` path still hand-rolled a second copy of the ADR-0007 epilogue (parse
  diagnostics, up-axis warning, 0013 texture scan). Extracted as `validateCpuRecord()` in
  `asset_cpu`, now the single implementation both entry points run; only status *accounting* stays
  per-path (it is not ADR-0007-specified). Likewise `doLoadManifestOnly()`/`doLoadFull()` shared a
  duplicated parse-seed-run-move preamble, extracted as `runManifestPipeline()` so the two modes
  cannot differ in how a manifest is read. Both extractions are behaviour-preserving and the suite
  covers both paths (22/22 dev + release after).
- **Standards, judgement call — accepted as-is.** `renderer::Model` is spelled as an alias twice
  (`renderer/model.hpp`, `asset_registry.hpp`). A `model_fwd.hpp` would centralise it, but that adds
  a new `src/renderer/*.hpp` — an ADR-0012 ① classification decision this ticket should not make, and
  the allowlist guard is what forces that decision to be explicit. The duplication is compile-safe: a
  divergent alias is a conflicting-declaration error in the facade TU, not silent drift. Deferred.
- **Standards, nits — fixed.** Unused `<optional>` include dropped; the reload suite's `log_init`
  logger renamed (`noto3d-asset-reload-tests`) since it is a separate binary.
- **Spec, scope creep — accepted.** CHANGELOG/CONTEXT carry 0036 and 0037 entries: those are
  pre-existing uncommitted doc edits inherited from those sessions, not written by 0038.

Nothing outstanding. Re-verified after the review fixes: dev + release build clean, ctest 22/22 in
both, GL-free link line unchanged, 0 GL/glad symbols, negative include guards still fail configure.
