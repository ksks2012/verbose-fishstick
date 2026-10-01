# 0047 — Manual verification tier: the M5 and M6 boxes that need a real window session

**Source:** open items recorded by the close tickets of M5 and M6 (0033 and 0046).
**Status:** ready-for-agent (needs hardware; no code change expected)
**Blocked by:** — (0033 and 0046 are both complete and archived)
**Related:** `docs/archive/ticket/0033-m5-close-integration-regression.md`,
`docs/archive/ticket/0046-m6-close-integration-regression.md`, ADR-0010 Q8 / ADR-0011 Q9
(the two-tier verification policy both closes rest on)

## What it delivers

Every **manual** verification box left open by the M5 and M6 closes, discharged on a machine that
has a display. The automated tier of both milestones is complete and green; nothing here is a
missing test, and **no code change is expected**. This ticket exists so the outstanding work is
tracked rather than living only in the prose of two archived tickets, where it is easy to mistake for
"done, the docs just weren't updated".

The two-tier policy (ADR-0010 Q8) is explicit that a close is not complete until the platform-
specific evidence is recorded, or until the environment limitation is recorded **honestly**. 0033 and
0046 both chose the latter: this environment has no window session, so the boxes below were left
unclaimed rather than ticked.

## Why this is a ticket and not a note

A manual box that nobody owns does not get done. 0033 recorded four such boxes in September and
they were still open when M6 closed. The specific failure mode this ticket prevents: a later reader
seeing "M5 close: automated matrix ALL PASS" and inferring the milestone is fully verified, when a
real window was never opened. The status table in `README.md` and the M6 status line in
`docs/CONTEXT.md` both point here so that inference is harder to make.

## Scope — the M5 boxes (from 0033)

1. **Docking move/resize** — drag the dock splitter, resize the window, confirm the viewport aspect
   stays correct for a deliberately non-square viewport and that `glViewport`/scissor track the
   `ViewportRect` rather than the window.
2. **The full user edit pass** — select an entity in the Hierarchy, change its transform in the
   Inspector, swap its asset, create an entity, delete it. Confirm each `M5_*` line matches what the
   user did (not just what the headless suite logs).
3. **Interaction-ownership checks** — click inside vs outside the viewport with panels present, and
   over an open combo. Confirm the camera moves only when `viewportHovered && !IsAnyItemHovered() &&
   !IsAnyItemActive() && !globalModalCapture` holds, and that the wheel is gated the same way.
4. **No-gizmo / no-undo confirmation** — the M5 exclusions still hold in the running app: no
   manipulator gizmo, no undo/redo.

## Scope — the M6 boxes (from 0046)

5. **Open a saved document in the real editor.** Save a scene via `--save-scene=`, then load it
   with `--scene=` and confirm **visually** that the entities and their transforms are what was
   saved. The automated tier already proves this structurally (byte-identity re-save, non-identity
   transform assertions), so this box is about the human-visible result, not about new doubt.
6. **`xprop` `WM_NAME` for a `--scene=` run.** Record the three titles — the default scene, a
   `--scene=` run, and `--demo` — and confirm they are unchanged and that the milestone tag is still
   frozen at `(M4)`. This is the M4/M5 title contract 0046 promised to leave alone; the automated
   tier asserts the tag as a static contract and the `M4_INFO` rows unchanged, but the actual
   `WM_NAME` string is a platform observation.

## How to discharge it

Requires a Linux display (X11; GLFW 3.4). For each box: record what was done, what was observed, and
the command used. `scripts/m5-close-matrix.sh` and `scripts/m6-close-matrix.sh` should be run in the
same session as evidence that the automated tier still holds on that machine — the automated tier
being green in CI does not establish anything about a specific display.

A box that cannot be performed should be recorded as **not performed, with the reason** — not ticked
on the strength of the automated evidence. The point of the manual tier is precisely that some
things cannot be established without looking.

## Explicitly NOT in scope

- Fixing anything the manual pass finds. If a box fails, that is a **new** finding: file it as a
  defect ticket and keep this ticket about verification only. A close ticket that also repairs is a
  close ticket whose evidence cannot be trusted.
- Screenshot / readback / image-similarity comparison (the M3 exclusion continues).
- Any change to the automated matrix or the log formats. Both are frozen and green.

## Verification

- Each of the six boxes has a recorded observation, or a recorded reason it was not performed.
- `xprop` titles for the three modes recorded verbatim, with the milestone tag.
- Both close matrices green in the same session.
- On completion: update the M5 and M6 status lines in `README.md` / `docs/CONTEXT.md`, and append a
  closing note to this ticket (per DOCUMENTATION-LIFECYCLE, append — never edit an ADR to record
  that a box was ticked).

**Rolls under spec:** ADR-0010 Q8 and ADR-0011 Q9 (the two-tier verification policy both closes
rest on). Does not block any new milestone work; it blocks the claim that M5 and M6 are *fully*
verified rather than *machine-verified*.
