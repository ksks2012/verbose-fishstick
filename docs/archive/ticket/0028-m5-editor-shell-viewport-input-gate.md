# 0028 — M5 editor shell: docked layout, viewport rect, input-ownership gate

**Source:** ADR-0011 M5 spec (Q0 / Q6 / Q9 / Q10); first vertical slice of M5.
**Status:** implemented — machine-verified; manual evidence pending
**Blocked by:** None (frontier root)

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

### Resulting layout

`main.cpp` went 611 → 256 lines by splitting the entry point along the layering ADR-0008 and
ADR-0011 already defined, and **headers are now declaration-only** (every definition lives in a
`.cpp`):

- `src/app/` (`noto3d_app`, links neither ImGui nor GLFW) — `viewport_rect.{hpp,cpp}`,
  `input_gate.{hpp,cpp}`, `camera_input.{hpp,cpp}` (gesture math, previously an anonymous-namespace
  blob in `main.cpp`), `viewport_state.hpp`.
- `src/ui/` (`noto3d_ui`, **new target**; presentation and input adaptation only, per ADR-0011) —
  `imgui_harness.{hpp,cpp}` (context + backend RAII), `dock_layout.{hpp,cpp}`,
  `editor_shell.{hpp,cpp}` (panels + ownership inputs; carries the 0034 ② ordering contract),
  `panel_render.{hpp,cpp}` (scissor/viewport scope + the `collectDrawItems` draw),
  `window_input.{hpp,cpp}`.
- `src/app/main.cpp` — GLFW lifecycle, the GL scope (ticket 0025's rule), and the frame loop only.
- `src/app/editor_entry.{hpp,cpp}` — CLI→Scene entry model + `--list-assets`. Built into the
  **executable**, not `noto3d_app`: it needs `noto3d_asset`, and adding that edge to the app library
  is a target-graph change owned by 0027 / 0031, not this slice.
- `noto3d-viewport-rect-tests` / `noto3d-input-gate-tests` now compile their TU directly and link
  `noto3d_core` only (the `noto3d-cli-options-tests` pattern), so the headless guarantee is
  structural rather than incidental.

The gate-read ordering from 0034 ② is now stated in `submitEditorPanels()`'s contract, next to the
code that depends on it, rather than only in a comment inside the frame loop.

## Acceptance

- [x] Dock host with the 3D view as a real panel; Hierarchy and Inspector panels present but empty
- [x] 3D view renders at its panel's aspect with no stretching; a deliberately non-square panel
      still looks correct
- [x] GL viewport **and** scissor confine rendering *and* the clear to the 3D panel; regions
      outside the 3D view are not cleared by the 3D pass
- [x] Cursor over the 3D panel, no item hovered, no item active, no modal capture ⇒ camera may
      consume mouse drags and wheel
- [x] Cursor over any panel, over any hovered item, during any widget drag, or under a modal
      capture ⇒ camera consumes nothing
- [x] Scrolling a panel does not zoom the camera; denied scroll deltas are discarded, not deferred
- [x] The gate is a pure function of its four inputs in the app layer, headlessly covering both
      polarities of all four
- [x] `ViewportRect` maths (GL-space rect, aspect, cursor containment) headlessly covered,
      including a non-square rect and cursor positions outside the rect
- [x] The app layer still links neither ImGui nor GLFW; the new suites are headless
- [ ] M4 contracts unchanged: CLI exit-code matrix, `M4_INFO` lines, and `xprop` window titles
      behave exactly as at M4 close
- [ ] Manual evidence recorded: docking behaviour, panel layout, and confirmation that panel
      interaction does not move the camera
- [x] No readback, screenshot comparison, or image similarity introduced

**Points to ADR:** 0011 (Q0 viewport/coordinate contract, Q6 state lifetimes, Q9/Q10 input
ownership) · 0004 amendment (viewport ≠ framebuffer, aspect from the rect) · 0005 amendment
(app establishes `glViewport`/scissor; wheel ownership) · 0010 (camera purity; single resolution
seam preserved)

## Verification (recorded 2026-09-28)

**Machine-verified** — dev + release build 0 error / 0 warning; `ctest` **18/18** in both.
`app-viewport-rect-math` (37 checks) and `app-input-gate-contract` (17 checks, full 16-combination
sweep of the four gate inputs) are headless and link `noto3d_core` only. The HiDPI fix adds
`toFramebufferRect()` plus `DisplayTransform` regression cases: non-zero host viewport origin,
anisotropic scale, `lround` rounding, and a minimised/zero-height framebuffer.

> **Correction (2026-09-28).** An earlier version of this section claimed "All headers are
> declaration-only — every definition lives in a `.cpp`". **That was false.** It was derived from a
> grep matching only `inline` and `constexpr`, which cannot see a member function defined inside a
> class body — those are *implicitly* inline. A definition-shaped scan finds eight accessors still
> defined in headers, one of them (`PanelViewport::active()`) introduced by this ticket's own
> extraction. The rule (ADR-0008) is unchanged; the code is what was wrong. Tracked and fixed by
> **0035**.

**Still open** — the two boxes above: `xprop` window titles and the manual evidence (docking
behaviour, panel layout, and that panel interaction does not move the camera). Both need a display
and a real pointer.

## Known deviation from ADR-0011 Q5 — unresolved

This ticket's implementation split `src/ui/` into a **new `noto3d_ui` static library target**, but
ADR-0011 Q5 states that `src/ui/` sources "compile into the existing `noto3d-editor` executable" and
that a separate library is **rejected** for M5, on the stated grounds that the adapter is untestable
headlessly either way, so a new target "buys no verification and adds a speculative boundary".
The revisit condition ("only if a second host needs to link the editor") is **not** met — there is
one host. On the ADR's own reasoning the target delivers no verification benefit.

`noto3d_ui` is retained in `CMakeLists.txt` pending an explicit decision, and this deviation is
recorded rather than papered over. Resolution options: fold the five `src/ui/` sources back into
`noto3d-editor` (restores the locked decision, no behaviour change), or append an ADR-0011
amendment per DOCUMENTATION-LIFECYCLE if the separation is judged worth keeping.
