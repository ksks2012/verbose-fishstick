# ADR-0001 — Asset Spike must precede any renderer work

Status: Accepted

## Context
The MVP plan assumed Noto 3D emoji exist as OBJ+PNG and scheduled asset loading for M2.
Investigation (Sep 2026):
- The official `googlefonts/noto-emoji` repo contains **zero mesh files** on any branch
  (`main`, `emoji18`, `3dpng`, `emoji_u1f44f_SourceFile`): only `3D/fonts` (TTF) and `3D/png` (renders ≤512px).
- Issue #549 ("Where are the .OBJ files?") is closed, maintainer reply: "It's coming with Unicode 18."
- License of shipped 3D material: SIL OFL 1.1.

## Decision
- Move the asset spike to Day 0 (before any Renderer/ModelLoader work).
- Instrumenting asset availability before writing loaders.
- Required asset count for MVP: 1 real Noto 3D asset; target curated set 5–8.
- Full ~3,977 library is out of scope.
- Fallback (cube) is a permanent infrastructure test asset; asset availability must never block
  renderer development. The cube is an **explicit test asset** only — it is NOT an implicit
  runtime substitution mechanism for failed asset loading (see ADR-0007: no implicit fallback;
  engine must never `if (load_failed) useCube()`).

## Consequences
- Risk of "Day 3: assets don't exist" is eliminated.
- M0.5 produces an Asset Contract that is the formal input to M2.