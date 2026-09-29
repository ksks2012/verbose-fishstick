# 0036 — extract `renderer::Vertex` into a GL-free `vertex.hpp`

**Source:** ADR-0012 ① (type ownership) — tracer bullet 1 of the 0027 spike execution.
**Status:** ready-for-agent
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
  block, with `#include <glm/glm.hpp>` and `<cstdint>` only. **No** `glad/gl.h`.
- `src/renderer/mesh.hpp`: drop the `Vertex` definition and the `static_assert`, add
  `#include "vertex.hpp"`. `Attrib` (`GLuint`) and `Mesh` stay put; `mesh.hpp` keeps `glad/gl.h`.
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

## Verification

- Clean dev + release build, **0 new warnings** (`-Wall -Wextra -Wpedantic`).
- **ctest green (21/21 dev + release)** — unchanged, since this touches only the header layout.
- Static check: `grep -l "glad/gl.h" src/asset/*.hpp` returns **nothing** (the CPU parse headers
  are now GL-free at the header level).
- `scripts/m5-close-matrix.sh` still ALL PASS in both build dirs (no behavioural change expected).

**Rolls under spec:** ADR-0012 ① — type ownership (`Vertex` gains a GL-free home).
