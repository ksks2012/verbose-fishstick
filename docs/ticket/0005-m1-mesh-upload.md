# 0005 — M1 mesh GPU upload (ADR-0006)

**What to build:**

First renderer deliverable: the immutable mesh pipeline. Define the noto3d::renderer Mesh type
that owns a single interleaved VBO + EBO + VAO, uploads once, and is immutable after upload.

Contract (ADR-0006):
- Vertex = interleaved `position vec3 + normal vec3 + uv vec2`, **32 B stride**; enforce with a
  `static_assert(sizeof(Vertex) == 32)`. No tangent/bitangent.
- Indices are always `GL_UNSIGNED_INT` (real meshes may exceed 65,535 verts; uniform path).
- Mesh is immutable after GPU upload: one upload, deleted copy, movable; no future vertex-edit API.
- Render via the explicit cube test asset only (ADR-0001 — the cube is a test asset, never an
  implicit fallback). Placeholder OBJ loading is deferred to M2/0008; M1 feeds known-good CPU data.
- Data is authored Y-up (ADR-0005); axis correction is baked by the loader (M2), not here.

Deliverable: a cube rendered with the base GLSL 330 shaders produced by 0006, visible in the
window with depth/cull/blend uses coming from 0007. No pass hygiene yet (0007 owns that).

**Blocked by:** none (M1 start)

**Status:** ready-for-agent

- [ ] `renderer::Vertex` (pos+normal+uv) with `static_assert(sizeof(Vertex) == 32)`
- [ ] `renderer::Mesh` owns interleaved VBO + EBO + VAO; uploaded once; immutable after upload;
      movable, non-copyable
- [ ] Indices bound as `GL_UNSIGNED_INT`
- [ ] Explicit cube test asset renders through M1 shaders (manual renderer verification, ADR-0008)
- [ ] Y-up authoring respected; axis correction left to the M2 loader (placeholder data is Y-up)

**Points to ADR:** 0006 (mesh contract), 0001 (cube = explicit test asset only)