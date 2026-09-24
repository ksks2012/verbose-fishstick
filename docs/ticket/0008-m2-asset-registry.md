# 0008 — M2 AssetRegistry runtime + OBJ→Mesh pipeline (ADR-0007)

**What to build:**

Runtime half of ADR-0007 (the manifest data contract exists from M0.5). Load
`assets/manifest.json` at startup via nlohmann/json into a read-only `noto3d::asset::AssetRegistry`,
resolve stable IDs to immutable Models, and give diagnostics per ADR-0007 without ever throwing in
the render loop. Pull tinyobjloader now (ADR-0003 defers it to M2) to parse placeholder OBJs.

Contract (ADR-0007):
- Registry is a read-only view of the manifest; entry = `id` (stable ASCII slug),
  `reference_image?`, `geometry?`, `geometry_source`.
- Invariant: `geometry_source ∈ {placeholder, official}` iff `geometry != null`, else null — invalid
  manifests are rejected with a diagnostic.
- Paths relative to the manifest parent; no absolute paths, no `../`, no runtime network.
- `reference_image` is metadata-only (reference, never a texture — ADR-0002); lazy thumbnails are M5.
- Axis correction baked once into the immutable CPU mesh (off-axis assets handled here, not in the
  shader); `recorded_winding` is diagnostic only.
- Deduplication by canonical geometry path → shared immutable Model.
- Loader result tri-state: `SUCCESS` / `LOADED_WITH_WARNINGS` (counters: missing normals →
  area-weighted + diagnostic; missing UVs → canonical `(0,0)` + warning) / `FAILED`.
- `FAILED` → entity disabled + diagnostic; never throw in the render loop; **no implicit cube
  substitution** (ADR-0001 — never `if (load_failed) useCube()`); fallback only via explicit
  `fallback_geometry?` opt-in (unset for all MVP assets).
- Reload explicit (`AssetRegistry::reload()`); no filesystem watcher.
- Headless tests in `tests/`: manifest parse, invariant rejection, canonical-path dedup, tri-state
  results, reload (ADR-0008 test contract).

**Blocked by:** 0006

**Status:** ready-for-agent

- [ ] `noto3d::asset::AssetRegistry` loads `assets/manifest.json` (nlohmann/json) as a read-only view
- [ ] Entry schema + invariant enforced (`geometry_source` pres/abs with `geometry`), invalid
      manifest rejected with diagnostic
- [ ] OBJ parsed via tinyobjloader → CPU mesh; missing normals computed (area-weighted) + counter;
      missing UVs defaulted to `(0,0)` + warning
- [ ] Axis correction baked once into the immutable CPU mesh; `recorded_winding` diagnostics-only
- [ ] Dedup by canonical geometry path → shared immutable Model
- [ ] Tri-state result `SUCCESS`/`LOADED_WITH_WARNINGS`/`FAILED`; `FAILED` → disable + diagnostic,
      no throw in render loop, no implicit cube; explicit `fallback_geometry?` honored (unset now)
- [ ] Explicit `reload()`; no watcher
- [ ] Headless tests in `tests/` for all of the above

**Points to ADR:** 0007 (registry/loader contract), 0001 (no implicit cube), 0002 (reference_image
metadata-only)