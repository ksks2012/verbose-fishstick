# 0036 — extract `renderer::Vertex` into a GL-free `vertex.hpp`

**Source:** ADR-0012 ① (type ownership) — tracer bullet 1 of the 0027 spike execution.
**Status:** complete (2026-09-29) — evidence recorded below
**Blocked by:** none (pure header relocation; no behaviour change)

## What it delivers

`renderer::Vertex` — the 32 B interleaved layout (ADR-0006) — lives in its own **GL-free** header
`src/renderer/vertex.hpp`, so a CPU-only consumer can name the vertex type without reaching
`glad/gl.h`. `mesh.hpp` keeps only the GL-owning parts (`Attrib` `GLuint` locations, `Mesh`
VAO/VBO/EBO) and includes `vertex.hpp`.

Today `obj_loader.hpp` reaches `glad/gl.h` **only** because it includes `<mesh.hpp>` to name
`Vertex`; `material.hpp` is already GL-free (glm only). This ticket makes the asset CPU parse
header GL-free at the *header* level.

**Namespaces and types do not change.** `Vertex` stays `noto3d::renderer::Vertex`. This is a
header relocation, not a type move: no `using` alias, no rename, no consumer change beyond the
include line. The `static_assert(sizeof(Vertex) == 32, …)` moves with it (ADR-0006 contract
preserved verbatim).

## Scope

- New `src/renderer/vertex.hpp`: the `Vertex` struct + the 32 B `static_assert` + its comment
  block, with `#include <glm/glm.hpp>` only. **No** `glad/gl.h`. (`<cstdint>` is deliberately
  *not* included — `Vertex` names no `<cstdint>` type; include-what-you-use, cf. the
  GL-free `material.hpp`, which does include it because it uses `std::uint32_t`.)
- `src/renderer/mesh.hpp`: drop the `Vertex` definition and the `static_assert`, add
  `#include "vertex.hpp"`. `Attrib` (`GLuint`) and `Mesh` stay put; `mesh.hpp` keeps `glad/gl.h`.
  `#include <glm/glm.hpp>` also drops out here: `mesh.hpp` names no glm type of its own, and
  `vertex.hpp` re-supplies it. `mesh.hpp` keeps `<cstdint>` (the `Mesh` ctor takes
  `std::span<const std::uint32_t>`).
- `src/asset/obj_loader.hpp`: include `vertex.hpp` + `material.hpp` directly; it no longer needs
  `mesh.hpp`.
- `src/asset/asset_registry.hpp` and any other `Vertex` consumer: no change required — they already
  include `model.hpp`/`mesh.hpp` transitively for the GPU side. Only `obj_loader.hpp` actually
  needs the narrower include.

## Invariants

- **`lib/` untouched; no type renamed; no public API changed.** `noto3d::renderer::Vertex` is
  still spelled exactly as before at every call site (proved by a clean build of every consumer).
- `sizeof(Vertex) == 32` static_assert still compiles and holds.
- `Attrib` and `Mesh` remain in `mesh.hpp`; `mesh.hpp` still includes `glad/gl.h` (it must — it
  owns GL handles).
- Comment cross-references to the stride invariant are updated when it moves (`texture.cpp`'s
  "same style as the Vertex stride invariant at mesh.hpp" now points at `vertex.hpp`).

## Verification

- Clean dev + release build, **0 new warnings** (`-Wall -Wextra -Wpedantic`).
- **ctest green (21/21 dev + release)** — unchanged, since this touches only the header layout.
- **Static check — the CPU parse header is GL-free at the *header* level**, i.e. its **transitive
  include closure** contains no glad header:
  ```
  g++ -std=c++20 -M -Isrc/renderer -Isrc/core -Isrc/asset -Ilib/glad/include \
      src/asset/obj_loader.hpp | tr ' ' '\n' | grep -i glad     # must print nothing
  ```
  > **Correction (2026-09-29, this ticket).** The check originally written here —
  > `grep -l "glad/gl.h" src/asset/*.hpp` returns nothing — is **vacuous**: `obj_loader.hpp` never
  > contained the literal string `glad/gl.h`; it reached glad *transitively* through `<mesh.hpp>`.
  > That grep passed at HEAD and passes after the change, so it can never detect the defect this
  > ticket exists to fix. The closure check above is strictly stronger and genuinely closes the
  > requirement (at HEAD: 2 glad entries, `glad/gl.h` + `KHR/khrplatform.h`; after: 0).
  > 0037–0039 must use the closure form, not the literal grep.
- `scripts/m5-close-matrix.sh <build-dir> [--release]` still ALL PASS in both build dirs (no
  behavioural change expected).

**Rolls under spec:** ADR-0012 ① — type ownership (`Vertex` gains a GL-free home).

---

## Evidence (recorded 2026-09-29)

### Files changed (4; 1 new)
| File | Change |
|---|---|
| `src/renderer/vertex.hpp` | **new** — `Vertex` + `static_assert` + comment block; GL-free |
| `src/renderer/mesh.hpp` | dropped the `Vertex` definition/`static_assert` and `<glm/glm.hpp>`; added `#include "vertex.hpp"`; `Attrib` + `Mesh` + `glad/gl.h` unchanged |
| `src/asset/obj_loader.hpp` | `<mesh.hpp>` → `<vertex.hpp>` (the point of the ticket) |
| `src/renderer/texture.cpp` | stale comment fix: stride-invariant cross-reference `mesh.hpp` → `vertex.hpp` (found by `/code-review`) |

### Static check — the transitive closure, red → green
```
before (HEAD):  obj_loader.hpp  →  lib/glad/include/glad/gl.h, lib/glad/include/KHR/khrplatform.h
after:          obj_loader.hpp  →  (no glad)  GLAD-FREE ✓
```
`asset_registry.hpp` still reaches glad — correctly: it is the GPU-facing facade that exposes
`renderer::Model*`/`Texture`, which the ticket explicitly leaves to 0037.

### Build + test
- dev build clean, **0 warnings, 0 errors**; release build clean, **0 warnings**.
- **ctest 21/21 dev** and **21/21 release**.
- **`scripts/m5-close-matrix.sh build/dev` → ALL PASS**; **`build/release --release` → ALL PASS**
  (pre-GL exit-code rows, structured-log content + ordering for all three entry modes, the four
  headless app suites, and the three static seam checks).

### Invariants re-checked
- `Attrib` (`GLuint` locations) and `Mesh` (VAO/VBO/EBO) still in `mesh.hpp`; `mesh.hpp` still
  includes `glad/gl.h`; `sizeof(Vertex) == 32` assert compiles and holds.
- No type renamed, no `using` alias, no namespace change: every consumer still spells
  `noto3d::renderer::Vertex`. All other `Vertex` consumers (`mesh.cpp`, `mesh_test.cpp`,
  `renderer.cpp`, `main.cpp`) reach it transitively via `mesh.hpp` → `vertex.hpp`; no consumer
  needed editing.
- `lib/` untouched. Behavioural surface unchanged (ctest + both close matrices green).
- Pre-existing ADR-0008 breach untouched and out of scope: `mesh.hpp`'s two inline accessors
  (`vao()`, `indexCount()`) — inventoried in ticket 0035.

### Code review
Both axes run via `/code-review` (fixed point HEAD `1a57a3f`).
- **Standards** — 0 hard violations. One minor finding, **fixed**: the stale `mesh.hpp`
  cross-reference in `texture.cpp`. The `<cstdint>` omission judged **not** a breach (IWYU; the
  GL-free sibling `material.hpp` includes it because it uses `std::uint32_t`). A judgement-call
  observation about the `Attrib` locations being duplicated as prose in `vertex.hpp` was left as-is
  (a `static_assert` there would be tautological; `vertex.hpp` already points at `mesh.hpp` for the
  locations).
- **Spec** — implementation faithful. Two micro-deviations from the Scope bullets, both recorded
  above and judged improvements: `<glm/glm.hpp>` also dropped from `mesh.hpp` (it names no glm
  type; `vertex.hpp` re-supplies), and 5 added prose lines in `vertex.hpp` documenting *why* the
  header is GL-free. Flagged the **vacuous static check**, now corrected in this ticket.

