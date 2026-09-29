# Execution Summary: 0038-asset-cpu-no-gl-link

- **Date**: 2026-09-29
- **Spec/ADR Path**: `docs/adr/0012-asset-cpu-gpu-boundary.md` ③ (link-graph boundary)
- **Ticket Path**: `docs/ticket/0038-asset-cpu-no-gl-link.md` (complete; archived at close of the
  0036–0039 chain, per 0039)
- **Fixed point for review**: HEAD `db5db54` (0037)
- **Branch**: `main`

---

## 📌 Overview

- **Commits**: 0 new (working tree, uncommitted per implement skill `/commit` disabled)
- **Files changed**: 13 (2 new: `src/asset/asset_registry_cpu.cpp`, `tests/asset/registry_reload_test.cpp`)
- **Behaviour change**: **none intended** — a link-graph change. The `--list-assets` output is
  byte-identical, the 21 pre-existing CTest names all survive, and `noto3d-editor`/`noto3d_app`
  link graphs are untouched. The review did surface and fix one real behaviour regression in
  Full mode; see "Review outcome" below.
- **Ticket status**: ready-for-agent → **complete** (evidence recorded in the ticket)

## 🎯 What it delivered (ADR-0012 ③ — tracer bullet 3)

The CPU-suite and the read-only CLI stop depending on GL:

- `noto3d-registry-tests` links `noto3d_asset_cpu` + `noto3d_core` — no `noto3d_renderer`, no
  `glad`, no `libGL`.
- `--list-assets` no longer constructs an `AssetRegistry` at all; it drives `parseManifest()` +
  `loadManifestCpu()` directly.
- `AssetRegistry`'s public API is unchanged. The class was split across two libraries by
  *definition*, not by declaration: `loadManifestOnly()`, `loadModelShapesOnly()` and the
  read-only accessors live in `asset_registry_cpu.cpp` (no GL symbol reachable); `load()`,
  `loadModelFile()`, `reload()` and `doLoadFull()` stay in the facade. `reload()` had to stay
  there — it must repeat whichever mode ran last, and Full mode needs `uploadToGpu()`.

The single shared include-closure guard `noto3d_assert_target_reaches_no_gl()` in `CMakeLists.txt`
makes the boundary self-enforcing: any GL or windowing header that reaches a CPU target fails
*configure* with a named offender, instead of being caught later by a link error or not at all.
`noto3d_core` still has to be linked by the CPU test (for `log.hpp`), and it exposes the glad
include directory — the guard is what closes that gap.

## 🔍 Two pipeline invariants worth knowing (both now pinned by tests)

1. **The GPU dedup probe fires before the CPU parse.** A second manifest entry sharing a
   canonical geometry path reports Success with no parse and no diagnostics. It is observable
   through `uniqueGeometryCount()`, so reordering it breaks a test.
2. **The GPU dedup probe fires *after* the `fallback_geometry` resolution.** This one was not
   covered, and the first cut of this ticket got it backwards. The symptom was invisible on the
   frozen manifest (no MVP asset sets `fallback_geometry`) and unreachable through the GL-free
   suite (the probe only answers true in Full mode), so nothing failed — but an entry that
   short-circuited to an already-uploaded model silently stopped reporting an unresolvable
   `fallback_geometry`, and stopped degrading `overall`. Fixed, and pinned by
   `testGpuDedupOrderingKeepsFallbackWarning`.

Writing that test required one non-obvious fixture: `weakly_canonical()` tolerates non-existent
components, so a bogus `fallback_geometry` resolves cleanly and never reaches the catch. A
**symlink cycle** is what actually makes canonicalisation fail (ELOOP).

## 🧭 Review outcome (two axes, fixed point `db5db54`)

| Finding | Axis | Outcome |
| --- | --- | --- |
| GPU dedup probe ordered ahead of `fallback_geometry` resolution (real regression) | Spec | **Fixed + pinned by a red-green test** |
| Suite invariant claimed "assertions and CTest names unchanged" while adding both | Spec | **Re-worded** |
| ADR-0012:223 stale `21/21` vs CHANGELOG/CONTEXT `22/22` | Standards (hard) | **Fixed** |
| ADR-0007 epilogue implemented twice (manifest + standalone) | Standards (judgement) | **Extracted** as `validateCpuRecord()` |
| Parse/seed/run preamble duplicated in both load drivers | Standards (judgement) | **Extracted** as `runManifestPipeline()` |
| `renderer::Model` alias spelled in two headers | Standards (judgement) | **Accepted, deferred** — see below |
| Unused `<optional>`; duplicate `log_init` logger name | Standards (nits) | **Fixed** |

The two extractions exist because the header comment claimed "the whole ADR-0007 per-entry CPU
contract in one place", and the code did not honour it. The review was right and the comment was
the thing that was wrong; both are now true. Status *accounting* (tri-state counts) deliberately
stays per-path, because ADR-0007 does not specify it.

`renderer::Model` is declared as an alias in both `renderer/model.hpp` and `asset_registry.hpp`.
A `model_fwd.hpp` would centralise it, but that means adding a new `src/renderer/*.hpp` — an
ADR-0012 ① classification decision, and the guard is deliberately built to force such decisions
to be explicit rather than silent. The duplication is also compile-safe: a divergent alias is a
conflicting-declaration error in the facade TU, not slow drift. Left for a follow-up.

## ✅ Verification (all re-run after the review fixes)

- dev + release build: 0 errors, 0 new warnings
- `ctest`: **22/22** in both build dirs (21 pre-existing names + `asset-registry-reload`)
- `scripts/m5-close-matrix.sh`: ALL PASS in both build dirs
- `--list-assets`: `M3_ASSETS version=1`, 6 asset lines, `end count=6`, exit 0
- old-vs-new CLI output, timestamps stripped: identical — including a probe run with
  `axis_data.up_axis = "q"`, which is what proves the CLI still surfaces per-asset diagnostics
  (a `parseManifest()`-only CLI would look identical on today's clean manifest and drop warnings
  later). The manifest fixture was restored bit-for-bit.
- negative guard: injecting `#include <glad/gl.h>` into the CPU TU fails configure with the
  header named
- `nm -C --defined-only` / `--undefined-only libnoto3d_asset_cpu.a`: 0 GL or glad function
  symbols. (Case-sensitivity matters: with `-i`, `[A-Z]` matches `glm`'s `m` and reports ~100
  false hits. The exact filters are recorded in the ticket.)
- `noto3d-editor` / `noto3d_app` still link `libnoto3d_asset.a libnoto3d_renderer.a libglad.a`;
  the `collectDrawItems` borrowed-pointer seam is intact

## 🧠 Reasoning Notes

- **The public API stayed frozen on purpose.** `loadManifestOnly()` had to be declared in a header
  that cannot see `<model.hpp>`, so `renderer::Model` is an alias over an *incomplete* `SubMesh`
  (`std::vector<SubMesh>` allows incomplete elements). The incomplete-type names show up in
  mangled STL plumbing; that is expected and documented, not a leak. The authoritative boundary is
  the link line plus the include-closure guard, not `nm` alone.
- **The CLI had to move to the pipeline, not just to `parseManifest()`.** Diagnostics are produced
  during the CPU load, not the parse. Had the CLI stopped at `parseManifest()`, the per-asset
  warnings would have disappeared the first time a manifest was not clean — the exact failure mode
  the `up_axis = "q"` probe was run to disprove.
- **The suite count went 21 → 22, and that amendment is recorded rather than smoothed over.**
  `reload()` cannot exist in a GL-free TU, so its 2 cases moved to a new GL-linked target
  (`noto3d-registry-reload-tests`) — the ticket's own prescribed remedy for a facade-needing case.
  No assertion was dropped; the 7 `check()` calls are the same 7.

## ➡️ Next

- **0039** closes the 0036–0039 chain: graph record, full regression, doc archiving. Ticket 0038 is
  complete and not archived, per 0039's scope.
- Optional follow-up (not scheduled): `model_fwd.hpp` to hold `SubMesh`/`Model` aliases, which
  would remove the last accepted duplication on this boundary.
