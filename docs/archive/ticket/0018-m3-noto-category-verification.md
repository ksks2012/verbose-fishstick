# 0018 — M3 Noto category verification

**What to build:**

A **one-shot milestone evidence** record, not a runtime feature and not a per-build regression:
for each of the 6 frozen M3 assets, a human reviewer renders `--asset=<id>`, opens the
corresponding official reference PNG, and confirms the placeholder asset still represents the
Noto category the manifest declares.

It verifies **category/identity consistency only**. It explicitly does NOT evaluate pixel
similarity, geometry similarity, pose/color accuracy, or visual quality (placeholder geometry is
not official geometry — numeric comparison has no engineering value, ADR-0001/0002).

Roles: 0015 = baseline fence · 0016 = fixture freeze · 0017 = runtime observability ·
0018 = human category validation.

## Scope

- Product: the 6-row manual verification matrix template below, filled in by a human.
- Tools at hand: `--asset=<id>` (identity + reference path printed by 0017), `--list-assets`,
  the official PNG files under `assets/references/noto3d/`.

## Verification matrix template

| Asset | Placeholder geometry | Reference | Category match | Notes |
| ----- | -------------------- | --------- | -------------- | ----- |
| `smile`      | `base_face.obj`   | u1f600_512.png | ☐ | |
| `warm`       | `base_face.obj`   | u1f60a_512.png | ☐ | |
| `cool`       | `base_face.obj`   | u1f60e_512.png | ☐ | |
| `crown`      | `accessory.obj`   | u1f451_512.png | ☐ | |
| `top_hat`    | `accessory.obj`   | u1f3a9_512.png | ☐ | |
| `sunglasses` | `generic.obj`     | u1f576_512.png | ☐ | |

Where a category needs a note (e.g. geometry too abstract to read at a glance), record it in
Notes — a "no" defers the asset flag for the official-mesh swap and notes what M3 revealed.

**Explicitly excluded (locked):** no runtime reference-image loading; no screenshot/readback
pipeline; no image-similarity algorithm; no `ReferencePreview` GPU quad (that is M5 UI territory);
the result is a milestone record, not a repeatable automated check.

## Acceptance

- [ ] 6-row matrix filled and committed with PASS/NOTES per frozen asset
- [ ] Confirms "6 frozen Noto asset IDs go manifest → placeholder geometry → renderer, identity
     /reference relationship human-verified" (the M3 architectural outcome)
- [ ] Any category failure recorded with note; no code/pipeline changes introduced by this ticket

**Blocked by:** 0017
**Status:** open
**Rolls under spec:** ADR-0009 (M3 spec embedded; execution tickets only in docs/ticket/)
**Points to ADR:** 0001/0002 (placeholder = geometry driver, reference = ground truth, category
verification ≠ similarity), 0009 (M3 decisions)