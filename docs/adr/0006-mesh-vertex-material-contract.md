# ADR-0006 — Mesh / vertex / material data contract

Status: Accepted

## Context
The interfaces between M1 renderer, M2 asset loader and M4 scene must be fixed before writing
any of them. Real OBJ assets vary in normal/UV presence; material semantics must be unambiguous.

## Decision
- Vertex: interleaved `position vec3 + normal vec3 + uv vec2`, 32 B stride. No tangent/bitangent
  (no normal mapping in MVP/MVP+1).
- Indices: always `GL_UNSIGNED_INT` (real meshes may exceed 65,535 verts; uniform path).
- Missing normals → loader computes area-weighted vertex normals + diagnostic counter
  ("loaded successfully" ≠ "asset was clean").
- Missing UVs → canonical `(0,0)` + warning; texture over missing UVs → error + flat baseColor.
- Shader never branches on missing data; the loader canonicalizes everything upstream.
- Mesh: immutable after GPU upload (single interleaved VBO + EBO + VAO, upload once). Scene
  operates on Transform + Material only. No future vertex-edit API reserved.
- Model = `vector<SubMesh>`; `SubMesh { Mesh, Material }` (OBJ/MTL may carry several materials).
- Material fields: `baseColor vec4 (linear)`, `opacity float`, `cullEnabled bool`,
  `texture?` (single optional diffuse, sRGB), `shininess float` (default 32).
  Excluded: emissive, metallic/roughness, normal mapping, PBR.
- Alpha: `effectiveAlpha = baseColor.a * opacity`.
- Coordinates: Y-up, -Z forward; asset-specific axis correction recorded in the Asset Registry
  and baked into the CPU mesh once at load.

## Consequences
- Loader owns asset validation/canonicalization; downstream code is uniform.
- Editing is cheap state change (transform/material), never vertex re-upload.