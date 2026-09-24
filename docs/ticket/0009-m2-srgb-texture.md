# 0009 — M2 sRGB diffuse texture upload (ADR-0004)

**What to build:**

Wire `Material::texture?` (single optional diffuse, sRGB). Textures are uploaded with internal
format `GL_SRGB8_ALPHA8`, so hardware sampling returns linear values into the linear pipeline
(ADR-0004/0006); the fragment shader reads linear and the sRGB framebuffer converts on output.

Pipeline is fixed and color-managed end to end:

```text
PNG → stb_image → GL_SRGB8_ALPHA8 → hardware sRGB decode → linear shader → GL_FRAMEBUFFER_SRGB → sRGB framebuffer
```

Contract:
- Diffuse color textures use `GL_SRGB8_ALPHA8`. Masks/lookup textures (future) are NOT assumed
  sRGB — not introduced now.
- Texture **minification/magnification filters and mipmap policy are explicitly selected for the
  MVP and applied consistently** (e.g. `GL_LINEAR_MIPMAP_LINEAR` + `GL_LINEAR`, or `GL_LINEAR`/
  `GL_LINEAR` with no mipmaps). This decision is independent of MSAA (which concerns
  framebuffer/sample coverage, not texture sampling).
- PNG loading via stb_image (committed at M0, ADR-0003).
- The M0.5 pinned **reference PNGs are never uploaded as textures** — `reference_image` stays
  metadata-only (ADR-0002); verify with a dedicated non-reference texture.
- Alpha stays canonical: `effectiveAlpha = baseColor.a * opacity` (DEFINITION.md) is unchanged;
  this ticket defines `finalAlpha = effectiveAlpha * sampledAlpha`. The DEFINITION of
  `effectiveAlpha` is not extended.
- Sampling validation: `finalAlpha` drives output alpha; shader still outputs linear RGB.

**Blocked by:** 0008

**Status:** ready-for-agent

- [ ] Diffuse upload requests `GL_SRGB8_ALPHA8` for color textures (dependency-free CPU-side test
      pins the internal-format selection on the upload path — it verifies what the app requests,
      not the driver's actual storage)
- [ ] stb_image loads the diffuse PNG; min/mag/mipmap policy explicitly selected and applied
      consistently (chosen independently of MSAA)
- [ ] Reference PNGs (assets/references/**) are never bound as textures — verified via a dedicated
      non-reference texture
- [ ] `finalAlpha = effectiveAlpha * sampledAlpha`; `effectiveAlpha` definition unchanged
- [ ] Fragment shader samples diffuse, decodes via sRGB format, outputs linear RGB + `finalAlpha`

**Points to ADR:** 0004 (sRGB baseline, texture format & color pipeline), 0006 (texture? material
slot, effectiveAlpha), 0002/0007 (reference_image metadata-only)