# 0008 — M2 AssetRegistry runtime + OBJ/MTL→Mesh pipeline (ADR-0007)

**What to build:**

Runtime half of ADR-0007 (manifest data contract exists from M0.5). Load `assets/manifest.json`
at startup via nlohmann/json into a read-only `noto3d::asset::AssetRegistry`, resolve stable IDs
to immutable Models, and give diagnostics per ADR-0007 without ever throwing in the render loop.
Pull tinyobjloader now (ADR-0003 defers it to M2).

Pipeline is explicitly a **reuse of 0005**, not a new GPU type:

```text
OBJ/MTL → CPU canonical mesh (load, canonicalize) → renderer::Mesh (0005) → GPU upload → Model
```

Contract (ADR-0007):
- Registry is a read-only view of the manifest; entry = `id` (stable ASCII slug),
  `reference_image?`, `geometry?`, `geometry_source`.
- Invariant: `geometry_source ∈ {placeholder, official}` iff `geometry != null`, else null —
  invalid manifests rejected with a diagnostic.
- Paths relative to the manifest parent; no absolute paths, no `../`, no runtime network.
- `reference_image` is metadata-only (reference, never a texture — ADR-0002); lazy thumbnails are M5.
- `tinyobjloader` parses OBJ geometry **and imported MTL material groups** into `SubMesh`
  boundaries (`Model = vector<SubMesh>`, ADR-0006). MTL diffuse texture **references** are
  recorded on the Material; resolving them to GL textures is 0009's job.
- Axis correction baked once into the immutable CPU mesh; `recorded_winding` diagnostic only.
- Canonical parser behavior (ADR-0006): missing normals → area-weighted + diagnostic counter;
  missing UVs → canonical `(0,0)` + warning. Always upload through `renderer::Mesh` from 0005 —
  do not introduce a parallel asset/GPU mesh type.
- Deduplication by canonical geometry path → shared immutable Model.
- Loader result tri-state: `SUCCESS` / `LOADED_WITH_WARNINGS` / `FAILED`.
- `FAILED` → the asset is marked **unavailable** and a diagnostic is recorded; scene/entity
  consumers must not render it. **No implicit cube substitution** (ADR-0001 — never
  `if (load_failed) useCube()`); fallback only via explicit `fallback_geometry?` opt-in
  (unset for all MVP assets; no fallback system test in M2).
- Reload explicit (`AssetRegistry::reload()`); no filesystem watcher.
- Headless tests in `tests/`: manifest parse, invariant rejection, canonical-path dedup, tri-state
  results, reload (ADR-0008 test contract).

**Blocked by:** 0007

**Status:** ready-for-agent

- [x] Registry loads `assets/manifest.json` (nlohmann/json) as a read-only view
- [x] Entry schema + invariant enforced; invalid manifest rejected with diagnostic
- [x] OBJ + MTL parsed (tinyobjloader) into CPU canonical data → `SubMesh` boundaries; MTL
      diffuse references recorded on Material (`diffuseTextureRef`); GL resolution = 0009
- [x] CPU canonical mesh uploaded through `renderer::Mesh` (0005); no parallel GPU mesh type
- [x] Missing normals computed (area-weighted) + counter; missing UVs defaulted `(0,0)` + warning
- [x] Axis correction baked once; `recorded_winding` diagnostics-only
- [x] Dedup by canonical geometry path → shared immutable Model (CPU parse shared once,
      headless-observable via `uniqueGeometryCount()`)
- [x] Tri-state result; `FAILED` → asset unavailable + diagnostic, no throw in render loop,
      no implicit cube; explicit `fallback_geometry?` honored (unset for all MVP assets)
- [x] Explicit `reload()`; no watcher
- [x] Headless tests in `tests/` for all of the above

> Note (updated 2026-09, ticket 0012): the `Full`-mode GPU upload path is now exercised at the
> app level — 0012's registry integration wires `AssetRegistry::load()` in `noto3d-editor`
> (`--asset=<id>` renders manifest models through the Full path). Headless tests remain CPU-only
> by design (ADR-0008: renderer paths verified at the app level).

**Points to ADR:** 0007 (registry/loader contract), 0001 (no implicit cube), 0002 (reference_image
metadata-only), 0006 (Model/SubMesh/Material)