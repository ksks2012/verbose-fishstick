# 0017 — M3 asset selection & diagnostics

**What to build:**

Close the runtime observability gap found in M2: with `--asset=warm` the binary renders, but the
selected asset's identity is invisible in stdout (only the default path prints
`M2_INFO rendering manifest asset 'smile'`) and the window title is a hard-coded
`noto3d-editor (M2)`. Users must be able to see, at runtime, which asset is selected and where it
came from — without any new renderer capability (no ImGui, no overlay, no text renderer, no
reference texture loading).

Roles: 0015 = baseline fence · 0016 = fixture freeze · 0017 = runtime observability ·
0018 = human category validation.

## Scope

**A — unified selection diagnostics (both default and `--asset=<id>` go through the SAME path):**
- After selection, print one identity line with asset id, geometry source, and reference path,
  e.g. `M3_INFO  selected asset '<id>' geometry=placeholder reference=references/noto3d/….png`.
- The background default-selection branch must not keep its own special logging.

**B — dynamic window title:**
- `glfwSetWindowTitle(window, "<id> [placeholder] | noto3d (M3)")` — existing GLFW API, no text
  rendering. Title reflects the selected asset + geometry source.

**C — `--list-assets` diagnostic CLI (NOT a new asset-pipeline capability):**
- Reads the existing `registry.entries()`; prints id / geometry source / reference path per
  asset, e.g. `M3_ASSETS version=1` header then one line per asset.
- **Exits without rendering** — parse args → load manifest → print → exit before GLFW context
  creation (use the existing `loadManifestOnly` path; do not restructure `main()`'s GL lifecycle,
  do not invent new enumeration APIs or manifest schema).

## Acceptance

- [x] `--asset=<id>` prints selected identity (id + geometry source + reference path)
- [x] Default selection prints the same identity line via the same code path
      (old `M2_INFO rendering manifest asset` special-case removed)
- [x] Window title reflects `<id> [placeholder] | noto3d (M3)`
- [x] `--list-assets` enumerates the 6 frozen assets with geometry source + reference, exits
      non-rendering with exit 0
- [x] Existing `--frames=`, `--demo`, exit-code behaviour unchanged (verified: valid → 0,
      `--frames=abc`/`--asset=nope` → non-zero)
- [x] No new GL/render/text capability introduced

**Blocked by:** 0016 (complete)
**Status:** completed

> Evidence (2026-09-26): `--list-assets` → `M3_ASSETS version=1` + 6 id lines + `end count=6`,
> exit 0, before any GL context (loadManifestOnly + entries() only). `--asset=warm --frames=3` →
> `M3_INFO selected asset 'warm' geometry=placeholder reference=references/noto3d/…png`, exit 0.
> Default `--frames=3` → same identity line for 'smile', exit 0 (unified path; old M2_INFO line
> removed). Window title verified via `xprop WM_NAME` = `top_hat [placeholder] | noto3d (M3)`.
> `--demo` still `M2_OK (0009 E2E)`; invalid flags still exit 1. dev + release ctest 5/5.
**Points to ADR:** 0008 (CLI/diagnostics; no new renderer surface)
**Rolls under spec:** 0019 (M3 milestone spec)