# 0009 — M2 sRGB diffuse texture upload (ADR-0004)

**What to build:**

Wire `Material::texture?` (single optional diffuse, sRGB). Textures are uploaded as
`GL_SRGB8_ALPHA8` so hardware sampling returns linear values into the linear pipeline
(ADR-0004/0006). Decode with `glTexImage2D` internal format `GL_SRGB8_ALPHA8`; the shader reads
linear and the sRGB framebuffer converts on output.

Contract:
- Diffuse color textures use `GL_SRGB8_ALPHA8`; this is the color/diffuse case. Masks/lookup
  textures (future) are NOT assumed sRGB — not introduced now.
- PNG loading via stb_image (committed at M0, ADR-0003).
- The M0.5 pinned **reference PNGs are never uploaded as textures** — `reference_image` stays
  metadata-only (ADR-0002); verify with a dedicated texture unit test source, not a reference PNG.
- `effectiveAlpha` respects texture alpha: `baseColor.a * opacity * sampledAlpha`.
- Manual renderer verification (ADR-0008); a CPU-side check pins the chosen internal format.

**Blocked by:** 0008

**Status:** ready-for-agent

- [ ] `Material::texture?` uploads as `GL_SRGB8_ALPHA8` (hardware sRGB decode → linear in shader)
- [ ] stb_image loads the diffuse PNG; mip filter/sampling chosen consistently with MSAA baseline
- [ ] Reference PNGs (assets/references/**) are never bound as textures — verified via dedicated
      non-reference texture
- [ ] `effectiveAlpha = baseColor.a * opacity * sampledAlpha`
- [ ] CPU-side check pins `GL_SRGB8_ALPHA8` internal format selection

**Points to ADR:** 0004 (sRGB baseline, texture format), 0006 (texture? material slot),
0002/0007 (reference_image metadata-only)