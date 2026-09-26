# 0005 — M1 Mesh GPU upload (ADR-0006)

**What to build:**

Pure GPU resource ticket. Define `noto3d::renderer::Vertex` and an immutable `renderer::Mesh`
that owns a single interleaved VBO + EBO + VAO and uploads once. This ticket has **no rendering
obligation** — the first draw is 0006's job, pass hygiene is 0007's job.

Contract (ADR-0006):
- `Vertex` = interleaved `position vec3 + normal vec3 + uv vec2`, **32 B stride**; enforced with
  `static_assert(sizeof(Vertex) == 32)`. No tangent/bitangent.
- Indices are always `GL_UNSIGNED_INT`.
- CPU vertex/index arrays are **constructor/upload inputs**; `Mesh` does not expose a vertex/index
  mutation API after upload. Whether `Mesh` retains the CPU copy is implementation freedom.
- GPU handles have **single ownership and are released exactly once** (RAII); move operations
  transfer ownership; a moved-from `Mesh` must be safely destructible.
- Data is authored Y-up (ADR-0005); axis correction is baked by the M2 loader, not here.

Delivered to 0006: a `Mesh` that 0006 can fill with the cube test asset and draw through the base
shaders (ADR-0001 — the cube is an explicit test asset, never an implicit fallback).

**Blocked by:** none

**Status:** completed (implemented + committed + tested; archived from active queue)

- [x] `renderer::Vertex` (pos+normal+uv) with `static_assert(sizeof(Vertex) == 32)`
- [x] `renderer::Mesh` owns interleaved VBO + EBO + VAO; uploaded once; immutable after upload;
      movable, non-copyable
- [x] Indices bound as `GL_UNSIGNED_INT`
- [x] CPU arrays are construction inputs; no post-upload vertex/index mutation API
- [x] GPU handles released exactly once (RAII); move transfers ownership; moved-from safe
- [x] No rendering or state obligations (first draw = 0006, pass state = 0007)

**Points to ADR:** 0006 (mesh contract), 0001 (cube = explicit test asset only)