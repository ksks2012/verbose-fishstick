# ADR-0011 — M5 editor architecture: ImGui editor, viewport rect, input ownership

Status: Accepted

> Per DOCUMENTATION-LIFECYCLE: **specs (PRDs) live at the ADR layer**; `docs/ticket/` holds
> execution tickets derived from accepted ADRs. This ADR embeds the M5 milestone spec (Problem /
> Solution / User Stories / Testing / Out of Scope) below; the M5 execution tickets will roll
> under it. M3's spec is embedded in **ADR-0009**, M4's in **ADR-0010**.

## Context

M4 (0020–0025) is closed: a pure-data `Scene` of entities with stable `EntityId`s, one
app-layer resolution seam (`app::collectDrawItems`), a CLI→Scene entry model, and a pure-math
`renderer::Camera`. What does not exist is any way to *edit* that Scene with a mouse, or any
arrangement of a 3D view alongside UI panels.

M5 introduces the first UI: a docked ImGui editor with a 3D viewport plus Hierarchy and Inspector
panels. Everything is already fetched and compiled — ImGui is pinned to `v1.92.9-docking`
(ADR-0003) and linked into the `noto3d-editor` executable — but **no ImGui source has been added
to the project yet**, and the current runtime is a single full-window 3D view with camera input
that is completely unconditional.

Introducing a UI next to that runtime is not merely "draw some widgets". It raises a chain of
load-bearing questions, each of which would otherwise be re-litigated by the first ticket that
touches it: where the 3D viewport is (and what its coordinate space is), who owns input when
ImGui and the camera both want the mouse, what the UI is allowed to hold, what the editor mutates
and how, whether any of it is headless-testable, and what M5 explicitly does *not* attempt.

The following decisions were pressure-tested in a grilling session and are locked here so that no
M5 ticket re-opens them and so that M6 builds on a settled shape.

> **Decision identifiers** are the grilling session's, kept verbatim for traceability: **Q0, Q1,
> Q3–Q14**. The numbering is not a reordered sequence and has two gaps. `Q2` (selection state) was
> not a separate question — it was resolved as part of the state-lifetime decision (**Q6**). `Q0`
> and `Q9`/`Q10` were asked and answered after their neighbours because the input questions were
> unreachable until viewport geometry and slice scope were fixed. Nothing is missing from the set.

## Decision (locked 2026-09-28, grilling Q0–Q14)

### Q0 — Frame layout, viewport geometry & coordinate contract
- **Docked layout**: the 3D view is an ImGui window inside a dock space, alongside Hierarchy and
  Inspector. The editor is a docked workspace, not a floating panel over a full-window 3D view.
- **One explicit `ViewportRect`**, in **framebuffer pixels**, as **plain data** — no ImGui types,
  no `ImVec2`, no hidden coordinate space:
  ```cpp
  struct ViewportRect { int x, y, width, height; };  // origin = framebuffer lower-left
  ```
  It is produced by the UI adapter per frame and consumed by the app layer.
- **Coordinate space, and where the Y-flip lives.** The `ViewportRect` is the **GL-consumable**
  form: origin at the framebuffer's **lower-left**, matching `glViewport`. ImGui reports viewport
  rectangles in screen space with **Y growing downward from the top**, and GLFW cursor positions
  are likewise top-left / Y-down, so the conversion is
  `y_gl = framebufferHeight − (pos.y + size.y)`. **Producing the GL-space rect — i.e. applying
  that flip — is the UI adapter's job**; the app layer consumes a rect it can hand to `glViewport`
  unchanged and never re-derives it. Getting this wrong renders the scene upside down inside the
  panel, and the mistake is invisible until something is drawn, so it is named here rather than
  left to whichever ticket writes the adapter.
- **No whole-window debt.** Every M5 path that needs a 2D extent takes it from the
  `ViewportRect`. M4's implicit "the framebuffer size *is* the view" assumption is replaced, not
  paralleled by a second convention.
- **Projection aspect comes from the rect** (`width / height`), not from the full framebuffer.
  A non-square viewport must not stretch the render.
- **Cursor mapping is explicit and two-stage**: GLFW window coordinates → viewport-local
  coordinates, with a containment test as part of the mapping, not applied afterwards by
  accident. Consumers never subtract coordinates "by hand".
- **App owns `glViewport`**: the app layer establishes the viewport from the `ViewportRect`
  **before** the 3D pass (see ADR-0005 amendment). `Renderer::Begin3DPass()` stays UI-agnostic and
  takes no rect at M5.
- **The scissor box follows the viewport**, so the pass' clear is confined to the 3D rect instead
  of clearing the whole framebuffer. This is part of the same contract, not an optimisation
  (see ADR-0005 amendment).
- The ImGui OpenGL3 backend saves and restores viewport and scissor state, so per-frame
  re-establishment by the app is required and expected; nothing needs a manual save/restore
  handshake in app code.

### Q1 — Asset CPU/GPU boundary is not M5 scope; it is a gate before M6
- **0027 is not an M5 prerequisite and is not in the M5 critical path.** It is an architecture
  gate that must be closed *before M6 scope opens*.
- The premise recorded in 0027 is **corrected**: a CPU/GPU boundary **already exists** and is
  already enforced. `src/asset/` makes no direct GL calls; `loadObjShapes()`,
  `CanonicalShape` and `loadModelShapesOnly()` are CPU-only; `uploadToGpu()` is the single
  GPU-upload gate. What is left is **header/type coupling**, not a missing runtime boundary:
  `AssetRegistry`'s public API exposes `renderer::Model*` and caches a `Texture`.
- **M5 invariant (binding on every M5 ticket)**: M5 introduces **no new ownership** of `Model`,
  `Mesh`, `Texture`, or any GPU resource, and M5 does not resolve the coupling. Asset *editing* in
  M5 means **mutating a stable `assetId` string** and nothing else.
- 0027 stays open and un-implemented; it is re-scoped to M6-prework with the corrected premise.

### Q3 — Editor write path: `EntityId` in, frame-local re-resolution out
- Editor state holds **`EntityId` only**. It never holds an `Entity*`, a reference, or a
  `std::span<Entity>` across a frame boundary or across a structural mutation.
- Every frame re-resolves the id through the app layer (`findEntity`). A stale `Entity*` cannot
  be held because one is never retained.
- Mutations go through **thin app-layer operations** keyed by `EntityId`; the UI never writes
  entity fields through a pointer it obtained itself.
- `Scene`'s mutable accessors (`findEntity(EntityId) noexcept`, `entities() noexcept`) stay
  available to the app layer, but the UI has no route to them (see Q11).

### Q4 — No undo/redo at M5
- M5 ships **no undo/redo**: no command history, no command objects, no apply-on-release, no
  drag coalescing, no inverse-command bookkeeping.
- Editing is **immediate and per-frame**. The frame *is* the save point.
- Recovery from a bad value is **manual** — correct the field in the Inspector, or restart the
  session. This is a named M5 limitation, not a hidden debt.
- **No destructive undo.** A mutation that is "undone" by destroying and recreating the entity is
  forbidden: `EntityId`s are monotonic and never reused (ADR-0010 Q2), so destroy+create is not
  identity-preserving and would silently break M6's single-identity forward contract.
- Undo/redo remains a candidate for a later milestone once the editor has a state container
  worth snapshotting.

### Q5 — Editor logic is ImGui-free; the UI is an adapter
- Selection, edit operations, hierarchy enumeration, and viewport geometry are **ImGui-free**
  and headless-testable. They live in the app layer, behind ImGui-facing functions.
- The application library `noto3d_app` links **neither ImGui nor GLFW**. This already holds in the
  current target graph and must keep holding: it is the property that makes the editor logic
  testable by a headless CTest suite linked against `noto3d_app` alone.
- `src/ui/` is **presentation and input adaptation only** — ImGui widget calls, docking, and
  translation of widget state into editor operations. It holds no editor semantics.
- **`ViewportRect` is plain data**, so viewport geometry and the coordinate contract stay testable
  without ImGui.
- New editor-logic suites extend `tests/app/` and link `noto3d_app`.
- New sources in `src/ui/` compile into the existing `noto3d-editor` executable. A separate
  `noto3d_editor` library is **rejected** for M5: the adapter is untestable headlessly either way
  (it needs a display and a GL context), so a new target buys no verification and adds a
  speculative boundary. Revisit only if a second host needs to link the editor.

### Q6 — State lifetimes: `EditorState` / `ViewportState` / `InputState`
- **Three separate lifetimes, three separate owners:**
  - **`EditorState`** — session/editor-scoped: currently the selected `EntityId`. Lives as long as
    the editor session, independent of any frame, any viewport, and any render pass.
  - **`ViewportState`** — **per view**: the `Camera` plus the current `ViewportRect` for that view.
    M5 ships exactly **one** viewport, but the state shape is per-view so that a future
    multi-view layout is an instantiation, not a redesign.
  - **`InputState`** — separate from both: raw per-frame input deltas (cursor, button edges,
    accumulated scroll) and cursor-baseline bookkeeping.
- A `Camera` is therefore **not** a `renderScene()` local in the M5 design: it is owned by a
  `ViewportState`, which is the unit that a future split view would multiply.
- No new cross-view or global state registry. A `Scene` revision counter is **not** introduced at
  M5; nothing in the locked M5 contracts needs it, and M6 can add it where a real consumer exists.

### Q7 — Asset picker consumes a value snapshot
- The asset picker is fed a **snapshot of values** produced by the app layer from
  `AssetRegistry::entries()` — one flat list of choices the UI may freely keep and sort.
- The UI **must not** hold a `std::span<const AssetEntry>` or any other view into registry
  storage. `entries()` returns a span into internal storage that reload invalidates; a UI holding
  it would be holding a dangling view across a reload.
- **No cache, no generation counter, no version token.** The list is rebuilt from the registry
  when the picker needs it.
- **Only renderable assets are selectable** (geometry present). Geometry-less / reference-only
  entries are **hidden** from the picker, and the hiding is **observable** in the structured log:
  `M5_INFO asset-picker n=<renderable> hidden=<reference-only>`.
- The picker's effect on the Scene is **only** `Entity::assetId` mutation (Q1) — never a
  `Model*`, never a GPU object.
- `AssetChoice` carries **typed** values (id + typed geometry source), not pre-formatted
  display strings: converting to user-facing text is presentation work and belongs in `src/ui/`,
  by the same principle that fixes unit conversion there (Q13).

### Q8 — Structured runtime observability
- Editor behaviour is observable through the **existing structured log** surface, continuing the
  `M0_OK` / `M3_INFO` / `M4_INFO` pattern. Selection, entity creation and destruction, asset
  assignment, and transform commits each log.
- **Commit boundary**: continuous widgets (`DragFloat` and similar) log on
  `ImGui::IsItemDeactivatedAfterEdit()` — on release, with the final value. Discrete widgets
  (buttons, combos) log on the event. Never on every frame, never on intermediate drag values.
- **Transform logs carry both units, explicitly marked.** Because the widget is in degrees while
  the `Scene` stores radians (Q13), there are two numbers and the log records both, e.g.
  `M5_INFO transform-edited entity=<id> field=rotation.y value=90.000deg radians=1.5708`. Logging
  only the stored value would make the evidence disagree with what the user actually saw and
  verified; logging only the displayed value would hide what the `Scene` holds. Asset-picker
  filtering is logged the same way, e.g. `M5_INFO asset-picker n=<renderable>
  hidden=<reference-only>` (Q7).
- **The frozen window-title contract is not reopened.** M4's `WM_NAME` titles and their `xprop`
  verification stay exactly as they are; the editor does not rewrite the title.
- **No readback, no screenshot comparison, no image similarity.** M3's exclusion continues; M5
  adds UI but no rendering validation.

### Q9 — First-slice input ownership gate
- The first M5 vertical slice includes the **input ownership gate** — viewport containment plus
  ImGui capture arbitration — but **not** the camera interaction semantics.
- **The gate is binary**: input belongs to the UI **or** to the camera. Operating a Hierarchy or
  Inspector widget must never drive the camera.
- Camera *semantics* (orbit/pan/zoom mapping, deltas, baseline) remain ticket **0026**, which is
  now in M5 scope and blocked by this gate (see Q14 and 0026's updated status).
- Rationale: the current runtime applies camera input **unconditionally** — no viewport
  containment, no ImGui capture check, and an unconditionally accumulated scroll delta feeding
  `camera.zoom()`. Without the gate, the moment a panel exists, interacting with a panel will
  move the camera, and scrolling a Hierarchy list will zoom the view. The gate is therefore a
  prerequisite of any docked layout, not an enhancement.

### Q10 — Precise mouse and wheel arbitration (non-obvious constraint)
- **First-version predicate** for mouse-driven camera input:
  ```cpp
  mouseCameraInputAllowed =
      viewportHovered            // ImGui::IsWindowHovered() on the 3D viewport window
      && !ImGui::IsAnyItemHovered()   // not over any widget
      && !ImGui::IsAnyItemActive()    // no widget drag in progress
      && !globalModalCapture;         // real popups / modals only
  ```
- **`!ImGui::GetIO().WantCaptureMouse` MUST NOT be used as the viewport camera-input predicate.**
  This is a **non-obvious constraint** and the single most fragile thing in this ADR:
  > The 3D viewport is itself an ImGui window. Therefore, when the cursor is inside the viewport
  > window, ImGui mouse capture may legitimately be true even though the application is expected
  > to consume camera interaction there. Viewport input ownership is determined explicitly by
  > viewport hover state plus ImGui item arbitration, **not** by `WantCaptureMouse`.
  The intuitive spelling `inViewport && !io.WantCaptureMouse` therefore makes camera input
  **permanently dead**. ImGui's own documentation points at `WantCaptureMouse` for
  app-vs-ImGui dispatch, so this trap is actively inviting; anyone "fixing" the predicate toward
  it will silently disable all camera interaction. `io.WantCaptureMouse` is reserved for
  **genuinely global** capture (modals, popups) and is not a viewport gate.
- `ImGui::IsWindowHovered()` alone is also insufficient: it does not account for a widget drag
  in progress, which is exactly the case where a drag started in the Inspector and moved over the
  viewport must **not** hand control to the camera. Hence `!IsAnyItemActive()`.
- **The wheel is governed by the same gate**, as containment/ownership, not as a separate rule:
  - The viewport window is created with `ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse`, so the wheel is not consumed by ImGui's scrolling
    mechanism and remains available to the app.
  - Hierarchy / Inspector: mouse and wheel both belong to the UI. Viewport: both belong to the
    camera. An ImGui item being operated inside the viewport still wins, via the same item
    arbitration.
- **A direct consequence, recorded so it is not re-decided elsewhere**: while a widget drag runs
  and the cursor moves over the viewport, the camera stays frozen. When that drag ends the cursor
  is wherever it is, so a **cursor-baseline re-establishment is required** on widget-drag end.
  That is a **camera-baseline** problem owned by 0026 — not an input-ownership problem. 0026 does
  not re-decide ownership; it defines semantics and baselines under the gate fixed here.

### Q11 — `Editor` owns the `Scene`; the UI receives a facade
- **`Editor` owns the `Scene` by value** and the editor/session state with it:
  ```text
  renderScene() → Editor { Scene scene; EditorState state; }
  ```
  Not by reference: single ownership is what makes M6's snapshot-replace a local operation
  (`replaceScene(snapshot)`) instead of a fight with a `renderScene()` local's lifetime.
- **The UI receives the `Editor` facade, never a mutable `Scene&` and never an `Entity*`.** The
  UI has no route to `Scene&`, `Entity*`, or `std::span<Entity>` — the constraint is a
  **type/interface boundary**, not a convention. This is what makes Q3's discipline structural
  instead of advisory: with a mutable `Scene&` in scope, a direct field write would remain
  perfectly legal and unnoticed.
- The facade exposes **semantic operations keyed by `EntityId`** — select, edit transform, assign
  asset, create entity, destroy entity. The exact spelling and signature are left to
  implementation; the **boundary is locked now** to avoid over-designing the API during grilling.
- **`Editor` introduces no second render or resolution path.** Rendering remains exactly
  `collectDrawItems(editor.scene(), resolver)`; the single M4 resolution seam is unchanged, and
  the borrowed `DrawItem::model` lifetime contract is untouched.
- **`Editor` is not** an asset resolver, a `Model` owner, a renderer, or a GPU-resource owner. It
  is the owner of the Scene and of the editor/session mutation boundary, and nothing else
  (consistent with Q1).

### Q12 — Gizmo / direct manipulation is out of M5
- **M5 transform editing is inspector numeric fields only.** No on-viewport gizmo, no axis
  handles, no drag-to-transform in the 3D view.
- This is a scope decision with a structural consequence, not just a feature-list entry: a gizmo
  introduces a **third input owner** (`idle` / `orbiting` / `gizmo-dragging`), which would replace
  Q10's boolean arbitration with a state machine and drag viewport picking, ray/handle hit-testing
  and screen-space handle layout into the same slice.
- Gizmo work is recorded as a **named follow-up** (direct-manipulation milestone), consistent with
  ADR-0010 having already listed gizmos as out of scope.
- It is also the choice that keeps the slice verifiable: numeric edit semantics (read → validate →
  commit → log) are headless-testable and log-observable, whereas gizmo behaviour is largely
  "it looks right" — which is the class of thing M3's and M4's close criteria deliberately
  excluded.

### Q13 — Transform presentation: degrees at the UI, radians in the `Scene`
- **`Scene` stores radians and stays that way.** `Transform::rotation` and `scene::modelMatrix`
  are unchanged (ADR-0010 Q3).
- **Unit conversion is a presentation-layer concern**, living in the `src/ui/` adapter — the same
  principle that puts pixel→radian mapping in the app/input layer for the camera (ADR-0010 Q7)
  and keeps `renderer::Camera` sensitivity-agnostic. The storage format is never leaked as the
  user-facing format: radians are unreadable for a human (0.005 rad ≈ 0.29°).
- **Fields are labelled by semantic role — `Pitch (X)` / `Yaw (Y)` / `Roll (Z)` — and the applied
  order is shown once** ("Apply order: Y → X → Z"), not left for the user to infer from the
  letters. This is a correctness requirement, not cosmetics: `modelMatrix` composes
  `glm::eulerAngleYXZ(rotation.y, rotation.x, rotation.z)`, so **storage letter order is not
  application order**. A panel labelled only "X / Y / Z" is actively misleading whenever two axes
  are non-zero.
- **The ImGui widget's source of truth is the `Scene` value, every frame**: read → convert to
  degrees → feed the widget → on `IsItemDeactivatedAfterEdit()` convert back and commit. **No
  persistent degrees copy is kept in UI state**, which would be a second representation of the
  truth and invites per-frame write-back drift.
- **Displayed precision is authoritative on commit (WYSIWYG)**: the value written is the displayed
  value converted back to radians, so quantisation to the displayed precision (~1.7e-5 rad at
  three decimal places of a degree) is really written to the `Scene`. This is stated explicitly
  so that "I stored 33.333333333° and it became 33.333°" is a documented contract, not a bug
  report.
- `position` and `scale` are unitless. Per-field reset-to-default widgets are **not** part of the
  first slice's required set.

### Q14 — Documentation landing
- This ADR carries the M5 spec. **ADR-0010 is not modified**; its Q1/Q3/Q5/Q6/Q7/Q8 contracts are
  all preserved, and that preservation is stated here as an auditable claim rather than left as
  silence.
- **ADR-0004 is amended**: the viewport is no longer identical to the framebuffer, and the aspect
  source is the viewport rect.
- **ADR-0005 is amended**: pass ownership extends to `glViewport`/scissor establishment from the
  app layer, and wheel ownership follows the pass contract.
- **Ticket 0026** becomes active M5 scope, explicitly blocked by this ADR's first-slice input
  gate, and narrowed to camera semantics plus baseline.
- **Ticket 0027** becomes an M6-preceding architecture gate with its premise corrected (Q1).

## Consequences
- The M5 runtime stops being "one full-window 3D view with unconditional input" and becomes a
  docked workspace whose view geometry, input ownership, and editor state all have explicit
  contracts.
- **Target graph today** (unchanged by this ADR, and the property Q5 depends on):
  `noto3d_app` (static: `cli_options.cpp`, `draw_list.cpp`) → `noto3d_scene` + `noto3d_renderer`,
  with **no ImGui and no GLFW**; `noto3d-editor` (executable: `src/app/main.cpp`) →
  `glfw glad glm imgui noto3d_app noto3d_asset`. Editor logic added to `noto3d_app` must not add
  new renderer/GL type dependencies — `Editor` holds `Scene` + `EntityId`, no renderer types
  (this is the same coupling recorded by 0025⑧).
- `Editor` becomes the single owner of the editable `Scene`, which gives M6 a ready-made
  snapshot-replace boundary and gives M5 a place to hold session state.
- Verification stays honest: the parts of M5 with real semantics (selection, edit, asset
  filtering, viewport geometry, input gating) are headless-testable, and the parts that are
  inherently visual (layout, docking, widget feel) are manual smoke evidence — the same two-tier
  split ADR-0010 Q8 locked for M4.
- M5 adds UI but no rendering validation: no readback, no screenshot comparison, no similarity.
- M6 inherits a settled editor shape: one `Scene` owner, one resolution seam, no undo to
  serialise, and a per-view `ViewportState` that a multi-view layout can instantiate.

## Spec: M5 ImGui Editor (PRD)

### Problem Statement

The runtime can hold and render a composed Scene of emoji parts, but there is no way to *edit*
it. There is no Hierarchy to see the entities, no Inspector to change a transform, no way to add
or remove a part, and no way to see a 3D view next to UI at all — the single full-window 3D view
is all there is, and its camera consumes input unconditionally. M6 will need to persist whatever
the user builds, which means the editor must first establish what an editable document *is* and
who is allowed to mutate it. Without that, M5 becomes a pile of `main.cpp` widget calls with
mutable `Entity*` handles floating through them, and M6 inherits the mess.

### Solution

Introduce the **M5 editor** as a docked ImGui workspace — 3D viewport + Hierarchy + Inspector —
built on three explicit boundaries: an **`Editor` facade that owns the `Scene` and is the only
mutation route** (Q11), **ImGui-free editor logic in the app layer** so selection/editing/geometry
are headless-testable (Q5), and an **explicit input-ownership gate** so UI and camera never both
own the same gesture (Q9/Q10). Transform editing is numeric, in the Inspector, in degrees
(Q13); on-viewport gizmos are excluded (Q12); undo/redo is excluded (Q4). The viewport's geometry
becomes an explicit `ViewportRect` that drives projection aspect, cursor mapping, and GL viewport
state (Q0).

### User Stories

1. As a user, I want the 3D view docked next to panels, so I can see the scene while editing it,
   and the view's aspect and framing stay correct whatever shape the viewport is.
2. As a user, I want a Hierarchy listing the scene's entities in creation order, so I can see
   what is in the document and select something.
3. As a user, I want a selection that survives frames and repaints, so clicking a row keeps the
   entity selected.
4. As a user, I want an Inspector showing the selected entity's position, rotation, and scale in
   **degrees** for rotation, labelled by their real meaning (`Pitch (X)` / `Yaw (Y)` /
   `Roll (Z)`), so I can type a value I understand and know which axis I am changing.
5. As a user, I want transform edits to take effect immediately, so the 3D view reflects what I
   typed without an extra confirm step.
6. As a user, I want to create and delete entities, and to change which asset an entity uses,
   from a picker that only offers things that can actually be drawn.
7. As a user, I want the camera to respond only when I interact with the 3D view — dragging a
   slider or scrolling the Hierarchy list must not move the camera — so panel work does not
   corrupt my view.
8. As a developer, I want editor logic (selection, edit operations, asset filtering, viewport
   geometry) testable without a display or a GL context, so M5 closes on automated evidence
   rather than screenshots.
9. As a developer, I want the UI structurally unable to mutate the `Scene` outside the facade, so
   the `EntityId`-only write path (Q3) is enforced by types instead of by good intentions.
10. As a maintainer, I want M5's close to be an automated matrix plus a manual smoke check, with
    no readback or image similarity, so the milestone answers "can I edit a Scene with a mouse?"
    without becoming a rendering-validation project.
11. As a developer for M6, I want a single owner of the `Scene` and a single resolution seam, so
    save/load is a snapshot operation over a settled document model.

### Testing Decisions
- **What good looks like** — external behaviour only: selection behaviour, edit-operation
  semantics, entity create/destroy, asset-picker filtering, viewport rect/containment maths, and
  the input-gate predicate as a pure function. No widget rendering correctness, no readback, no
  image comparison.
- **Seams** — editor logic is ImGui-free and lives in `noto3d_app`, so new suites in `tests/app/`
  link `noto3d_app` and run headlessly. The input gate is specified as a **pure predicate over
  (hovered, anyItemHovered, anyItemActive, globalModalCapture)**, precisely so it can be tested
  without ImGui: the four ImGui queries are read at the adapter edge and passed in.
- **Process-level verification is two-tier** (continuing ADR-0010 Q8):
  - **Automated (dev + release)**: CTest green with the new suites in the count; structured
    runtime-log assertions for selection, entity lifecycle, asset assignment, and transform
    commits; the existing M4 CLI exit-code matrix and `xprop` title matrix unchanged.
  - **Manual / platform-specific evidence (recorded in the ticket)**: docking behaviour, panel
    layout, and the visual/interaction smoke — that the gate actually feels right and that
    dragging a panel widget does not move the camera.
- **Acceptance of the manual tier is manual confirmation, not a rendering-correctness test** —
    the same standard 0024 used.
- **Prior art** — `check()`/`ALL PASS` runner; 0021's `tests/app/` draw-list suite (the pattern
  for app-layer logic tests); 0025's `cli_options_test.cpp` (proving a `noto3d_app` TU can be
  tested without pulling GL in); 0017's exit-code + `WM_NAME` verification.

### Out of Scope
- **Undo/redo** (Q4) — no command history, no apply-on-release, no drag coalescing, no
  destructive destroy/recreate undo.
- **Gizmos / direct manipulation in the 3D view** (Q12) — no picking, no axis handles, no
  screen-space handle layout, and therefore no third input owner.
- **The asset CPU/GPU boundary refactor** (Q1) — 0027 is an M6 gate, not M5 work; M5 introduces
  no new GPU-resource ownership.
- **Full camera interaction semantics** — orbit/pan/zoom mapping, sensitivity configuration and
  cursor-baseline policy are 0026, behind this ADR's input gate.
- **Save/load, scene serialization, `--scene=`** (M6).
- **Editor preferences / persistence of settings**, multi-window or split-view layouts (M5 ships
  one viewport; `ViewportState` is per-view so a split is an instantiation later), and any scene
  graph, prefab, or ECS concepts (M4 exclusions continue).
- **Screenshot/readback/image-similarity** (M3 exclusion continues).

### Further Notes
- The first slice is deliberately ordered so that the **input gate lands with the layout that
  needs it**: docked layout and `ViewportRect` establish where the view is, the gate establishes
  who owns input, and only then does camera semantics (0026) have a stable place to attach.
- Q10's `WantCaptureMouse` exclusion and Q13's WYSIWYG commit rule are the two contracts most
  likely to be "corrected" by a later change, because both contradict a plausible reading of
  upstream ImGui/GL documentation. Both are recorded here for that reason.
- `ViewportState` being per-view while M5 ships one view is intentional: it is a shape decision
  that costs nothing now and prevents the "one global camera" rewrite later.
- M5's editor logic must not add renderer/GL type dependencies to `noto3d_app` (Q5). This is
  checkable by the same target-graph observation 0025⑧ recorded.

## Amendment — none yet

This ADR is Accepted as written. Any later milestone needing a different viewport, input, or
editor-ownership convention **supersedes this ADR** rather than editing it (DOCUMENTATION-LIFECYCLE:
ADRs are append-and-supersede, never overwrite). Tickets 0026 and 0027 are updated in the same
change as this ADR to match Q1 and Q14.
