# Execution Summary: 0036-extract-vertex-gl-free-header

- **Date**: 2026-09-29
- **Spec/ADR Path**: `docs/adr/0012-asset-cpu-gpu-boundary.md` ① (type ownership)
- **Ticket Path**: `docs/ticket/0036-extract-vertex-gl-free-header.md` (complete; archived at close
  of the 0036–0039 chain, per 0039)
- **Fixed point for review**: HEAD `1a57a3f`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 new (working tree, uncommitted per implement skill `/commit` disabled)
- **Files changed**: 7 (1 new: `src/renderer/vertex.hpp`; 3 code edits; 3 docs)
- **Behaviour change**: **none** — this is a header relocation. Every consumer still spells
  `noto3d::renderer::Vertex`; ctest and both close matrices are unchanged.
- **Ticket status**: ready-for-agent → **complete** (evidence recorded in the ticket).

## 🎯 What it delivered (ADR-0012 ① — tracer bullet 1)
The 32 B `Vertex` layout (ADR-0006) now lives in its own **GL-free** header
`src/renderer/vertex.hpp`. `mesh.hpp` keeps only the GL-owning half — the `Attrib` `GLuint`
locations and the `Mesh` class (VAO/VBO/EBO) — and includes `vertex.hpp`. The asset CPU parse
header `src/asset/obj_loader.hpp` names `Vertex` via `vertex.hpp` instead of reaching
`glad/gl.h` through `mesh.hpp`.

| File | Change |
|---|---|
| `src/renderer/vertex.hpp` | **new** — `Vertex` + the `sizeof==32` `static_assert` + comment block; glm only, no glad |
| `src/renderer/mesh.hpp` | dropped the `Vertex` definition, the assert, and the now-unused `<glm/glm.hpp>`; added `#include "vertex.hpp"`; `Attrib`/`Mesh`/`glad/gl.h` untouched |
| `src/asset/obj_loader.hpp` | `<mesh.hpp>` → `<vertex.hpp>` — the point of the ticket |
| `src/renderer/texture.cpp` | stale comment fix: stride-invariant cross-reference `mesh.hpp` → `vertex.hpp` (from `/code-review`) |

**No type renamed, no namespace change, no `using` alias** — the ticket deliberately scoped this
as a *relocation*, not a type move. Every other `Vertex` consumer (`mesh.cpp`, `mesh_test.cpp`,
`renderer.cpp`, `main.cpp`) reaches it transitively via `mesh.hpp` → `vertex.hpp`, so none needed
editing. `lib/` untouched. `asset_registry.hpp` still reaches glad **by design** — it is the
GPU-facing facade exposing `renderer::Model*`/`Texture`, which is 0037's job.

## 🔬 The check that actually mattered (and the ticket bug it exposed)
The ticket's static check was `grep -l "glad/gl.h" src/asset/*.hpp` returns nothing. That check is
**vacuous**: `obj_loader.hpp` never contained the literal string `glad/gl.h` — it reached glad
*transitively* through `<mesh.hpp>`, so the grep passed at HEAD and would pass after any edit,
including no edit at all. It cannot detect the defect the ticket exists to fix.

Replaced with the **transitive include closure**, which is strictly stronger and genuinely red:

```
g++ -std=c++20 -M -Isrc/renderer -Isrc/core -Isrc/asset -Ilib/glad/include \
    src/asset/obj_loader.hpp | tr ' ' '\n' | grep -i glad
```
- before (HEAD `1a57a3f`): 2 hits — `lib/glad/include/glad/gl.h`, `lib/glad/include/KHR/khrplatform.h`
- after: **0 hits** ✓

The ticket was amended (in the same change) so 0037–0039 reviewers do not re-run the no-op grep.

## ✅ Verification
- **dev build**: 0 warnings, 0 errors (`-Wall -Wextra -Wpedantic`); **release build**: 0 warnings.
- **ctest 21/21 dev + release** (unbroken — 21/21 was the count before and after).
- **`scripts/m5-close-matrix.sh build/dev` → ALL PASS**; **`build/release --release` → ALL PASS**
  (pre-GL exit-code rows, `M0_OK`→`M3_INFO`/`M2_OK`→`M4_INFO entities=N`→`M1_DONE` content +
  ordering for all three entry modes, the four headless app suites, the three static seam checks).
- Invariants re-checked: `Attrib` + `Mesh` still in `mesh.hpp`; `mesh.hpp` still includes
  `glad/gl.h`; the 32 B `static_assert` compiles and holds; `mesh.hpp` keeps `<cstdint>` (the
  `Mesh` ctor takes `std::span<const std::uint32_t>`).

## 🔍 Code review (`/code-review`, two parallel axes, fixed point HEAD `1a57a3f`)
**Standards** — 0 hard violations. One minor finding, **fixed**: the stale `mesh.hpp`
cross-reference in `texture.cpp:13` (the stride invariant had moved out). The `<cstdint>` omission
was judged **not** a breach (include-what-you-use; the GL-free sibling `material.hpp` does include
it because it uses `std::uint32_t`). A judgement-call note about the `Attrib` locations being
mirrored as prose in `vertex.hpp` was left alone — a `static_assert` there would be tautological,
and `vertex.hpp` already points at `mesh.hpp` for the locations.

**Spec** — implementation faithful. Two micro-deviations from the Scope bullets, both recorded in
the ticket and judged improvements: `<glm/glm.hpp>` also dropped from `mesh.hpp` (it names no glm
type of its own; `vertex.hpp` re-supplies it) and 5 added prose lines in `vertex.hpp` explaining
*why* the header is GL-free. The Spec axis independently confirmed the vacuous-check finding and
that the `<cstdint>` omission is a spec improvement, not a deviation ("only" caps the set; it does
not mandate both).

## 📌 Note on scope
This is header-level decoupling only. The **link**-level win the spike was opened to price — a
GL-free `noto3d_asset_cpu` so `tests/asset` and `--list-assets` stop linking GL — is 0037/0038. As
ADR-0012 records, the "single library with a GL-free header subset" shape was rejected as the
*final* decision precisely because it cannot sever the link; it is kept as this first, cheap step
on the way to the real split.
