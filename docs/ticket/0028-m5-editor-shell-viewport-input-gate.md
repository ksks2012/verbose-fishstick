# 0028 — M5 editor shell: docked layout, viewport rect, input-ownership gate

**Source:** ADR-0011 M5 spec (Q0 / Q6 / Q9 / Q10); first vertical slice of M5.
**Status:** ready-for-agent
**Blocked by:** None — can start immediately (frontier root)

The first M5 slice. It deliberately adds **no editing feature**. It establishes *where the view
is* and *who owns the mouse*, which is the prerequisite every later M5 ticket attaches to — and
0026 waits on exactly this.

## What it delivers

- A dock host in which the 3D view is a real dockable panel, beside **Hierarchy** and
  **Inspector** panels that exist but are empty.
- The 3D view renders correctly at whatever shape its panel is: projection aspect comes from the
  panel's rect, and the GL viewport and scissor are confined to the 3D panel, so the 3D pass' clear
  does not wipe the rest of the window.
- **One owner per gesture.** Mouse drags and the wheel reach the camera only while the cursor is
  over the 3D panel and no ImGui item is hovered or being dragged. Hovering, clicking, or scrolling
  any panel leaves the camera untouched.
- The camera interaction that already works keeps working, now correctly scoped to the 3D panel.

## Contracts this slice must honour

- The frame's view extent is a single explicit **`ViewportRect`** (framebuffer pixels, plain data,
  no ImGui types), produced per frame by the UI adapter and consumed by everything else. **No path
  may treat the framebuffer size as the view** (Q0).
- The Y-flip from ImGui's top-left screen space into the GL lower-left form is applied **in the UI
  adapter**; the app layer consumes a GL-space rect and never re-derives the flip (Q0).
- Input ownership is `viewportHovered && !IsAnyItemHovered() && !IsAnyItemActive() &&
  !globalModalCapture`, and the **wheel obeys the same gate**, with the 3D panel created with
  `NoScrollbar | NoScrollWithMouse` (Q10).
  **`!io.WantCaptureMouse` must not be used as the viewport camera gate** — the 3D panel is itself
  an ImGui window, so that spelling makes camera input permanently dead. `WantCaptureMouse` is
  reserved for genuinely global modal/popup capture.
- The gate is a **pure predicate over those four inputs**, with the ImGui queries read at the
  adapter edge, precisely so it is testable without ImGui (Q10, Testing Decisions).
- A scroll delta denied by the gate is **discarded**, never queued for a later frame.
- The camera and that view's rect live together in a per-view **`ViewportState`**; input deltas and
  baseline bookkeeping live in **`InputState`**, separate from both (Q6). The camera is no longer a
  render-scope local.
- **No `Scene` ownership change in this slice.** The Scene is still constructed and rendered exactly
  as at M4, and rendering still goes through the single `collectDrawItems` seam. The `Editor`
  facade is 0029's job.

## Making the change possible

The frame / viewport / input maths currently lives inside the render scope in the app entry point.
This slice moves the parts that carry contracts (rect, aspect, containment, the gate predicate)
into the app layer so they are headless-testable — which is what makes the slice deliverable as
evidence rather than as a screenshot. That extraction belongs to **this** slice, not to a separate
horizontal prefactor ticket.

## Acceptance

- [ ] Dock host with the 3D view as a real panel; Hierarchy and Inspector panels present but empty
- [ ] 3D view renders at its panel's aspect with no stretching; a deliberately non-square panel
      still looks correct
- [ ] GL viewport **and** scissor confine rendering *and* the clear to the 3D panel; regions
      outside the 3D view are not cleared by the 3D pass
- [ ] Cursor over the 3D panel, no item hovered, no item active, no modal capture ⇒ camera may
      consume mouse drags and wheel
- [ ] Cursor over any panel, over any hovered item, during any widget drag, or under a modal
      capture ⇒ camera consumes nothing
- [ ] Scrolling a panel does not zoom the camera; denied scroll deltas are discarded, not deferred
- [ ] The gate is a pure function of its four inputs in the app layer, headlessly covering both
      polarities of all four
- [ ] `ViewportRect` maths (GL-space rect, aspect, cursor containment) headlessly covered,
      including a non-square rect and cursor positions outside the rect
- [ ] The app layer still links neither ImGui nor GLFW; the new suites are headless
- [ ] M4 contracts unchanged: CLI exit-code matrix, `M4_INFO` lines, and `xprop` window titles
      behave exactly as at M4 close
- [ ] Manual evidence recorded: docking behaviour, panel layout, and confirmation that panel
      interaction does not move the camera
- [ ] No readback, screenshot comparison, or image similarity introduced

**Points to ADR:** 0011 (Q0 viewport/coordinate contract, Q6 state lifetimes, Q9/Q10 input
ownership) · 0004 amendment (viewport ≠ framebuffer, aspect from the rect) · 0005 amendment
(app establishes `glViewport`/scissor; wheel ownership) · 0010 (camera purity; single resolution
seam preserved)
