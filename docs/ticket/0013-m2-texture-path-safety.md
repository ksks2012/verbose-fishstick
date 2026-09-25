# 0013 — M2 texture path safety: apply manifest path rules to diffuseTextureRef (ADR-0007)

**What to build:**

The asset registry enforces path safety on `geometry` and `fallback_geometry` manifest fields
(no absolute paths, no `../` traversal — ADR-0007). `Material::diffuseTextureRef`, which is set
by the OBJ loader from MTL `map_Kd` values, does **not** receive the same checks. A maliciously
crafted or mis-authored MTL file could therefore escape the asset directory via `../` — the
same class of path traversal that manifest fields already guard against.

Contract:
- Before `Texture::fromFile()` is called in `uploadToGpu()`, validate `diffuseTextureRef` by
  **reusing the existing manifest path-safety predicate** (extract `unsafePath()` in
  `asset_registry.cpp` into a shared utility, or call the same predicate inline). Same rules, no
  duplicated or re-invented path grammar — this ticket does not re-specify what is / is not an
  absolute or `../` path; that is defined once by the manifest predicate (covering e.g.
  `foo/../../bar` while not misjudging `foo..bar`).
- On rejection: **promote an otherwise successful asset load to `LOADED_WITH_WARNINGS`**
  (0008 tri-state: SUCCESS / LOADED_WITH_WARNINGS / FAILED), log a diagnostic, and leave
  `material.diffuseTextureRef` unset (asset still renders with `baseColor` only; not FAILED). A
  rejected optional diffuse texture must not destroy otherwise valid geometry — a plain
  `log + texture = null + SUCCESS` would break the tri-state contract and is not acceptable.
- `reference_image` remains unaffected (it is never uploaded; this ticket is about MTL textures).
- This is a **hardening pass after 0009**, not a prerequisite of 0009 or 0014: 0014 conforms to
  the ADR-0007 path contract when authoring its fixture; 0013 turns that contract into enforced
  shared code.

**Blocked by:** 0009

**Status:** ready-for-agent

- [ ] `diffuseTextureRef` validated via the shared manifest path-safety predicate before the
      `Texture::fromFile()` call
- [ ] Rejected texture ref → overall asset load promoted to `LOADED_WITH_WARNINGS`;
      `material.diffuseTextureRef` left unset; asset still renders with `baseColor`
- [ ] Same predicate as the manifest geometry-path check (no duplicated logic)
- [ ] Headless test: OBJ with MTL `map_Kd ../escape.png` → `LOADED_WITH_WARNINGS`, texture ref unset

**Points to ADR:** 0007 (path safety: no absolute paths, no `../`, no runtime network)
