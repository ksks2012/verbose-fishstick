# 0009 — M2 sRGB diffuse texture upload (ADR-0004)

**What to build:**

Texture **runtime pipeline** that consumes the demo fixture built by 0014. 0014 proves the
fixture resolves to `Material.diffuseTextureRef` (CPU); 0009 turns that reference into a rendered
diffuse texture. 0009 does not own the asset contract — 0014 does.

```text
PNG (0014 fixture)
   ↓
stb_image
   ↓
glTexImage2D(allocation requests GL_SRGB8_ALPHA8)
   ↓
OpenGL sRGB texture sampling converts sampled RGB → linear before shader use
   ↓
linear shader (consumes linear; does NOT decode)
   ↓
GL_FRAMEBUFFER_SRGB
   ↓
sRGB framebuffer
```

Contract:
- Diffuse color textures request internal format `GL_SRGB8_ALPHA8` at allocation. Masks/lookup
  textures (future) are NOT assumed sRGB — not introduced now.
- **sRGB decode happens in the sampler, not the shader**: the texture is allocated with
  `GL_SRGB8_ALPHA8`; OpenGL's sRGB texture sampling converts sampled RGB to linear space before
  the shader sees it. The shader consumes linear and outputs linear.
- Texture minification/magnification filters and mipmap policy are explicitly selected for the MVP
  and applied consistently (`GL_LINEAR_MIPMAP_LINEAR` + `GL_LINEAR`, mipmap chain generated).
  This decision is independent of MSAA.
- PNG loading via stb_image (committed at M0, ADR-0003).
- **MVP `uHasTexture` branch** (NOT a 1×1-white dummy texture) with locked no-texture semantics:

  ```glsl
  sampler2D uDiffuse;   // always bound to texture unit 0 while the shader is active;
                        // glUniform1i(uDiffuse, 0) at program setup
  uniform bool uHasTexture;

  vec4 texColor = vec4(1.0);        // no texture → identity
  float sampledAlpha = 1.0;
  if (uHasTexture) {
      texColor = texture(uDiffuse, vUV);
      sampledAlpha = texColor.a;
  }
  // finalColor.rgb = baseColor.rgb * texColor.rgb
  // finalAlpha = effectiveAlpha * sampledAlpha
  ```

  `uHasTexture = false` means the sampler is not sampled (its bind is irrelevant then); the
  no-texture semantics are `sampledAlpha = 1.0`, `texColor = vec4(1.0)`.

  This does not violate ADR-0006 "no branch on missing vertex data": UVs are canonicalized by the
  loader, so the branch is on Material-has-texture, not on missing per-vertex attributes.
  Forcing a 1×1-white dummy would turn "no texture" into a renderer-level dummy resource with
  extra lifecycle/binding rules for an optionally-textured Material.
- Alpha stays canonical: `effectiveAlpha = baseColor.a * opacity` (DEFINITION.md) unchanged;
  this ticket defines `finalAlpha = effectiveAlpha * sampledAlpha`. `sampledAlpha = 1.0` when no
  diffuse texture is bound.
- Reference PNGs (`assets/references/**`) are never bound as textures — verified end-to-end using
  the dedicated non-reference fixture from 0014.

**Blocked by:** 0014 for end-to-end demo verification only (core implementation does not wait on
0014; the 0008 → 0014 → 0009 chain supersedes the old 0008 → 0009 ordering)

**Status:** in-progress

- [x] Regression check: `static_assert(sizeof(Vertex) == 32)` remains present at
      `src/renderer/mesh.hpp` (ADR-0006 Vertex-stride invariant; 0009 does not modify Mesh)
- [x] Diffuse allocation requests `GL_SRGB8_ALPHA8`, pinned by
      `static_assert(Texture::kDiffuseInternalFormat == GL_SRGB8_ALPHA8)` — a compile-time API
      contract on the application-side requested internal format (verifies what the app requests,
      not the driver's actual storage)
- [x] stb_image loads the diffuse PNG; min/mag/mipmap policy explicitly selected and applied
      consistently (chosen independently of MSAA)
- [x] MVP `uHasTexture` branch semantics locked: no texture → `sampledAlpha = 1.0` and
      `texColor = vec4(1.0)`; `finalColor.rgb = baseColor.rgb * texColor.rgb`;
      `finalAlpha = effectiveAlpha * sampledAlpha`; `effectiveAlpha` unchanged (shader: `uniform
      bool uHasTexture`; `uDiffuse` — no sample when false)
- [x] Fragment shader samples diffuse; OpenGL sRGB sampling converts to linear (shader does not
      decode); shader outputs linear RGB + `finalAlpha`
- [ ] Consumes the 0014 fixture: `Material.diffuseTextureRef` → `Texture::fromFile` (`GL_SRGB8_ALPHA8`)
      → demo render; Reference PNGs (`assets/references/**`) never bound — **E2E demo verification
      (manual per ADR-0008): pending — the editor does not yet consume the AssetRegistry (app
      wiring is 0012 territory)**

> Implementation progress: committed `6225dac` (`renderer::Texture::fromFile` + GL_SRGB8_ALPHA8 +
> mipmap/filter policy, shader sampling + `finalAlpha`, registry `diffuseTextureRef → Texture`
> wiring); staged: `static_assert(kDiffuseInternalFormat == GL_SRGB8_ALPHA8)`, shader
> bool/branch refinement. Remaining: manual E2E demo render once the editor consumes the registry.

**Points to ADR:** 0004 (sRGB baseline, texture format & color pipeline), 0006 (texture? material
slot, effectiveAlpha), 0002/0007 (reference_image metadata-only)