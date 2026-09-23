# ADR-0002 — Split "look" from "geometry": official PNG as reference, placeholder OBJ as geometry

Status: Accepted

## Context
No official Noto 3D mesh exists yet (see ADR-0001). The MVP vertical slice must not be gated on
an artifact Google has not published ("Unicode 18"). Official 3D PNGs (512px) are obtainable today
under SIL OFL 1.1.

## Decision
- Official Noto 3D PNGs are **reference images** (visual ground truth), not textures.
- Placeholder OBJs are the **geometry driver** for the MVP pipeline.
- Official mesh status recorded as `PENDING` (watch item); migration is a `geometry_source`
  placeholder → official swap in the asset registry, not a scene schema change.
- Curated set = 5–8 official PNG references shared across a minimal set of placeholder geometries
  (base_face, accessory, generic) — NOT one placeholder per emoji.
- A reference image is never assumed to be a valid UV texture of placeholder geometry.

## Consequences
- The vertical slice is deliverable today: a registry-defined Noto-3D-inspired asset through the
  full pipeline into the viewport.
- Scene stores only stable asset IDs; mesh arrival (placeholder→official) is invisible to Scene.