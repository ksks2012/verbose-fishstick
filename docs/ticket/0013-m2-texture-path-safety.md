# 0013 — M2 texture path safety: apply manifest path rules to diffuseTextureRef (ADR-0007)

**What to build:**

The asset registry enforces path safety on `geometry` and `fallback_geometry` manifest fields
(no absolute paths, no `../` traversal — ADR-0007). `Material::diffuseTextureRef`, which is set
by the OBJ loader from MTL `map_Kd` values, does **not** receive the same checks. A maliciously
crafted or mis-authored MTL file could therefore escape the asset directory via `../` — the
same class of path traversal that manifest fields already guard against.

Contract:
- Before `Texture::fromFile()` is called in `uploadToGpu()`, validate `diffuseTextureRef`:
  - Reject absolute paths (starts with `/` or `\`).
  - Reject `../` traversal sequences.
  - On rejection: log a diagnostic at `LoadedWithWarnings`, leave `material.texture` unset
    (asset still renders with `baseColor` only; no FAILED).
- The validation must use the same predicate as the manifest path check in `asset_registry.cpp`
  (extract `unsafePath()` into a shared utility or apply it inline with the same rules).
- `reference_image` remains unaffected (it is never uploaded; this ticket is about MTL textures).

**Blocked by:** 0009

**Status:** ready-for-agent

- [ ] `diffuseTextureRef` validated for absolute path / `../` before `Texture::fromFile()` call
- [ ] Rejected texture ref → `LoadedWithWarnings` diagnostic; `material.texture` left unset
- [ ] Same predicate as the manifest geometry-path check (no duplicated logic)
- [ ] Headless test: OBJ with MTL `map_Kd ../escape.png` → diagnostic recorded, texture unset

**Points to ADR:** 0007 (path safety: no absolute paths, no `../`, no runtime network)
