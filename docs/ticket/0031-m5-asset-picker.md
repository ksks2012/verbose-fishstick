# 0031 — M5 asset picker: assign a renderable asset to an entity

**Source:** ADR-0011 M5 spec (Q1 / Q7 / Q8); fourth M5 slice, parallel to 0030.
**Status:** ready-for-agent
**Blocked by:** 0029

Delivers "selected something → I can change what it is". Independent of 0030 (transform editing):
this slice is about *which asset*, not *where it sits*.

## What it delivers

- An Inspector control listing the assets the picker offers, so the user can change the selected
  entity's asset from the UI.
- The list contains **only assets that can actually be drawn**. Geometry-less / reference-only
  entries are hidden, and the hiding is **observable** in the structured log.
- Choosing an entry reassigns the entity's `assetId` and the 3D view shows the new asset
  immediately.
- The reassignment is logged.

## Contracts this slice must honour

- **The picker consumes a value snapshot**, built by the app layer from the registry's asset list.
  **The UI must not hold a `std::span` (or any other view) into registry storage** — the registry's
  accessors return a span into internal storage that a reload invalidates, so a UI-held view would
  be a dangling view across a reload (Q7).
- **No cache, no generation counter, no version token.** The list is rebuilt from the registry when
  the picker needs it (Q7).
- **The snapshot carries typed values** (id + typed geometry source), not pre-formatted display
  strings; turning them into user-facing text is presentation work in the UI adapter — the same
  principle that fixes unit conversion there (Q7, Q13).
- **Renderable-only** is the selection rule; hidden counts are reported rather than silently
  dropped (Q7).
- **The only effect on the Scene is `EntityId`-keyed mutation of `assetId`.** M5 introduces **no new
  ownership** of `Model`, `Mesh`, `Texture`, or any GPU resource, and does not resolve the
  AssetRegistry's header/type coupling — that is 0027, an M6 gate (Q1).
- Assignment goes through the **`Editor` facade**, keeping the single `collectDrawItems` resolution
  seam and the borrowed frame-local `DrawItem::model` contract untouched (Q11, ADR-0010 Q5).
- An asset id that cannot be resolved degrades to the existing **skip, not failure** behaviour; the
  picker never turns a bad assignment into a crash or a silent empty list (ADR-0010 Q5).
- **No undo**: a reassignment is immediate and not reversible (Q4).

## Acceptance

- [ ] A control in the Inspector lists selectable assets for the selected entity, and the current
      asset is identifiable
- [ ] Every offered entry is drawable; geometry-less / reference-only entries are absent, and the
      counts are logged (e.g. `n=<renderable> hidden=<reference-only>`)
- [ ] Choosing an entry changes the entity's `assetId`; the 3D view shows the new asset on the next
      frame, and the log records the assignment
- [ ] Two entities on the same asset remain two independent entities; changing one does not affect
      the other
- [ ] An entity whose asset cannot be resolved is skipped in the frame and the run continues, with
      the existing diagnostic
- [ ] Headless suite covers snapshot construction from the registry, renderable-only filtering
      (including a manifest that has reference-only entries), and that the effect on the Scene is
      limited to `assetId`
- [ ] A code review confirms the UI holds no span, pointer, or view into registry storage, and that
      M5 added no new `Model`/`Mesh`/`Texture`/GPU-resource ownership
- [ ] The snapshot carries typed values, with display text produced in the UI adapter
- [ ] M4 contracts unchanged: CLI exit-code matrix, `M4_INFO` lines, `xprop` titles, `--list-assets`
      output
- [ ] Manual evidence recorded: picker contents, a reassignment, and the visible 3D change

**Points to ADR:** 0011 (Q7 value-snapshot picker, Q1 no new GPU-resource ownership, Q8
observability) · 0010 (Q5 resolution seam, unresolved → skip, borrowed `DrawItem::model`) · 0007
(AssetRegistry as sole authority, manifest invariants)
