# Execution Summary: 0039-asset-cpu-split-closeout

- **Date**: 2026-09-29
- **Spec/ADR Path**: `docs/adr/0012-asset-cpu-gpu-boundary.md` (tracer bullet 4; the close of the
  0027 spike chain 0036–0039)
- **Ticket Path**: `docs/archive/ticket/0039-asset-cpu-split-closeout.md` (complete and archived)
- **Fixed point for review**: HEAD `4ec08b6` (the 0038 commit)
- **Branch**: `main`

---

## 📌 Overview

- **Commits**: 1 (documentation-only)
- **Files changed**: 13 (+504 / −68), including 4 `git mv`s into `docs/archive/ticket/`
- **Production code changed**: **none.** This ticket's entire subject is the question "did the
  refactor change anything observable?" — so it adds no code by design.
- **Ticket status**: ready-for-agent → **complete** (evidence recorded in the ticket, chain archived)

## 📝 Key Changes

1. **The final target graph, read out of CMake rather than transcribed from the ADR.** A
   `cmake_language(DEFER …)` probe dumps `LINK_LIBRARIES` / `INCLUDE_DIRECTORIES` /
   `COMPILE_DEFINITIONS` per target at the end of configure, so the recorded table is the build
   system's own answer. It covers the 10 targets in the transitive closure of the asset boundary and
   matches ADR-0012 ② — including the 0037 amendment (`glm::glm` PUBLIC, `spdlog::spdlog` PRIVATE,
   `spdlog` linked *directly* so `noto3d_core`'s PUBLIC `lib/glad/include` cannot put glad back on
   the CPU target's include path).

2. **The top-level invariant verified two ways, one of them structural.** *Structurally:* the whole
   0036–0039 chain touched **no** file in `src/scene/`, `src/app/draw_list.*`, `src/app/editor.*`
   or `src/ui/`, and every public declaration of `AssetRegistry` is byte-identical to pre-chain
   (`1a57a3f`) — only the private side changed, as intended. So the M4 contracts (registry sole
   authority, `Scene` pure data, the `collectDrawItems` seam, the borrowed `DrawItem::model`) never
   had an opportunity to move, which is a stronger statement than "still passing".
   *Behaviourally:* the full regression below.

3. **The manual tier-B `xprop` check was actually executed, not just re-cited.** `m5-close-matrix.sh`
   keeps `WM_NAME` in its manual tier, so the three frozen titles were read live on a real display
   and all three matched exactly. Two obvious approaches fail on this box and the working
   incantation is recorded in the ticket for the next person.

4. **Close-out audit → ticket 0040.** Two ADR-0012 invariants the ADR states as contracts while
   nothing in the repo checks them. The interesting one: `registry_test.cpp:400` contains
   `check(reg.model(e.id) == nullptr || true, …)` — a vacuous assertion whose comment claims exactly
   the ADR-0012 ① guarantee (`material.texture` always null after a CPU load). A test that reads as
   though it verifies an invariant and provably cannot fail is worse than no test, because it
   suppresses the question. Filed, not fixed, since this ticket is scoped "ideally no new code" and
   adding assertions would have widened the "zero observable change" claim it exists to make.

## 🧪 Verification

- **Clean build**: 🟢 dev and release, from fresh build dirs, **0 errors / 0 warnings** under
  `-Wall -Wextra -Wpedantic`
- **ctest**: 🟢 **22/22** in both clean build dirs
- **Close matrix**: 🟢 `scripts/m5-close-matrix.sh` ALL PASS on the clean dev dir, and ALL PASS on
  the clean release dir with `--release`
- **CLI exit codes**: 🟢 `--list-assets` → 0; `--bogus`, `--frames=-1`, `--frames=abc`,
  `--asset=nope`, `--asset=nope --demo` → 1
- **`--list-assets` creates no GL context**: 🟢 zero `M0_INFO` / `M0_OK` / `M1_OK` / shader lines
- **Entry-mode log lines**: 🟢 `entities=3` (default), `entities=1` (`--asset=crown`),
  `entities=1` (`--demo`) — identical to the frozen contract
- **`xprop` `WM_NAME`**: 🟢 all three titles exact
- **GL-free proof**: 🟢 CPU test link line has zero `noto3d_renderer` / `glad` / `libGL`; `nm`
  defined-and-undefined GL symbol count 0/0; the configure-time guards still fail a fresh
  `cmake -S . -B …` (exit 1) when `#include <glad/gl.h>` is injected, naming the header
- **Doc hygiene**: 🟢 every `docs/{ticket,summary}/NNNN-*.md` reference resolves except five
  pre-existing dead ones, recorded rather than silently fixed

## 🔍 Code Review Findings

Two axes, parallel sub-agents, fixed point `4ec08b6`. Both found real problems in work I had just
written, which is the point of running it on a documentation change too.

- **Standards — 3 hard, all fixed.** (1) `CONTEXT.md:27` still said "all 32" archived tickets while
  the same change made it 36. (2) `CONTEXT.md:332` pointed at `docs/ticket/` for the four tickets the
  change had just moved to `docs/archive/ticket/`, contradicting the section header three lines
  above. (3) `docs/summary/0027-…md:65` still carried `21/21` — the same class of stale count the
  repo had already fixed once inside ADR-0012 during 0038. Documentation-lifecycle treats
  `CONTEXT.md` as *current implementation truth*, so a number the change itself falsifies is a hard
  violation, not a nit.
- **Standards — judgement calls.** Duplicated prose across ADR/CONTEXT/CHANGELOG for the 0037
  rationale (accepted: the hierarchy mandates ADR+CONTEXT parity, and the CHANGELOG copy is
  conventional). No `docs/summary/0039` (fixed — this file). No CHANGELOG entry of its own for
  0039, merged into the tail of the 0027 spike bullet (fixed — 0039 now has its own bullet).
- **Spec — 3 real accuracy defects in my own evidence, all fixed.** (1) The graph table had 8 of
  the 10 in-scope targets: `noto3d_scene` and `noto3d_ui` were missing, so "noto3d_app /
  noto3d-editor unchanged" was *asserted* rather than *shown*. (2) "The `public:` section is
  byte-identical" was literally imprecise — the compared range ends on the `private:` label, so the
  claim is now stated as "every public declaration" with the re-runnable command. I checked this one
  myself: the diff really is empty and no private member is in range. (3) "13 files over
  `src/`, `tests/`, `scripts/`, `assets/`" was wrong — that range yields 12; the 13th is
  `CMakeLists.txt`, which is outside those paths.
- **Spec — both invariant amendments judged legitimate, not rationalisations.** 0038 adds exactly
  one `add_test` (21→22, none removed) and ADR-0012's 0037 amendment really does add glm + spdlog.
- **Spec — the agent found a better version of my own 0040 finding** (the vacuous `|| true`), and
  corrected a mis-attribution in my dead-link note. 0040 and the note were updated accordingly.

## 📦 Recent Commits

- `4ec08b6` `feat(asset): split AssetRegistry by definition so CPU tests and --list-assets link no GL` (0038)
- `db5db54` `refactor(asset): split GL-free CPU core into a noto3d_asset_cpu target` (0037)
- `840ea40` `refactor(vertex): relocate renderer::Vertex to a GL-free header and update references` (0036)

## ➡️ Next

- **0040** — the two ADR-0012 invariants nothing asserts, including the vacuous
  `check(... || true)`. Code-quality, does not block M6.
- **Open, unrelated to ADR-0012:** 0026 (M5-adjacent; blocked by 0028, the input-ownership gate)
  and 0035 (declaration-only header conformance, `ready-for-agent`). 0033 is *not* open — it was
  completed and archived during this chain's regression work.
- M6 Save/Load may proceed on the determined ownership model: **ADR-0012 is closed.**
