# 0034 — M5 viewport/input contract hardening: containment helper's role and gate-read ordering

**Source:** review of the 0028 implementation against ADR-0011 (findings ② and ③ of the 0028
confirmation review).
**Status:** ready-for-agent
**Blocked by:** 0028

0028 is implemented and green, but two of its contracts are recorded only as code comments, or are
recorded *wrongly* in ADR-0011. Both are cheap to fix now and expensive to discover later: each one
is a trap for the next person who "tidies" the frame loop. This slice is **documentation and
comment only** — it must not change behaviour.

## ① `ViewportRect::contains()` is not part of input ownership

ADR-0011 Q0 currently says the rect drives "cursor window→viewport-local mapping + containment".
The 0028 implementation does not do that, and deliberately so: viewport input ownership is decided
by the UI toolkit's hover and item state, never by converting a cursor position and testing it.

**The decision:** input ownership stays on `ImGui::IsWindowHovered()` + the item arbitration of
ADR-0011 Q10. The rect is not on that path.

Rationale: hover already answers "is the cursor inside this panel" exactly, in the same coordinate
space ImGui itself uses. Routing ownership through a coordinate conversion adds a DPI-correctness
dependency to input arbitration for no benefit — and a wrong answer there is a permanently dead
camera, the failure mode Q10 already exists to prevent.

`contains()` therefore stays in the app layer as a headless-covered predicate, **reserved** for
consumers that genuinely need a point-in-view test in framebuffer space (0026's cursor-baseline and
gesture policy work, future picking). It is deliberately not dead code, and it is deliberately not
in the ownership path.

## ② The gate inputs must be read before the viewport panel submits its own items

`ImGui::Dummy()` calls `ItemAdd()`. So if the four gate inputs are read *after* the 3D panel
reserves its content area, `ImGui::IsAnyItemHovered()` reports an item under the cursor on **every**
frame the cursor is over the 3D view, and the camera is dead from then on — silently, with all tests
still green, because the predicate itself is correct and only its *evaluation site* is wrong.

**Locked rule:** `IsWindowHovered`, `IsAnyItemHovered`, `IsAnyItemActive` and the popup check are
read at the top of the 3D panel block, before any content is submitted.

If the 3D panel ever gains real widgets, they must come *after* those reads, and such a widget then
arbitrates against the camera on purpose.

## Contracts

- Viewport input ownership is `ImGui::IsWindowHovered(...)` + Q10's item arbitration. It must not be
  routed through `ViewportRect::contains()` or through any cursor-position conversion.
- `contains()` takes **framebuffer pixels** (GL space, lower-left origin) — not window coordinates
  and not logical/scaled pixels. A future consumer holding a window-space cursor position must
  convert it with the same origin + scale transform that `toFramebufferRect` applies before
  testing.
- The gate reads precede the viewport panel's content submission (②). The code comment at the
  `Dummy()` call site keeps pointing here.
- ADR-0011 is amended by **appending** to its Amendment section (per
  `docs/DOCUMENTATION-LIFECYCLE.md`), not by silently rewriting the decision.
- **No behaviour change.** No new semantics, no new test, no change to the gate, the rect, or the
  conversion helper.

## Acceptance

- [ ] ADR-0011 amended: Q0 no longer claims the rect drives cursor mapping/containment; it states
      that ownership uses toolkit hover and that `contains()` is reserved, framebuffer-space, and
      not on the ownership path
- [ ] ADR-0011 amended: the gate-read-before-content ordering is added as a locked constraint
- [ ] `docs/ticket/0028-…` contracts updated so they point at both constraints instead of
      implying containment is on the ownership path
- [ ] `CONTEXT.md`'s ADR-0011 summary reflects both constraints
- [ ] `contains()`'s header comment states its input space and reserved role; the `Dummy()` call
      site's comment cites 0034 ②
- [ ] dev + release build clean and `ctest` still 17/17 — the two existing suites
      (`app-viewport-rect-math`, `app-input-gate-contract`) already cover the affected semantics, so
      no new suite is added

**Points to ADR:** 0011 (Q0 viewport/coordinate contract, Q10 input ownership; Amendment section) ·
0004 amendment (viewport ≠ framebuffer).
