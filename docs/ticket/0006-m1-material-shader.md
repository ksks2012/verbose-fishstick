# 0006 — M1 Model/SubMesh/Material types + GLSL 330 base shaders (ADR-0006)

**What to build:**

The CPU data contract and the first shaders. Scene-facing types must be fixed before 0008/0009
(M2) and M4 (scene) build on them.

Contract (ADR-0006):
- `Model = std::vector<SubMesh>`; `SubMesh { Mesh, Material }` (OBJ/MTL may carry several materials).
- `Material` fields: `baseColor vec4 (linear)`, `opacity float` (default 1.0),
  `cullEnabled bool` (default true), `texture?` single optional diffuse (sRGB, wired in 0009),
  `shininess float` (default 32). Excluded now: emissive, metallic/roughness, normal mapping, PBR.
- `effectiveAlpha = baseColor.a * opacity`.
- Shaders live in `shaders/`, `#version 330 core` (ADR-0004). Fragment reads `baseColor`
  (linear) and multiplies by `effectiveAlpha`; the sRGB framebuffer performs the linear→sRGB
  output conversion (`GL_FRAMEBUFFER_SRGB`, ADR-0004) — the shader outputs linear values only.
  sRGB diffuse texture decode arrives with the texture wiring (0009).
- The shader never branches on missing data; missing normals/UVs are canonicalized by the loader
  (M2/0008), not by the shader.

Deliverable: cube (0005) is drawn with these shaders/material defaults; headless-safe material
computations (e.g. `effectiveAlpha`) are covered by a dependency-free test in `tests/`.

**Blocked by:** 0005

**Status:** ready-for-agent

- [ ] `renderer::Model`, `renderer::SubMesh`, `renderer::Material` as specified (ADRs 0005/0006)
- [ ] `effectiveAlpha = baseColor.a * opacity` implemented and tested in `tests/`
- [ ] GLSL `#version 330 core` vertex + fragment shaders in `shaders/`; shader emits linear,
      sRGB framebuffer converts on output
- [ ] `Material::cullEnabled` default true, honored at draw in 0007
- [ ] No shader branching on missing vertex data (loader canonicalizes upstream in 0008)

**Points to ADR:** 0006 (data contract, material, effectiveAlpha)