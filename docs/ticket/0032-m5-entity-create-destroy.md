# 0032 — M5 create and destroy entities

**Source:** ADR-0011 M5 spec (Q3 / Q4 / Q11); fifth M5 slice.
**Status:** ready-for-agent
**Blocked by:** 0031

Completes the editing surface: the user can not only change the entities they have, but add and
remove them. Blocked by 0031 because **a new entity needs a legitimate renderable `assetId`**, and
the picker is the single source of that choice — a "default to the first renderable" rule would
introduce hidden logic (who decides first, is the order stable, what if the list is empty) that
the picker would later have to replace anyway.

## What it delivers

- The user can **create an entity** in the Hierarchy, choosing its asset from the same choice the
  picker offers. The new entity appears in the list and in the 3D view immediately.
- The user can **destroy the selected entity**; it disappears from the list and from the 3D view
  immediately.
- Both operations are logged, and the runtime keeps reporting an accurate entity count.

## Contracts this slice must honour

- Create and destroy go through **`Editor` semantic operations keyed by `EntityId`**; the UI never
  touches Scene storage (Q3, Q11).
- **`EntityId` stays monotonic and is never reused.** A destroyed id is never handed out again,
  and destroying all entities does not reset the counter — `clear()` keeps the allocation state
  (ADR-0010 Q2 / Q4). Ids remain the **single identity** (runtime = persistent) that M6 persists.
- **There is no undo.** Destroy is **not** reversible, and it is explicitly *not* implemented as
  destroy+recreate: that would be a destructive fake undo which hands out a new id and silently
  breaks M6's identity contract. Recovery from a mistaken destroy is a new entity (Q4).
- **No `Scene` revision counter is introduced** just to support the UI; nothing in the locked
  contracts needs one (Q6).
- **No prefab, scene graph, or entity-type concept** creeps in — a created entity is the same flat
  world-space entity shape M4 defined (ADR-0010 Q3, M4 exclusions continue).
- The single `collectDrawItems` resolution seam and the borrowed frame-local `DrawItem::model`
  contract are unchanged (Q11, ADR-0010 Q5).
- Creation uses an asset that the picker can offer, so the new entity is drawable immediately; it
  never starts life as an unresolvable entity (Q7).

## Acceptance

- [ ] Creating an entity adds a row in the Hierarchy, in creation order, drawable in the 3D view on
      the next frame, and the runtime's reported entity count increases by one
- [ ] A new entity's asset comes from the picker's choices, and a created entity is never
      unresolvable by construction
- [ ] Destroying the selected entity removes it from the Hierarchy and the 3D view immediately, and
      the reported count decreases by one
- [ ] Destroying all entities, then creating again, yields **new, never-before-used ids**; the id
      counter is not reset by emptying the Scene
- [ ] Destroying when nothing is selected, or destroying an already-destroyed id, is a safe no-op —
      no crash, no error state
- [ ] Create and destroy are both logged; the log distinguishes the new id and the destroyed id
- [ ] A manual check confirms there is **no undo** affordance and that a mistaken destroy is
      recovered by creating a new entity, with a new id
- [ ] Headless suite covers create/destroy through the facade, id monotonicity across a
      destroy/create cycle, the empty-Scene counter behaviour, and no-op destroy cases
- [ ] `Scene` and `modelMatrix` unchanged; no new entity fields or types introduced
- [ ] M4 contracts unchanged: CLI exit-code matrix, `M4_INFO` lines, `xprop` titles
- [ ] Manual evidence recorded: create, destroy, and the resulting list and 3D view

**Points to ADR:** 0011 (Q3/Q11 the `EntityId` write path through the facade, Q4 no undo and no
destructive fake undo, Q6 no revision counter, Q7 the asset choice source) · 0010 (Q2 `EntityId`
never reused, Q4 `clear()` keeps the counter and the M6 snapshot-replace forward contract, Q5 single
resolution seam)
