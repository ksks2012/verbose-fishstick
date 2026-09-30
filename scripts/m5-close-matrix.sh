#!/usr/bin/env bash
# M5 close / integration-regression matrix (ticket 0033), mirroring 0024's role
# for M4 (ADR-0010 Q8 / ADR-0011 Q8).  Two tiers:
#
#   A — automated: CLI exit-code matrix + structured runtime-log assertions over
#       every CLI entry mode, plus static contract checks.
#   B — manual, platform-specific (xprop WM_NAME, docking, editing pass,
#       interaction ownership): recorded in the ticket, not scripted here.
#
# Usage:  scripts/m5-close-matrix.sh <build-dir> [--release]
#   <build-dir>  e.g. build/release
#   --release    assert GL-mode exit 0; otherwise GL modes assert log lines
#                only (the dev preset's LeakSanitizer reports a known Mesa
#                llvmpipe 112-byte leak — see docs/CONTEXT.md — so dev GL runs
#                exit 1 while still emitting the correct structured log).
#
# Exit: non-zero if any assertion fails.  Prints one line per check.

set -u

BUILD_DIR="${1:?usage: scripts/m5-close-matrix.sh <build-dir> [--release]}"
shift
RELEASE=0
if [ "${1:-}" = "--release" ]; then RELEASE=1; fi

BIN="${BUILD_DIR}/noto3d-editor"
if [ ! -x "$BIN" ]; then echo "FAIL  binary missing: $BIN"; exit 1; fi

FAILS=0
check() {  # check <label> <cond>
    if [ "$2" = "pass" ]; then echo "PASS  $1"; else echo "FAIL  $1"; FAILS=$((FAILS+1)); fi
}

run_bin() {  # run_bin <outfile> <expected-exit> <label> <args...>
    local out="$1" want="$2" label="$3"; shift 3
    "$BIN" "$@" >"$out" 2>&1
    local got=$?
    if [ "$RELEASE" = "1" ]; then
        check "$label (exit $got)" "$([ "$got" = "$want" ] && echo pass || echo fail)"
    fi
}

have() {  # have <outfile> <needle> <label>
    grep -q "${2:?needle}" "$1" && check "$3" pass || check "$3" fail
}
have_order() {  # have_order <outfile> <a> <b> <label> — a must appear before b
    local a_ln b_ln
    a_ln=$(grep -n "${2}" "$1" | head -1 | cut -d: -f1)
    b_ln=$(grep -n "${3}" "$1" | head -1 | cut -d: -f1)
    if [ -n "$a_ln" ] && [ -n "$b_ln" ] && [ "$a_ln" -lt "$b_ln" ]; then
        check "$4" pass
    else
        check "$4" fail
    fi
}

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "── tier A: CLI exit-code matrix ──────────────────────────────────────────"

# ============ pre-GL modes (no display needed) ============
run_bin "$TMP/list" 0 "pre-GL: --list-assets (exit 0)" --list-assets
have "$TMP/list" "M3_ASSETS version=1" "list-assets: version line"
have "$TMP/list" "end count=6"          "list-assets: 6 frozen assets"

run_bin "$TMP/bad"    1 "pre-GL: invalid flag (exit 1)" --no-such-flag
run_bin "$TMP/negl"   1 "pre-GL: --frames=-1 (exit 1)"  --frames=-1
run_bin "$TMP/abc"    1 "pre-GL: --frames=abc (exit 1)" --frames=abc
run_bin "$TMP/empty"  1 "pre-GL: --asset= (exit 1)"     --asset=
run_bin "$TMP/excl"   1 "pre-GL: --asset+demo (exit 1)" --asset=smile --demo

# ============ GL modes (need DISPLAY; log lines asserted always) ============
run_bin "$TMP/default" 0 "GL: default scene (exit 0)" --frames=8
have         "$TMP/default" "M0_INFO  numeric"      "default: M0 identity"
have         "$TMP/default" "M0_OK context"         "default: M0_OK"
have         "$TMP/default" "M4_INFO rendering scene 'm4_vertical_scene' entities=3" \
                                                      "default: M4_INFO 3 entities"
have         "$TMP/default" "M1_DONE rendered 8 frames" "default: M1_DONE"
have_order   "$TMP/default" "M0_OK" "M4_INFO" "default: M0_OK before M4_INFO"
have_order   "$TMP/default" "M4_INFO" "M1_DONE" "default: M4_INFO before M1_DONE"

run_bin "$TMP/asset" 0 "GL: --asset=smile (exit 0)" --asset=smile --frames=8
have         "$TMP/asset" "M3_INFO  selected asset 'smile' geometry=placeholder" \
                                                     "asset: M3_INFO identity"
have         "$TMP/asset" "M4_INFO rendering scene 'smile' entities=1" \
                                                     "asset: M4_INFO 1 entity"
have         "$TMP/asset" "M1_DONE rendered 8 frames" "asset: M1_DONE"
have_order   "$TMP/asset" "M3_INFO" "M4_INFO" "asset: M3_INFO before M4_INFO"

run_bin "$TMP/demo" 0 "GL: --demo (exit 0)" --demo --frames=8
have         "$TMP/demo" "M0_OK context"        "demo: M0_OK"
have         "$TMP/demo" "M2_OK   demo_checker fixture rendered with diffuse texture (0009 E2E)" \
                                                     "demo: M2_OK (0009 E2E)"
have         "$TMP/demo" "M4_INFO rendering scene 'demo_checker' entities=1" \
                                                     "demo: M4_INFO demo_checker 1 entity"
have         "$TMP/demo" "M1_DONE rendered 8 frames" "demo: M1_DONE"
have_order   "$TMP/demo" "M0_OK" "M2_OK"   "demo: M0_OK before M2_OK"
have_order   "$TMP/demo" "M2_OK" "M4_INFO" "demo: M2_OK before M4_INFO"
have_order   "$TMP/demo" "M4_INFO" "M1_DONE" "demo: M4_INFO before M1_DONE"

echo "── tier A: structured M5 runtime-log assertions (headless suites) ────────"
# The M5 editing ops (selection, asset assignment, transform commits, create/
# destroy) are UI-driven and cannot be exercised over the CLI entry modes; the
# close matrix asserts their structured-log formats via the headless app suites
# (same log canary the entity-edit/transform-edit suites added for 0033):
#   M5_SEL   — tests/app/editor_test.cpp
#   M5_ASSET — tests/app/asset_picker_test.cpp  (offered/hidden counts, assigned)
#   M5_XFRM  — tests/app/transform_edit_test.cpp (incl. dual-unit rotation line)
#   M5_ENTITY— tests/app/entity_edit_test.cpp   (create/destroy, count)
for suite in noto3d-editor-tests noto3d-asset-picker-tests \
             noto3d-transform-edit-tests noto3d-entity-edit-tests; do
    if [ -x "${BUILD_DIR}/${suite}" ]; then
        "${BUILD_DIR}/${suite}" >"$TMP/suite" 2>&1
        check "headless: ${suite} ALL PASS" \
              "$(grep -q 'ALL PASS' "$TMP/suite" && echo pass || echo fail)"
    else
        check "headless: ${suite} present" fail
    fi
done

echo "── tier A: static contract checks ────────────────────────────────────────"
# Single resolution seam: collectDrawItems is the only render-path entry; the
# only registry.model() uses are the resolver lambdas passed to it.
# The registry-lookup half was `-eq 2` until ticket 0045, which counts RESOLVER
# LAMBDAS rather than the invariant: the intent is "the registry model lookup is
# reachable only through a resolver handed to collectDrawItems", so that stays
# true whatever the entry-mode count is.  0045 could not honour the ticket's
# "existing rows unchanged" here -- adding a third entry mode is exactly what
# invalidates a literal 2.  The check is now: at least one lookup exists (or the
# assertion would be vacuous), and EVERY one of them is the resolver's return
# statement, so none has crept into a render path.
if [ "$(grep -rl "collectDrawItems(" src/app/*.cpp | wc -l)" -ge 1 ] && \
   [ "$(grep -c "registry.model(" src/app/editor_entry.cpp)" -ge 1 ] && \
   [ "$(grep -n "registry.model(" src/app/editor_entry.cpp \
        | grep -vc "return registry.model(")" -eq 0 ]; then
    check "seam: collectDrawItems sole render entry + resolver-only registry.model calls" pass
else
    check "seam: collectDrawItems sole render entry + resolver-only registry.model calls" fail
fi
# No GPU-resource ownership in the app layer: Model is borrowed (pointer), never owned.
if grep -q "const renderer::Model\* model;" src/app/draw_list.hpp; then
    check "seam: DrawItem::model remains a borrowed pointer (no GPU ownership)" pass
else
    check "seam: DrawItem::model remains a borrowed pointer (no GPU ownership)" fail
fi
# App layer links neither ImGui nor GLFW (ADR-0011 Q5) — CMake arch-guard also
# exists at configure time; this greps the link line as a belt-and-braces check.
if grep -q "target_link_libraries(noto3d_app" CMakeLists.txt && \
   ! sed -n '/target_link_libraries(noto3d_app/,/)/p' CMakeLists.txt | grep -q "imgui\|glfw"; then
    check "seam: noto3d_app links neither ImGui nor GLFW" pass
else
    check "seam: noto3d_app links neither ImGui nor GLFW" fail
fi

echo "──────────────────────────────────────────────────────────────────────────"
if [ "$FAILS" -eq 0 ]; then
    echo "M5 close matrix: ALL PASS"
    exit 0
else
    echo "M5 close matrix: ${FAILS} FAILURE(S)"
    exit 1
fi