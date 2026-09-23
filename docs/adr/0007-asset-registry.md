# ADR-0007 — Asset registry / loader contract

Status: Accepted

## Context
Scene must depend only on stable asset IDs — not on OBJ/PNG/GPU resources — so the placeholder-then-
official mesh migration (ADR-0002) stays a registry-level change. Diagnostics quality must not be
sacrificed for demo resilience.

## Decision
- `assets/manifest.json` (v1) is the single source of truth; the runtime `AssetRegistry` is a
  read-only view loaded at startup (nlohmann/json).
- Entry: `id` (stable ASCII slug), `reference_image?`, `geometry?`, `geometry_source`.
- Invariant: `geometry_source` is `placeholder|official` iff `geometry != null`, else null.
- `reference_image` is metadata-only (reference, not texture); lazy thumbnails are an M5 concern.
- Geometry-less assets are not spawnable; Scene may still persist the ID.
- Paths are relative to the manifest parent; no absolute paths, no `../`, no runtime network.
- Axis correction is baked once into the immutable CPU mesh; `recorded_winding` is diagnostic only.
- Deduplication by canonical geometry path (shared immutable Model).
- Loader result: `SUCCESS` / `LOADED_WITH_WARNINGS` (recovery counters) / `FAILED`.
- `FAILED` → entity disabled + diagnostic; never throw in the render loop; no implicit placeholder
  substitution. Fallback only via explicit `fallback_geometry?` opt-in in the manifest.
- Reload is explicit (`AssetRegistry::reload()`); no filesystem watcher.

## Consequences
- Scene JSON contains asset IDs, never GPU identities.
- Official-mesh arrival = manifest edit + reload, not scene migration.