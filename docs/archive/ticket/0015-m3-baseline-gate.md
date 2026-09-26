# 0015 — M3 baseline verification & clean-build gate

**What to build:**

A single question to answer before starting M3: **is M2 in a clean, freezeable state that M3
can safely build on?** This is an entry gate / baseline fence, not a feature ticket and not a
cleanup invitation. It verifies and records the current state; the only edits allowed are docs
where the text is *objectively inconsistent* with the running system.

Roles: 0015 = baseline fence · 0016 = fixture freeze · 0017 = runtime observability ·
0018 = human category validation.

## Scope

**Verifies:**
- `docs/CHANGELOG.md` / `docs/CONTEXT.md` / `README.md` milestone/status consistency
  (M0–M2 complete; next milestone = **M3 Noto Vertical Slice**).
- Completed-ticket / archive consistency (`docs/ticket/` empty; `docs/archive/ticket/` = 0001–0014).
- No stale `pending` / `blocked` / `in-progress` wording about the M2 chain.
- The 6 frozen M2 assets described consistently (smile / warm / cool / crown / top_hat / sunglasses).
- Test-suite count matches docs (4 suites).
- Clean-build gate: fresh `configure` + full `build` + `ctest` green on presets
  `dev`, `dev-clean`, `release`.

**Explicitly out of scope (owned by 0017):**
- No change to runtime diagnostics, smoke markers, `--asset` behaviour, window title,
  or `--list-assets`.

**Smoke-marker rule:** README's `# prints M0_OK / M1_DONE` describes the *current* output and is
accurate; do NOT pre-edit it for expected 0017 output. If a marker line is objectively wrong for
the current binary, fix it; if it will only change after 0017, leave it and record `Deferred to
0017 runtime diagnostics`.

## Acceptance

- [x] CHANGELOG/CONTEXT/README state matches the running system (M0–M2 complete; 6 assets described
      correctly; no active tickets; next = M3).
- [x] No `pending`/`blocked`/`in-progress` residue about the M2 chain across docs
      (archived-ticket "Blocked by:" and the official-mesh "PENDING" watch item are history/legit).
- [x] Fresh configure + build + `ctest` green on `dev`, `dev-clean`, `release` (recorded as
      baseline evidence).
- [x] Any objectively-wrong current marker fixed; any 0017-only changes left alone and noted
      `Deferred to 0017`.

**Blocked by:** nothing (M3 entry gate)
**Status:** completed

> Baseline evidence (2026-09-26): `cmake --preset {dev,dev-clean,release}` + `cmake --build` +
> `ctest` → 4/4 passed on all three presets. No stale wording found; README smoke comment
> (`prints M0_OK / M1_DONE`) matches current binary output (M0_OK, M1_OK, M2_INFO, M1_DONE appear;
> M2_OK on `--demo`); window title / runtime identity wiring left to 0017 — no pre-emptive edit
> per the smoke-marker rule.
**Points to ADR:** 0008 (docs lifecycle), M2 completion in CHANGELOG
**Rolls under spec:** 0019 (M3 milestone spec)