# 0025 — M4 code cleanup (narrowed scope)

**Source:** code review of tickets 0021–0024 (Sep 2026), then a strict re-review (Sep 2026) that
narrowed this ticket to work that is actually safe to do now.
**Status:** ready-for-agent
**Blocked by:** none (infrastructure pass, independent of M5)

> **Scope discipline (Sep 2026 re-review).** This ticket deliberately does **not** remove public
> APIs, does **not** introduce test-only build conditionals, does **not** split libraries, and
> does **not** pre-design M5 features. Items that need an architecture decision are recorded as
> deferred notes here and split into follow-up tickets:
> - M5 input/interaction concerns → **0026** (cursor baseline semantics, camera interaction
>   configuration ownership)
> - asset CPU/GPU library split → **0027** (architecture spike)
>
> Two original claims were stale and were corrected: the `ShaderProgram` uniform cache already
> exists (0011⑩ delivered with 0012) and `loadManifestOnly()` is **not** test-only — it backs the
> production `--list-assets` diagnostic CLI (0017/ADR-0009).

> **Top-level invariant (applies to every item below).** 0025 must not change externally observable
> M4 runtime behaviour, CLI semantics, or any established architecture contract: `scene::Scene`
> stays pure data, `scene::modelMatrix` stays the single T·R·S definition, `app::collectDrawItems`
> stays an app-layer resolution seam with borrowed `DrawItem::model` pointers, `AssetRegistry`
> remains the sole asset authority, and `renderer::Camera` semantics (orbit/zoom/pan signs, clamps,
> initial view) are untouched. When an item is ambiguous, this invariant wins: leave the behaviour
> alone, do the structural move, and record the open question in the evidence.

---

## P1 — ① Extract CLI option parsing to `src/app/cli_options.{hpp,cpp}`

`main.cpp` currently holds `Options`, `parseOptions`, the window constants, `InputState`, the GLFW
callbacks and the sensitivity constants in one anonymous namespace. `Options`/`parseOptions` cannot
be verified headlessly without forking a subprocess — the CLI contract (ADR-0010 Q8) is currently
only checked through the built binary.

**Work:**
- `src/app/cli_options.hpp` — `Options` (+ a mode discriminator for default / `--asset` / `--demo`
  / `--list-assets` if that reads better than the current optional/flag pair) and parse
  declarations.
- `src/app/cli_options.cpp` — `parseOptions` with today's validation **unchanged**: validated
  `--frames=` (non-negative integer, 0011-⑨), `--asset=` with non-empty id, `--demo`,
  `--list-assets`, and the unknown-flag rejection added in 0022.
- `tests/app/cli_options_test.cpp` — headless, dependency-free, **asserts on `Options` content,
  not only on exit codes**: default; `--asset=smile|crown|sunglasses`; `--demo`; unknown asset;
  `--asset` + `--demo`; unknown flag; malformed `--frames=abc` / negative `--frames=`.
- `main.cpp` keeps only GLFW lifecycle, callbacks, `InputState` and the sensitivity constants
  (see the note below on constant placement).

**Boundary note:** window geometry/title/version constants may stay in `main.cpp` or move to a small
app-config header; at this size either is acceptable — do not create a new module for them.

**Invariant:** CLI behaviour is byte-for-byte identical after the move; the 0024 matrix
(exit codes + `M4_INFO` lines + titles) must stay green.

**Points to ADR:** 0008 (module layout `src/app`, dependency-free headless tests) · 0010 (Q8
regression contract) · 0009 (M3 observability preserved)

---

## P2 — ④ Flatten the GL lifecycle scope in `run()`

`run()` wraps ~200 lines in a bare `{ ... }` block whose only purpose is to guarantee that GL-owning
objects (shader, registry, renderer, scene) are destroyed **while the context is still current**.

**Work:** extract that block into a helper returning the exit code —
`static int renderScene(GLFWwindow* window, const Options& options)` — so `run()` reads as: GLFW
init → window creation → `renderScene()` → window/context destruction. If the implementation needs
further parameters, derive them from the existing code rather than inventing a new contract; the
GL-owning objects (shader, registry, renderer, scene) are created **inside** the helper, not passed
in. Preserve destruction ordering exactly; this is a readability change, not a behaviour change.

**Depends on:** ① (the helper takes `const Options&` from the extracted header).

**Points to ADR:** 0008 · 0012 (the GL-scope lifetime rule the extraction must preserve)

---

## P2 (verification-only) — ② Verify the uniform-location cache

**No code change is planned.** The lazy per-name cache already exists
(`m_uniformCache` in `src/renderer/shader_program.hpp`, lookup/store in `shader_program.cpp`,
cleared on `reload()`), delivered by 0011⑩ with 0012.

**Work:** confirm the cache is still present and correct. Record evidence for exactly these three
invariants:
1. cache hit → **no** `glGetUniformLocation` call;
2. cache miss → query once, then store;
3. `reload()` → cache cleared.

Code evidence is sufficient; do **not** build a new GL-context test for this item. If — and only if
— a defect is found, fix it and say so in the ticket evidence. Do not "improve" a working cache to
have something to show for this item.

**Tracked by:** 0011⑩ / 0012 (complete).

---

## Deferred decision — ③ Audit the `loadManifestOnly()` test seam (do not remove)

`loadManifestOnly()` (CPU-only manifest parse, no GL upload) is a **public, production-used** API:
`main.cpp::listAssetsCommand()` uses it for the pre-GL `--list-assets` diagnostic CLI (0017,
ADR-0009). AssetRegistry's own tests are *supposed* to exercise `manifest → AssetRegistry`, so the
draw-list tests' injectable-`Resolver` pattern is not the same seam and does not transfer.

**Record (documentation only — no judgement calls here):**
1. Current role: the production entry point behind `--list-assets`, plus the CPU-only path used by
   asset tests; the reason it exists (AssetRegistry's GL coupling, see 005 / 0027).
2. Current constraints: it is a public production API; the draw-list tests' injectable-`Resolver`
   pattern is a **different layer** and does not replace it (asset tests are meant to exercise
   `manifest → AssetRegistry`).
3. Which question belongs to 0027: whether the CPU/GPU coupling should be restructured, and whether
   `loadManifestOnly()` survives, becomes the `asset_cpu` entry point, or is superseded.

**Explicitly out of scope:** deciding or redesigning the future seam here, removing the method,
moving it to a `testing` namespace, adding `#ifdef` gating (production `--list-assets` calls it, and
a config-dependent public interface is a worse contract), or refactoring manifest parsing.
**Output:** current state + constraints + the open question handed to 0027 — no architecture
decision, no ADR draft.

**Points to ADR:** 0007 (AssetRegistry is the sole asset authority) · 0009 (0017 pre-GL
`--list-assets`)

---

## Deferred — ⑤ `noto3d_asset` → `noto3d_renderer` PUBLIC coupling (architecture note only)

`SubMesh::mesh` is a `renderer::Mesh`, so `noto3d_asset` links `noto3d_renderer` PUBLIC and any
manifest-only consumer transitively pulls glad + GL headers. Splitting `asset_cpu` / `asset_gpu`
is an **architecture refactor**, not cleanup: it needs an ownership decision for
`CanonicalShape` / CPU `Material` / manifest representation vs `Mesh`/`SubMesh`/`Material`/
`Texture`/`Model`, plus a decision on where GL upload happens.

**No implementation in this ticket.** Recorded in **0027** (spike) for M5+.

---

## P2 (verification-only) — ⑧ Verify production/test target boundaries

Before any dependency refactor, confirm the CMake graph matches intent:

- `noto3d_scene` → glm only; no asset / renderer / GL.
- `noto3d_app` → scene + app seam (draw-list); no GL window/GLFW dependency in the seam library.
- `noto3d_asset` → `noto3d_renderer` (GL) — currently PUBLIC; **expected today, and the thing
  0027 will address**. Record which test targets actually need the GL link
  (`tests/asset/` does today; `tests/scene/` and `tests/app/` should not).
- `tests/renderer/camera_test.cpp` → must stay headless (no GL context needed for the math).

**Work:** read `CMakeLists.txt`, record the actual graph in the ticket evidence, fix only clear
oversights (e.g. a target linking GL unnecessarily). No library splitting here.

**Boundary:** do **not** change `noto3d_asset → noto3d_renderer` link visibility (PUBLIC → PRIVATE or
vice versa) to make the graph look tidier — that decision belongs to 0027 and include propagation
depends on it.

---

## Acceptance

- [ ] ① `src/app/cli_options.{hpp,cpp}` extracted; `tests/app/cli_options_test.cpp` asserts
      `Options` **content** (default / `--asset` / `--demo` / unknown asset / `--asset`+`--demo` /
      unknown flag / malformed `--frames`) headlessly
- [ ] ① CLI behaviour unchanged: 0024 matrix (exit codes, `M4_INFO` lines, titles) still green on
      dev + release
- [ ] ④ `renderScene()` extraction preserves GL-scope destruction ordering; ctest + matrix green
- [ ] ② uniform-location cache verified against three invariants (hit → no query, miss → query +
      store, reload → cleared); no gratuitous change
- [ ] ③ current role + constraints + the 0027 hand-off question recorded; **no** architecture
      judgement, no removal, no `#ifdef` gating
- [ ] ⑤ recorded as deferred → 0027; no implementation
- [ ] ⑧ actual CMake target graph recorded; only clear oversights fixed; `noto3d_asset →
      noto3d_renderer` visibility untouched
- [ ] Global invariant held: no externally observable M4 behaviour or established architecture
      contract changed
- [ ] Follow-ups split out: 0026 (M5 input/interaction), 0027 (asset CPU/GPU architecture spike)
- [ ] CHANGELOG + CONTEXT updated; no M5 functionality pulled into this ticket

**Rolls under spec:** infrastructure cleanup — not a feature spec; bounded by ADR-0008 (module
boundaries) and ADR-0010 Q8 (CLI regression contract).
