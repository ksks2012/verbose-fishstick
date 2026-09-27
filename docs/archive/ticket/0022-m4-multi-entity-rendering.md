# 0022 — M4 multi-entity runtime rendering (CLI → Scene)

**What to build:**

Wire the runtime up so that **every supported CLI path constructs a Scene** and reaches the
Renderer **only through `collectDrawItems()`** — per ADR-0010 Q6. Removes the current single-Model
render path in `main.cpp` (one MVP computed at identity, hard-coded camera constants, submitModel
helper) in favour of the Scene pipeline.

## Scope

**A — Scene construction per entry mode (`main.cpp`):**
- `default` ⇒ named fixture scene `m4_vertical_scene`:
  `smile@(-1.5,0,0)`, `crown@(0,0,0)`, `sunglasses@(1.5,0,0)` (names from assetId); log
  `M4_INFO rendering scene 'm4_vertical_scene' entities=3`; window title
  `m4_vertical_scene | noto3d (M4)`.
- `--asset=<id>` ⇒ **single-entity Scene** (built directly, never fixture-then-filter); keeps the
  M3 identity line (`M3_INFO selected asset ...`) and title `<id> [placeholder] | noto3d (M4)`;
  unknown asset ⇒ exit 1 (M3 exit-code contract intact).
- `--demo` ⇒ **demo-only Scene** (ADR-0010 Q6 + Amendment): a Scene containing **exactly one
  Entity** (`assetId = "demo_checker"`, identity transform per the 0014 fixture contract),
  resolved via the registry's standalone `loadModelFile` path (0014 / ADR-0007 amendment) — **not**
  the default scene plus an append, and it does **not** imply `m4_vertical_scene`. Title =
  single-entity rule (`demo_checker [placeholder] | noto3d (M4)`); `M2_OK (0009 E2E)` prints
  through the new pipeline (M3 identity line does not apply to `--demo`).
- `--asset=<id>` + `--demo` ⇒ invalid invocation ⇒ exit 1 (mutually exclusive entry modes).
- No `--scene=` flag (Q6).
- **Every entry mode logs the same structured line** `M4_INFO rendering scene '<name>'
  entities=<N>` right after Scene construction (0024's automated matrix asserts on it — the only
  process-level window into Scene size; no `--dump-scene` debug CLI).

**B — render loop update:**
- `DrawItem` list = `collectDrawItems(scene, [&](id){ return registry.model(id); })`.
- Per DrawItem: `mvp = proj * view * item.modelMatrix`; per submesh
  `renderer.submit(mesh, material, shader, mvp)`.
- `proj`/`view` still computed per frame (camera comes in 0023; keep current lookAt constants until
  then so 0023 can land its headless-equivalence test against this exact view).
- **No legacy hotspot**: no direct `registry.model() → submit()` bypass remains.

## Acceptance

- [x] Default run constructs 3-entity `m4_vertical_scene`, renders all three, title
      `m4_vertical_scene | noto3d (M4)`, exit 0
- [x] `--asset=<id>` single-entity Scene, M3 identity line kept, title `<id> [placeholder] |
      noto3d (M4)`, exit 0; unknown asset exit 1
- [x] `--demo` constructs a demo-only Scene with **exactly one Entity** (`demo_checker`, identity
      transform, title `demo_checker [placeholder] | noto3d (M4)`); `M2_OK (0009 E2E)` still
      prints, exit 0
- [x] `--asset` + `--demo` ⇒ exit 1; no `--scene=` flag exists
- [x] Render path exclusively via `collectDrawItems()` (no direct submit bypass)
- [x] Existing exit-code matrix unchanged (valid → 0; `--frames=abc`/`--asset=nope` → non-zero)

**Blocked by:** 0021 (complete)
**Status:** completed

> Evidence (2026-09-26/27): `main.cpp` builds a Scene per entry mode and reaches the Renderer only
> through `collectDrawItems()` — no legacy `registry.model() → submit()` hotspot remains. Default →
> named fixture `m4_vertical_scene` with `smile@(-1.5,0,0)` / `crown@(0,0,0)` /
> `sunglasses@(1.5,0,0)`; `--asset=<id>` → single-entity Scene built directly (never
> fixture-then-filter) keeping the `M3_INFO selected asset ...` identity line; `--demo` →
> demo-only Scene with exactly one Entity (`demo_checker`, identity transform) resolved via the
> registry's standalone `loadModelFile` path, and `M2_OK (0009 E2E)` prints through the pipeline.
> Every mode logs the single structured line `M4_INFO rendering scene '<name>' entities=<N>` right
> after construction (the only process-level window into Scene size; no `--dump-scene`). Titles per
> the single-entity rule: `m4_vertical_scene | noto3d (M4)` / `<id> [placeholder] | noto3d (M4)` /
> `demo_checker [placeholder] | noto3d (M4)`. `--asset`+`--demo` → exit 1 (mutually exclusive);
> no `--scene=` flag. Q8's "invalid flag" row hardened in this ticket: `parseOptions` now rejects
> unknown options with a CRITICAL diagnostic + exit 1 (previously silently ignored → exit 0; e.g.
> `--bogus`). Verified on dev + release (matrix in 0024).
**Points to ADR:** 0010 (Q5/Q6) · 0009 (M3 observability line preserved) · 0008 (CLI)
**Rolls under spec:** ADR-0010 (M4 spec embedded; execution tickets only in docs/ticket/)