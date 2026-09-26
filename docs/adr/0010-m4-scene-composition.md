# ADR-0010 — M4 scene composition & transform: pure-data Scene, draw-list seam, camera

Status: Accepted

> Per DOCUMENTATION-LIFECYCLE: **specs (PRDs) live at the ADR layer**; `docs/ticket/` holds
> execution tickets derived from accepted ADRs. This ADR embeds the M4 milestone spec (Problem /
> Solution / User Stories / Testing / Out of Scope) below; the M4 execution tickets 0020–0024 roll
> under it.

## Context

M3 (0015–0018) is closed: one asset can be reliably selected and rendered. M4 must prove the
complementary capability — **multiple assets stably composed, transformed, and observed as a
Scene** — and land the architecture that M5 (editor UI) and M6 (persistence) build on.

The following 8 decisions were pressure-tested in a grilling session and are locked here so that no
later ticket re-litigates them. They resolve the remaining ambiguities that would otherwise pollute
0024 regression and M6 persistence.

## Decision (locked 2026-09-26, grilling Q1–Q8)

### Q1 — Scene ownership (pure-data Scene)
- `Scene`/`Entity` store stable asset IDs and transform data **only**. They do not reference
  `AssetRegistry`, `Model`, GPU objects, renderer state, or a logger.
- `Scene` does not know how an assetId becomes a `Model`. Runtime asset resolution is performed by
  the **application / render orchestration layer** through the existing `AssetRegistry` API.
- An **unresolved `assetId` does not invalidate the Scene**; the runtime reports and skips the
  affected Entity. Data layer never makes persistence decisions for the persistence layer.

### Q2 — Entity identity / storage
- `EntityId` = `std::uint32_t`; `0` = `InvalidEntityId`; first generated id = 1.
- Monotonic counter; **ids never reused during Scene lifetime**; `EntityId` ≠ vector index.
- Storage = `std::vector<Entity>`; `entities()` iterates in **creation order**.
- `findEntity` = linear scan (editor scale, O(n) accepted); no `id → index` reverse map; no
  free-list / tombstone / ECS-style sparse storage.
- **Lifetime contract**: references/pointers/spans obtained from `Scene` are invalidated by
  subsequent structural mutation (`createEntity` may reallocate, `destroyEntity` erases and shifts).

### Q3 — Transform / model matrix
- `Transform` stores Euler **radians**: `rotation.x = pitch (X)`, `rotation.y = yaw (Y)`,
  `rotation.z = roll (Z)`. Convention **YXZ**, sole implementation `glm::eulerAngleYXZ`.
  Degrees exist only at an M5 UI input boundary.
- `glm::eulerAngleYXZ` is an **experimental GTX API**: `GLM_ENABLE_EXPERIMENTAL` is required
  **only in the translation units that call it** and must never become a project-wide compile
  definition. Anyone changing this implementation must re-baseline ADR-0010 first.
- `modelMatrix(const Transform&) -> glm::mat4` is a **pure free function in `noto3d::scene`**
  (Scene already depends on glm via `Transform` fields; not a member — data layers stay dumb;
  not duplicated in app).
- Multiplication order fixed: **`M = T·R·S`** (scale → rotate → translate). 0022/0024 must not
  recompose matrices.
- M4 is a **flat world-space entity list**: `transform` IS the world transform; no
  parent/child, local/global, or scene-graph evaluation.
- Rotation correctness is verified by **matrix unit tests**, not visuals (assets are symmetric,
  unlit white placeholders — rotation is visually invisible at M4).

### Q4 — EntityId lifetime / M6 persistence boundary (forward contract)
- `Scene::clear()` removes all entities **without resetting `nextEntityId_`** — ids never repeated
  for the life of the Scene object, with a single unambiguous definition.
- M6 JSON = **complete Scene snapshot**; loading **replaces** the entire Scene (entities +
  id allocation state), never merges. Loader restores stored ids and sets
  `nextEntityId = max(restored ids) + 1` (empty snapshot → 1); loader must validate id ≠ 0,
  uniqueness within snapshot, uint32 range, and `max + 1` overflow.
- `EntityId` is the **single identity** (runtime = persistent). No second persistent-key identity
  space (reserved for a future multi-scene/merge ADR only). 0020 ships this as documentation
  contract, not serialization code (serialization is M6).

### Q5 — Resolution–render seam (draw list)
- A pure, headless-testable seam in **`noto3d::app`**:
  `collectDrawItems(const scene::Scene&, const Resolver&) -> std::vector<DrawItem>` where
  ```cpp
  struct DrawItem { const renderer::Model* model; std::string assetId; glm::mat4 modelMatrix; };
  using Resolver = std::function<const renderer::Model*(const std::string&)>;
  ```
- `DrawItem` is frame-local orchestration data: no ownership, no cache, not stored back into
  Scene/Entity. One Entity → at most one DrawItem; two Entities sharing an assetId → two DrawItems
  (asset identity ≠ entity identity).
- **`DrawItem::model` is a borrowed pointer**: its validity is guaranteed only for the duration of
  the draw-list result / current frame. It must not be retained across `AssetRegistry` mutation or
  `reload()` — the same lifetime discipline Q2 imposes on Scene references.
- **`Resolver` must be callable.** An empty `std::function` is a **programming error**, not a
  supported runtime state: `collectDrawItems` guards it explicitly (never silently emitting an
  empty list via `std::bad_function_call`); it is not treated as "everything unresolved".
- Unresolved assetId → **skip, not failure** (no DrawItem, frame continues).
- No `SceneRenderer` class, no resolution cache (`AssetRegistry` is the single asset authority).
- New test dir `tests/app/` is in scope for 0021.
- Dependency direction stays: `scene` knows nothing; `renderer` doesn't need `scene`; app works.

### Q6 — CLI → Scene runtime model
- Every supported runtime path **constructs a Scene** and reaches the Renderer **only through
  `collectDrawItems()`** — no legacy `registry.model() → submit()` bypass in `main.cpp`.
- `default` = named fixture scene `m4_vertical_scene`: `smile@(-1.5,0,0)`, `crown@(0,0,0)`,
  `sunglasses@(1.5,0,0)`; title `m4_vertical_scene | noto3d (M4)`; logs entity count.
- `--asset=<id>` = **single-entity Scene** (never built-then-filtered): keeps M3 identity line +
  title `<id> [placeholder] | noto3d (M4)`; unknown asset → exit 1 (M3 exit-code matrix intact).
- `--demo` = **the M2 0014 demo fixture as a demo-only Scene** (NOT the default scene plus an
  appended entity): constructs a Scene containing **exactly one Entity**,
  `assetId = "demo_checker"` (identity transform per the 0014 fixture contract), resolved via the
  registry's standalone fixture path (`loadModelFile`, ADR-0007 amendment). It does **not** imply
  `m4_vertical_scene`. Title follows the single-entity rule
  (`demo_checker [placeholder] | noto3d (M4)`); M2 `M2_OK (0009 E2E)` prints through the new
  pipeline.
- `--asset=<id>` + `--demo` = **invalid invocation → exit 1** (mutually exclusive entry modes).
- No `--scene=` flag in M4 (scene selection ≠ persistence; fixture chosen by code constant).
- Version bump M3 → M4 in titles is a conscious contract change (xprop verification method
  unchanged).

### Q7 — Camera & interaction
- `noto3d::renderer::Camera`: a pure **value/state object**, GL-independent, Scene/Entity-
  independent, no GLFW dependency, no Input abstraction in M4.
- State guarded via operations (`orbit/zoom/pan`) + const getters; never expose mutable refs.
- Radians throughout. **Initial view reproduces the M3 camera** — verified in tests by equality
  against `glm::lookAt(...)`, not "looks about right".
- **Spherical convention (locked)**: eye derives from orbit state exactly as
  ```cpp
  offset.x = cos(pitch) * sin(yaw) * distance;
  offset.y = sin(pitch) * distance;
  offset.z = cos(pitch) * cos(yaw) * distance;
  eye      = target + offset;
  ```
  worldUp = `(0,1,0)`; frame = `forward = normalize(target - eye)`,
  `right = normalize(cross(forward, worldUp))`, `up = cross(right, forward)` (orthonormal).
- **Initial state (locked)**: `target = (0,0,0)`, `yaw = 0`,
  `distance = sqrt(9.25) ≈ 3.0414`, `pitch = asin(0.5/sqrt(9.25)) ≈ +9.46°`. Feeding the locked
  formula gives eye = **exactly `(0, 0.5, 3)`** (the M3 camera). Tests must assert on the
  yaw/pitch/distance primitives, not on hand-copied eye values.
- **`orbit(dx,dy)`**: `yaw += dx`; `pitch += dy`, **pitch clamped to ±89°** (no pole flip).
  Positive dx increases yaw (eye orbits toward camera-right — the +X side of the initial view);
  positive dy increases pitch (eye rises). The App owns pixel→radian mapping (including any
  gesture sign flip); the Camera contract is only the numeric direction: yaw grows with dx, pitch
  grows with dy.
- **`zoom(delta)`**: `distance = clamp(distance - delta, 0.5f, 20.0f)`. Positive delta = zoom-in
  (orbit distance decreases), negative = zoom-out. Changes orbit distance, **not FOV**; the
  `[0.5, 20.0]` clamp is an interaction range independent of the near/far clipping planes.
- **`pan(dx,dy)`**: `target += right * dx + up * dy` — inputs are **world-space displacement
  units** along camera-local axes: positive dx moves target along camera-local right, positive dy
  along camera-local up. Eye is re-derived as `target + spherical offset` (orbit state unchanged),
  keeping orbit/zoom/pan consistent. The App converts mouse pixels → world units before calling
  Camera.
- Projection: FOV 45°, near 0.1, far 100 (clipping planes), `projectionMatrix(aspect)` per frame
  from the latest framebuffer size.
- Input wiring: GLFW polling/callbacks in app → Camera methods; **no `InputManager/State/Event`,
  no ActionMap** at M4. Reassess under ImGui at M5.

### Q8 — Integration validation (M4 close)
- **0024 = automated regression matrix + one lightweight manual smoke check**, no separate
  human-validation ticket (3 entities + camera interaction is smoke-check territory; M3's 0018
  matrix was a per-asset category audit and does not repeat).
- **Automated (dev + release), observable at process level:**
  - CLI exit-code matrix: default / `--asset=smile|crown|sunglasses` / `--demo` → 0;
    unknown asset / `--asset`+`--demo` / invalid flag → 1.
  - **Scene construction semantics via the app's structured runtime log** —
    `M4_INFO rendering scene '<name>' entities=<N>` proves default=3 (`m4_vertical_scene`),
    `--asset`=1, `--demo`=1 (`demo_checker`). Scene internals are **never** exposed to
    process-level tests (no `--dump-scene` debug CLI), so the ticket checks the observable log
    line, not Scene state.
  - Unresolved-asset → skip is proven by 0021's headless `tests/app/` suite (the resolution
    contract); 0024 does **not** add a production CLI entry to exercise it.
  - CTest suites green (dev + release) with the new suites included in the count.
- **Manual / platform-specific evidence (Linux/X11), recorded in the ticket — not an automated
  regression:** `WM_NAME` window-title verification via `xprop` (default `m4_vertical_scene |
  noto3d (M4)`, `--asset=<id>` → `<id> [placeholder] | noto3d (M4)`) plus the ~10 min visual and
  interaction smoke check below.
- Manual (~10 min): 3-entity layout (smile left / crown center / sunglasses right) + orbit /
  zoom / pan / ESC. **Acceptance = manual confirmation of the expected fixture layout and
  interaction behavior; it is not a rendering-correctness test.**
- **Locked exclusions continue from M3**: no screenshot comparison, no GL readback, no image
  similarity — the more entities there are, the more scope discipline matters.

## Consequences
- M4 replaces "one selected Model drawn at identity" with "a Scene of N entities, each resolved and
  transformed through one orchestration seam".
- No renderer GL contract change (`Renderer::submit(mesh, mat, shader, mvp)` stays the draw seam);
  renderer only gains the pure-math `Camera`.
- M6 gets a clean forward contract (snapshot replace, single identity, unresolved-tolerant Scene);
  M5 gets a flat world-space scene to display.
- New dependency edges: `app → scene`, tests gain `tests/scene/` and `tests/app/`; `scene` remains
  dependency-light (glm only). `renderer → scene` is avoided by design (Q5).

## Spec: M4 Scene Composition & Transform (PRD)

### Problem Statement
M3 proved that one asset can be reliably selected, frozen, and rendered. But the editor still
cannot hold more than one thing: the render loop draws a single selected Model (or the demo
fixture) at the identity transform, with no Scene, no per-entity transform, and no way to
compose, operate on, or later persist a set of emoji parts. The next milestone goal — "build a
composition from multiple Noto assets, give each its own transform, and view it through a camera"
— has no data model to land on, and the CLI/camera state live as hard-coded constants in
`main.cpp`.

### Solution
Introduce the **M4 Scene**: a pure-data, flat collection of Entities (stable `EntityId`, `name`,
`assetId`, world `Transform`), with `modelMatrix()` as its one pure math function. Route every
runtime entry path through `Scene → app::collectDrawItems() → Renderer::submit()` so single-asset
and multi-entity rendering share one seam. Add a pure-math orbit/zoom/pan `Camera` wired from
GLFW in the app layer. Close M4 with an automated regression matrix (dev + release) and a single
manual smoke check.

### User Stories
1. As a developer, I want `Scene` to create/destroy Entities with stable, never-reused
   `EntityId`s (0 = Invalid), so M6 can persist identity without ambiguity.
2. As a developer, I want Scene storage to iterate in creation order with linear `findEntity`, so
   the hand-shaking contract with M6 and M5 hierarchy stays simple.
3. As a developer, I want structural mutation to document pointer/span invalidation, so 0021's
   frame-local `DrawItem`s never cache `Entity*`/`Model*` across frames.
4. As a developer, I want `scene::modelMatrix(transform)` (radians, YXZ, T·R·S) as the single
   transform-to-matrix definition, so 0022/0024 and tests share one source of truth.
5. As a developer, I want `check()`-style headless tests for identity/storage and transform matrix
   math, so rotation correctness is verified mathematically, not by eyeballing symmetric spheres.
6. As a developer, I want `app::collectDrawItems(scene, resolver)` to map resolved entities to
   `DrawItem`s and skip unresolved ones — with a callable `Resolver` (empty = programming error)
   and a frame-local borrowed `model` pointer — so resolution is a tested contract, not a
   main.cpp side effect.
7. As a developer, I want the default run to construct the `m4_vertical_scene` fixture (smile /
   crown / sunglasses in a line) and `--asset=<id>` to construct a single-entity Scene, so every
   supported path is exercised through the Scene→DrawItem→Renderer pipeline.
8. As a developer, I want `--asset`+`--demo` rejected (exit 1), no `--scene=` flag, and the
   `(M4)` title bump, so the CLI entry model stays exactly three independent modes.
9. As a user, I want orbit (LMB), zoom (wheel), and pan (MMB) from a camera that starts where M3
   left off, so I can inspect a multi-part composition.
10. As a developer, I want camera interaction bounded (pitch ±89°, zoom distance 0.5–20 independent
    of near/far) and verifiable headlessly, so view state can't drift into clipping or pole-flips.
11. As a maintainer, I want M4 close to be an automated matrix + one manual smoke check, with no
    readback/similarity work, so the milestone answers "can I compose, transform, and view N
    assets?" without becoming a rendering-validation project.

### Testing Decisions
- **What good looks like** — external behaviour only: Scene identity/storage invariants, matrix
  math equality, draw-list resolution semantics, CLI exit codes, window-title semantics. No
  registry internals, no GPU readback.
- **Seams** — one headless CTest family, extended by three suites: `tests/scene/` (0020), the
  camera tests inside `tests/renderer/` (0023), and `tests/app/` for the draw-list seam (0021).
  Process-level verification is two-tier: automated (CLI exit codes + structured runtime-log
  assertions for Scene construction / draw-list semantics) and manual evidence (xprop `WM_NAME`
  title check, visual smoke), both recorded in the ticket.
- **Prior art** — `check()`/`ALL PASS` runner in `tests/*`; manifest-integrity suite (0016)
  shows the independent-contract pattern; 0017 shows exit-code + `WM_NAME` verification.
- **Regression** — ctest green on dev + release (suite count grows), plus the automated CLI
  matrix of 0024 alongside the manual smoke check.

### Out of Scope
- ImGui / editor UI / hierarchy / inspector / selection widgets (M5).
- Scene serialization, load/save/versioning (M6; forward contract only in Q4).
- Official Noto mesh migration (watch item; re-opens M3 scope if done now).
- ECS, scene graph, parent/child/local transforms, prefab, gizmos, undo/redo.
- `--scene=` flag, scene registry/selection at M4, resolution cache, Input abstraction.
- Screenshot/readback/image-similarity (M3 locks continue).

### Further Notes
- M4 fixture positions: `smile@(-1.5,0,0)`, `crown@(0,0,0)`, `sunglasses@(1.5,0,0)` — the 0022
  acceptance fixture and 0024's manual smoke-check subject.
- `demo_checker` is an **out-of-manifest fixture** (0014): in `--demo` mode the Scene holds
  **exactly one Entity** with `assetId = "demo_checker"` (identity transform per the fixture
  contract), resolved through the standalone `loadModelFile` fixture path (ADR-0007 amendment),
  never through the manifest. The `M3_INFO selected asset` identity line does **not** apply to
  `--demo` (no manifest selection); the M2 gate line (`M2_OK (0009 E2E)`) is the demo identity.
- Routing `--demo` through Scene → `collectDrawItems` keeps M2's `M2_OK (0009 E2E)` gate flowing
  through the new pipeline — a deliberate M4 regression probe. The demo fixture becomes the
  **whole** Scene, not an append to the default scene.

## Amendment — M4 contract hardening (2026-09-26, pre-implementation)

One-pass hardening of the locked Q1–Q8 after a strict contract review; **no architecture change**.
Critical locks that were under-specified and are now definitive:

- **Q7 Camera** — spherical-offset formula, initial `yaw/pitch/distance` (feeding the exact M3 eye
  `(0,0.5,3)`), `orbit`/`zoom`/`pan` numeric sign convention + units (yaw grows with dx, pitch
  grows with dy clamped ±89°; `distance = clamp(distance - delta, 0.5, 20)`; pan in world-space
  units along camera-local right/up).
- **Q6 `--demo`** — demo fixture = the **whole** Scene, **exactly one Entity** (`demo_checker`,
  identity transform); single-entity title rule applies.
- **Q5** — `DrawItem::model` is a borrowed pointer valid only for the frame/draw-list result
  (never retained across `AssetRegistry` reload); `Resolver` must be callable, empty = programming
  error (explicit guard, never a silent empty list).
- **Q8 verification tiers** — automated = exit codes + structured runtime-log assertions + CTest;
  `xprop` titles and the visual smoke = manual/platform-specific evidence in the ticket. Scene
  construction semantics verified via the `M4_INFO … entities=<N>` log line; no `--dump-scene`.
- **Q3** — `GLM_ENABLE_EXPERIMENTAL` stays TU-local (see Q3).

Tickets 0021–0024 updated in the same change; 0020's evidence reworded to the locked T·R·S
wording. If a later milestone needs a different spherical/gesture convention it supersedes this
ADR rather than editing it.