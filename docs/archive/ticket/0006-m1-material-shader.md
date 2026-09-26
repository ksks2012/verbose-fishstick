# 0006 — M1 Model/SubMesh/Material types + GLSL 330 base shaders + first draw (ADR-0006)

**What to build:**

The CPU data contract, the first shaders, and the **first visible draw**: the explicit cube test
asset (ADR-0001) rendered with base shader + material defaults. Scene-facing types must be fixed
before 0008/0009 (M2) and M4 build on them.

Contract (ADR-0006):
- `Model = std::vector<SubMesh>`; `SubMesh { Mesh, Material }` (OBJ/MTL may carry several materials).
- `Material` fields: `baseColor vec4 (linear)`, `opacity float` (default 1.0),
  `cullEnabled bool` (default true), `texture?` single optional diffuse (sRGB; **declared now,
  wired in 0009**), `shininess float` (default 32). Excluded now: emissive, metallic/roughness,
  normal mapping, PBR.
- `effectiveAlpha = baseColor.a * opacity` (canonical, DEFINITION.md; never silently extended).
- Base fragment shader outputs `baseColor.rgb` as **linear** color and `effectiveAlpha` as output
  alpha (`FragColor = vec4(baseColor.rgb, effectiveAlpha)`); the sRGB framebuffer converts
  linear→sRGB on output (ADR-0004) — the shader outputs linear values only.
- The M1 base shader does **not** sample `Material::texture`; texture binding/sampling is
  introduced by 0009.
- The shader never branches on missing data; missing normals/UVs are canonicalized by the loader
  (0008), not by the shader.

Deliverable: cube (from 0005) visible through the base shader; `effectiveAlpha` covered by a
headless-safe dependency-free test in `tests/`.

**Blocked by:** 0005

**Status:** completed (implemented + committed + tested; archived from active queue)

- [x] `renderer::Model`, `renderer::SubMesh`, `renderer::Material` as specified (ADRs 0005/0006)
- [x] `effectiveAlpha = baseColor.a * opacity` implemented and tested in `tests/`
- [x] GLSL `#version 330 core` vertex + fragment shaders in `shaders/`; fragment outputs
      `vec4(baseColor.rgb, effectiveAlpha)` (linear out; sRGB framebuffer converts)
- [x] M1 base shader does not sample `Material::texture` (bind/sample deferred to 0009)
- [x] No shader branching on missing vertex data (loader canonicalizes upstream in 0008)
- [x] First visible draw: cube test asset rendered with material defaults (manual verification)

**Points to ADR:** 0006 (data contract, material, effectiveAlpha)