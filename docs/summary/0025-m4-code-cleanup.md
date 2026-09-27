# Execution Summary: 0025-m4-code-cleanup

- **Date**: 2026-09-27
- **Spec/ADR Path**: `N/A` (rolled under ADR-0008 module boundaries + ADR-0010 Q8 CLI regression contract)
- **Ticket Path**: `docs/archive/ticket/0025-m4-code-cleanup.md` (was `docs/ticket/0025-m4-code-cleanup.md`)
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 1 commit
- **Files Changed**: 10 files (`+871` / `-472`)

## 📝 Key Changes
- **① CLI parsing extracted + headless coverage.** `Options`/`parseOptions` moved out of
  `main.cpp`'s anonymous namespace into `src/app/cli_options.{hpp,cpp}`, so the CLI contract
  (ADR-0010 Q8) is unit-testable in-process instead of only through a forked binary:
  `tests/app/cli_options_test.cpp` (12 blocks / 40 checks asserting `Options` **content**) plus
  the new CTest `app-cli-parse-contract`. Layering kept honest — parsing is a pure *syntax*
  contract, `--asset`+`--demo` exclusion stays pre-GL in `main()`, unknown-asset resolution stays
  in the registry layer, and the existing optional/flag shape was preserved (no mode enum).
- **④ `run()`'s GL scope flattened.** The ~200-line bare `{ ... }` block became
  `int renderScene(GLFWwindow*, const Options&)`. GL-owning objects (shader, registry →
  Mesh/Texture, renderer, camera) are locals of the helper, so the ticket-0012 lifetime rule is
  now *structural* — they die with the function, before `glfwDestroyWindow`. `run()` keeps the
  GLFW lifecycle, the capability gate and the debug-GL block.
- **②/③/⑤/⑧ verification and records.** The uniform-location cache was verified against its three
  invariants and left byte-identical (there is no `reload()`; the cache is cleared whenever the
  program handle is invalidated). `loadManifestOnly()` was recorded as the public production API
  behind `--list-assets` (removal / `testing`-namespace / `#ifdef` gating explicitly rejected) with
  the restructuring question handed to 0027. `noto3d_asset` → `noto3d_renderer` stays **PUBLIC**,
  and the real CMake target graph was written into the ticket.
- **Follow-ups split out and deferred.** 0026 (M5 input/interaction config) and 0027 (asset
  CPU/GPU architecture spike) now own the deferred design questions; README, CHANGELOG and
  CONTEXT were updated in the same change.

## 🧪 Verification
- **Typecheck**: 🟢 Pass (`cmake --build` clean, no new warnings; dead includes removed)
- **Tests**: 🟢 Pass — `ctest` **15/15 on dev + release** (suite 14→15)
- **0024 regression matrix re-run**: 🟢 5 valid invocations → exit 0, 6 invalid → exit 1
  (`--asset=nope`, `--asset=smile --demo`, `--bogus`, `--frames=abc`, `--asset=`, `--frames=-1`)
- **Runtime evidence re-verified**: `M0_OK` → `M4_INFO rendering scene 'm4_vertical_scene' entities=3`
  → `M1_DONE rendered 1 frames`; `--demo` → `M2_OK ... (0009 E2E)` + `entities=1`; xprop
  `WM_NAME` for all three entry modes unchanged.
- **Red-first TDD**: the suite could not configure before the implementation existed
  (`No SOURCES given to target: noto3d_app`), and the first green attempt failed 10 checks —
  the assertions were live before the parser did.

## 🔍 Code Review Findings
- **Standards**: no hard violation. Fixed the includes the extraction made dead
  (`<cstring>`, `<optional>`, plus two already-dead ones); matched the sibling tests'
  `using namespace noto3d::app;` style. Deliberately left alone and recorded: the duplicated
  `--demo`/`--asset` cascade + `Resolver` lambda inside `renderScene()` (verbatim 0022 contract
  code), `frameLimit == -1` doubling as unlimited-sentinel and parse-failure marker, and
  `bool parseOptions(..., Options&)` leaving `out` unspecified on failure — the ticket explicitly
  kept that shape.
- **Spec**: no missing requirement, no scope creep, top-level invariant held (no diff under
  `src/scene`, `src/asset`, `src/renderer`, `draw_list.*`, `camera.*`). One oversight fixed: the
  parser suite now links `noto3d_core` only and compiles `src/app/cli_options.cpp` directly, so
  `tests/app/` genuinely needs no GL (no glad linkage, no GL symbols in the binary). A dangling
  `ADR-0012` citation (0012 is an archived *ticket*) was removed from the ticket, CHANGELOG,
  CONTEXT and the `main.cpp` comment. Two reported findings were checked and were not defects:
  the test helper already used the house 2-arg `check()` shape, and unknown-asset **content** was
  already asserted (`--asset=nope` → `assetId == "nope"`).

## 📦 Recent Commits
- `1d9768e` refactor(app): extract CLI parsing and flatten run() GL scope (0025)
