# 0035 — declaration-only conformance: move remaining inline definitions out of headers

**Source:** ADR-0008 (C++20 code contract, "headers are declaration-only") surfaced by ticket
0028's refactor; raised while assessing 0030.
**Status:** ready-for-agent
**Blocked by:** None — independent of the M5 chain, and deliberately does **not** block 0030

ADR-0008 requires that headers contain declarations only, with every definition in a `.cpp`. Ten
definitions still violate this: eight class-body accessors (implicitly inline) plus two free
`inline` functions in `transform_edit.hpp` added by 0030. Eight are pre-existing (renderer layer
`camera.hpp`/`mesh.hpp`/`texture.hpp`, from tickets 0006/0016); one was introduced by 0028's own
`src/ui/` extraction; two landed with 0030. They are small, mechanical to fix, and currently cost a
reviewer a decision on every header touched.

## What it delivers

- Every function definition in `src/**/*.hpp` lives in a corresponding `.cpp`. The rule stops being
  a documented intention with ten known exceptions.
- The two dead accessors are **removed** rather than moved — moving code that nothing calls only
  relocates the problem.
- The false claim recorded in ticket 0028 is corrected (see *Records to correct* below).

## Scope — the complete inventory

A line-based scan of all 26 headers (`src/**/*.hpp`) for function *definitions* — a parameter list
closed by `)` followed only by qualifiers and then `{` — returns exactly these ten, and no others.
Eight are class-body accessors (implicitly inline); two are free `inline` functions new in 0030.

| File | Line | Definition | Call sites | Action |
|---|---|---|---|---|
| `src/renderer/camera.hpp` | 51 | `target()` | 9 | move to `camera.cpp` |
| `src/renderer/camera.hpp` | 52 | `yaw()` | 5 | move to `camera.cpp` |
| `src/renderer/camera.hpp` | 53 | `pitch()` | 7 | move to `camera.cpp` |
| `src/renderer/camera.hpp` | 54 | `distance()` | 7 | move to `camera.cpp` |
| `src/renderer/mesh.hpp` | 53 | `vao()` | 1 | move to `mesh.cpp` |
| `src/renderer/mesh.hpp` | 54 | `indexCount()` | 1 | move to `mesh.cpp` |
| `src/renderer/texture.hpp` | 34 | `id()` | **0 — dead** | **delete** |
| `src/ui/panel_render.hpp` | 28 | `active()` | **0 — dead** | **delete** |
| `src/app/transform_edit.hpp` | 17 | `degreesToRadians()` | 3 | move to `transform_edit.cpp` |
| `src/app/transform_edit.hpp` | 21 | `radiansToDegrees()` | 3 | move to `transform_edit.cpp` |

`camera.cpp`, `mesh.cpp`, and `transform_edit.cpp` already exist, so every "move" is a cut-and-paste
with no new build-system work. Note the two free functions are **live** — `transform_edit.cpp:11,15`
call them — so they are moved, never deleted. Source count is 26 headers at time of writing; the
ticket applies to any header present when worked.

## Contracts this slice must honour

- **No behaviour change.** These are pure forwarding accessors and simple unit conversions; moving
  a body cannot alter semantics. The scan above is the record of what must keep compiling.
- **No signature changes.** Return types, parameter lists, `const`/`noexcept` qualifiers and
  `[[nodiscard]]` attributes stay exactly as they are. This is a relocation ticket, not an API
  review.
- **Delete, do not move, the two dead accessors.** `Texture::id()` and `PanelViewport::active()` have
  zero call sites. Keeping them would preserve the violation for no benefit.
- **The invariant must be mechanically checkable.** A grep that only matches explicit `inline` and
  `constexpr` misses the eight class-body accessors (a member defined inside a class body is
  *implicitly* inline), and catches the two `transform_edit.hpp` free functions but would miss any
  class-body sibling. The check must match a definition *shape* (see below), not a keyword — it is
  what found all ten.
- **Out of scope:** header *layout* (include order, `std::` qualification, `#pragma once` vs
  include guards) is a separate question and is not reopened here.

## Acceptance

- [ ] `src/**/*.hpp` contains no function definition; a definition-shaped scan over all 26 headers
      returns zero hits
- [ ] The eight live definitions are in their `.cpp`, byte-identical bodies, unchanged signatures
- [ ] `Texture::id()` and `PanelViewport::active()` are deleted, and no build break follows
- [ ] `CMakeLists.txt` unchanged — no new or removed sources are needed
- [ ] dev + release build 0 error / 0 warning; `ctest` still **19/19** in both
- [ ] The eight live definitions' existing call sites still compile and their tests still pass, with
      no test modified to accommodate the change
- [ ] A repeatable definition-shaped check is recorded in the ticket, so the next violation is caught
      by a command rather than by review
- [ ] ADR-0008's `docs/CONTEXT.md` wording is re-read and confirmed accurate (no doc change expected —
      the rule is unchanged, the code is what was wrong)
- [ ] No test assertion, public signature, or runtime log line changed

## The check that must be recorded

The scan that produced the table above, in a form an agent can paste and re-run. Its two essential
properties, both learned by getting them wrong first:

1. It must key on a **definition shape** — `name( params ) qualifiers {` — not on the keywords
   `inline` / `constexpr`. A keyword-based grep reports 0 violations and is wrong: eight of the ten
   (all the class-body accessors) are *implicit* inlines that carry no keyword at all.
2. Its qualifier group must be **repeatable** (`(?:const|noexcept|...)*`, not `?`). A single-optional
   group silently drops `const noexcept` definitions — which is every accessor in this ticket — and
   reports a false clean.

## Records to correct

Ticket 0028's *Verification* section claims "All headers are declaration-only — every definition
lives in a `.cpp`". That claim was made from a keyword-based grep and is **false**; this ticket is
the correction path. The claim should be amended to state the rule plus the then-known ten
exceptions, or simply point here.

Ticket 0028's *Known deviation from ADR-0011 Q5* (`noto3d_ui` as a separate static library) remains
open and unrelated; this ticket does not resolve it.

## A caution for whoever applies this

`camera.hpp`'s four accessors are the ones with real call sites (5–9 each, mostly in
`tests/renderer/camera_test.cpp`). They are currently inline, so callers in other translation units
may already be relying on the implicit inline definition. Moving a body to `camera.cpp` is safe only
if `camera.cpp` is in the same link target as every caller — it is today, but verify rather than
assume, or the failure will show up as a link error in a test binary rather than in the renderer.
The same caution applies to the two conversions moving to `transform_edit.cpp`, whose callers are
`transform_edit.cpp` itself and the new `app-transform-edit` test suite (both in `noto3d_app`
targets).

This ticket was authored against a working tree that already contained 0030's staged
`transform_edit.*`; the "0 warning" baseline (see the 0028 warning below) can only be reached once
0030 is committed and built together with this ticket's changes.

**Points to ADR:** 0008 (C++20 code contract, declaration-only headers, `-Wall -Wextra -Wpedantic`
with opt-in `-Werror`).
