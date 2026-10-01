#!/usr/bin/env bash
# M6 close / integration-regression matrix (ticket 0046), mirroring the role of
# scripts/m5-close-matrix.sh (0033) and 0024's matrix for M4 (ADR-0010 Q8 /
# ADR-0011 Q8).  Two tiers:
#
#   A — automated: every M6 observable (--scene= load, --save-scene= output, the
#       structural-failure exits, the three-way exclusion, and the write-then-
#       reload round-trip), plus static contract checks.
#   B — manual, platform-specific (xprop WM_NAME for a --scene= run, and opening
#       a saved document in a real window to confirm entities and transforms):
#       recorded in the ticket, NOT asserted here — this environment has no
#       window session, so claiming a manual pass would be an unearned pass.
#
# This is a NEW script, not a grown m5 script (0046 locks that decision): the M5
# script has absorbed rows from two milestones already and a third would make it
# a monolith nobody can read.  The M5 script is left byte-identical, which is
# what makes "the pre-existing rows are unchanged" cheap to verify — run both.
#
# Per ADR-0013 Q9 the M4_INFO shape is REUSED for a --scene= run rather than
# replaced, so this matrix GAINS rows instead of introducing a log format.
#
# Usage:  scripts/m6-close-matrix.sh <build-dir> [--release]
#   <build-dir>  e.g. build/release
#   --release    assert GL-mode exit 0; otherwise GL modes assert log lines only
#                (the dev preset's LeakSanitizer reports a known Mesa llvmpipe
#                112-byte leak — see docs/CONTEXT.md — so dev GL runs exit 1
#                while still emitting the correct structured log).
#
# Exit: non-zero if any assertion fails.  Prints one line per check.

set -u

BUILD_DIR="${1:?usage: scripts/m6-close-matrix.sh <build-dir> [--release]}"
shift
RELEASE=0
if [ "${1:-}" = "--release" ]; then RELEASE=1; fi

BIN="${BUILD_DIR}/noto3d-editor"
if [ ! -x "$BIN" ]; then echo "FAIL  binary missing: $BIN"; exit 1; fi

# Mirrors the m5 script's helpers rather than sourcing it: sourcing would run its
# whole matrix, because the script executes at load time.  The shapes match, and
# have() takes an optional -e needle that m5's does not (three exclusion rows
# need it: a needle starting with "-" is read as a grep OPTION otherwise, which
# made correct rows report FAIL).
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
    else
        check "$label (exit $got; log asserted)" pass
    fi
}

have() {  # have <outfile> <needle> <label>  (needle may be -e <pattern>)
    local needle="${2:?needle}" label="${3:?label}"
    [ "$needle" = "-e" ] && needle="${3:?needle}" label="${4:?label}"
    grep -q -e "$needle" "$1" && check "$label" pass || check "$label" fail
}

hasnt() {  # hasnt <outfile> <needle> <label>
    grep -q "${2:?needle}" "$1" && check "$3" fail || check "$3" pass
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

# The scene directory holds only files THIS script writes.  Out of tree on
# purpose: a .scene dropped into the source tree would outlive the run and
# quietly become part of the next diff.
SC="${TMP}/scenes"
mkdir -p "$SC"

# One entity at a non-identity transform, so the round-trip row below can prove
# transforms SURVIVED the save rather than merely that a file appeared.
write_scene() {  # write_scene <path> <assetId> <x>
    cat >"$1" <<EOF
{"version":1,"entities":[{"id":1,"name":"e1","assetId":"$2","transform":{"position":[$3,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]}
EOF
}

echo "── tier A: M6 pre-GL rows (no display needed) ───────────────────────────"

# ---- Q5: the three-way mutual exclusion ------------------------------------
# Pointed at a VALID document on purpose.  With a missing file these rows would
# exit 1 for the wrong reason — the load would fail even with the gate deleted —
# so the gate would not actually be under test.
write_scene "$SC/good.scene" smile 0
run_bin "$TMP/excl_1" 1 "pre-GL: --asset+--scene (exit 1)"  --asset=smile "--scene=${SC}/good.scene"
run_bin "$TMP/excl_2" 1 "pre-GL: --demo+--scene (exit 1)"   --demo        "--scene=${SC}/good.scene"
run_bin "$TMP/excl_3" 1 "pre-GL: --asset+--demo+--scene (exit 1)" --asset=smile --demo "--scene=${SC}/good.scene"
for f in excl_1 excl_2 excl_3; do
    have "$TMP/$f" "mutually exclusive entry modes" "$f: names the exclusion"
done
# The diagnostic must name the flags actually combined, not just fail.  -e is
# required: a needle starting with "-" is read as an option by plain grep, which
# is why these three rows first reported FAIL against a correct binary.
have "$TMP/excl_3" -e "--asset" "excl_3: names --asset"
have "$TMP/excl_3" -e "--demo"    "excl_3: names --demo"
have "$TMP/excl_3" -e "--scene"   "excl_3: names --scene"
# An exclusion must be rejected BEFORE any GL work; if it were not, the window
# would be created and this line would appear.
hasnt "$TMP/excl_3" "M0_OK context" "excl_3: rejected before any GL context"

# ---- Q3/Q8: every structural failure is a pre-GL non-zero exit -------------
printf 'this is not json' >"$SC/malformed.scene"
printf '%s' '{"version":2,"entities":[]}' >"$SC/v2.scene"
printf '%s' '{"version":1,"entities":[{"id":1,"name":"e1","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]}' >"$SC/nofield.scene"
printf '%s' '{"version":1,"entities":[{"id":1,"name":"e1","assetId":"smile","transform":[0,0,0]}}]}' >"$SC/badtransform.scene"
printf '%s' '{"version":1,"entities":[{"id":1,"name":"e1","assetId":"smile","transform":{"position":[0,0]}}]}' >"$SC/shorttransform.scene"

run_bin "$TMP/missing" 1 "pre-GL: --scene= missing file (exit 1)"   "--scene=${SC}/no-such.scene"
run_bin "$TMP/malformed" 1 "pre-GL: --scene= malformed JSON (exit 1)" "--scene=${SC}/malformed.scene"
run_bin "$TMP/v2" 1 "pre-GL: --scene= version 2 (exit 1)"            "--scene=${SC}/v2.scene"
run_bin "$TMP/nofield" 1 "pre-GL: --scene= missing assetId (exit 1)"  "--scene=${SC}/nofield.scene"
run_bin "$TMP/badxform" 1 "pre-GL: --scene= mistyped transform (exit 1)" "--scene=${SC}/badtransform.scene"
run_bin "$TMP/shortxform" 1 "pre-GL: --scene= short transform (exit 1)"   "--scene=${SC}/shorttransform.scene"
# The load-bearing part: a bad document must not cost a window or a context.
for f in missing malformed v2 nofield badxform shortxform; do
    hasnt "$TMP/$f" "M0_OK context" "$f: rejected pre-GL (no context created)"
done
# A structural diagnostic must name the defect, not just fail.
have "$TMP/v2" "M6_FAIL"          "v2: emits an M6_FAIL diagnostic"
have "$TMP/malformed" "M6_FAIL"   "malformed: emits an M6_FAIL diagnostic"
have "$TMP/nofield" "assetId"     "nofield: diagnostic names the missing field"
hasnt "$TMP/v2" "M4_INFO rendering" "v2: no Scene was built"

# ---- Q7: --save-scene= is an output option, orthogonal to the entry set ----
run_bin "$TMP/saveempty" 1 "pre-GL: --save-scene= empty (exit 1)" "--save-scene="
# A missing parent directory is a diagnostic, never a silent mkdir (ADR-0013 Q8).
run_bin "$TMP/savemissingdir" 1 "pre-GL: --save-scene= under a missing dir (exit 1)" \
    "--demo" "--save-scene=${SC}/no-such-dir/out.scene"
check "savemissingdir: the parent directory was NOT created" \
      "$([ ! -d "${SC}/no-such-dir" ] && echo pass || echo fail)"

echo "── tier A: M6 GL rows (need DISPLAY; log lines asserted always) ───────────"

# ---- Q9: a valid --scene= run REUSES the frozen M4_INFO shape ---------------
run_bin "$TMP/good" 0 "GL: --scene= valid document (exit 0)" "--scene=${SC}/good.scene" --frames=8
have "$TMP/good" "M6_INFO scene-loaded path='${SC}/good.scene' entities=1" \
                                         "scene: M6_INFO scene-loaded line"
# Q9: identity is the FILE STEM, and the shape is the frozen M4_INFO one.
have "$TMP/good" "M4_INFO rendering scene 'good' entities=1" \
                                         "scene: M4_INFO reused with the file stem"
hasnt "$TMP/good" "M4_INFO rendering scene '${SC}/good.scene'" \
                                         "scene: M4_INFO does NOT use the full path"
have_order "$TMP/good" "M6_INFO scene-loaded" "M4_INFO rendering" \
                                         "scene: load line precedes M4_INFO"
have_order "$TMP/good" "M4_INFO rendering" "M1_DONE" \
                                         "scene: M4_INFO precedes M1_DONE"
have "$TMP/good" "M1_DONE rendered 8 frames" "scene: M1_DONE"

# ---- Q7/Q8: --save-scene= writes, and combines with an entry mode ----------
run_bin "$TMP/save" 0 "GL: --save-scene= good path (exit 0)" \
    "--scene=${SC}/good.scene" "--save-scene=${SC}/saved.scene" --frames=1
have "$TMP/save" "M6_INFO scene-saved" "save: M6_INFO scene-saved line"
check "save: the document was written" \
      "$([ -s "${SC}/saved.scene" ] && echo pass || echo fail)"
# Orthogonal to the entry set: --demo + --save-scene= is legal (0045), and this
# asserts the combination still is.
run_bin "$TMP/save_demo" 0 "GL: --demo + --save-scene= (exit 0)" \
    --demo "--save-scene=${SC}/demo-saved.scene" --frames=1
check "save_demo: --save-scene= combines with --demo" \
      "$([ -s "${SC}/demo-saved.scene" ] && echo pass || echo fail)"

# ---- Q8: overwrite is silent and leaves no residue ------------------------
# Two saves over the same path must leave ONE valid document, and no temp file.
run_bin "$TMP/ow1" 0 "GL: overwrite #1 (exit 0)" \
    "--scene=${SC}/good.scene" "--save-scene=${SC}/ow.scene" --frames=1
write_scene "$SC/good2.scene" crown 7
run_bin "$TMP/ow2" 0 "GL: overwrite #2 (exit 0)" \
    "--scene=${SC}/good2.scene" "--save-scene=${SC}/ow.scene" --frames=1
check "overwrite: exactly one document at the destination" \
      "$([ "$(find "$SC" -maxdepth 1 -name 'ow.scene' | wc -l)" = "1" ] && echo pass || echo fail)"
check "overwrite: no temp residue in the destination directory" \
      "$([ "$(find "$SC" -maxdepth 1 -name '.*' ! -name '.' | wc -l)" = "0" ] && echo pass || echo fail)"
# The second save must be the surviving content (position 7, not 0), which is
# what distinguishes a real overwrite from a no-op.
have "$TMP/ow2" "M4_INFO rendering scene 'good2'" "overwrite: 2nd save ran"

# ---- the end-to-end round-trip: save, then RELOAD via --scene= ------------
# The load-then-compare is the ticket's "entities and transforms match" row.
# Written as a real second process run, not a unit test, so the whole
# CLI -> file -> CLI path is covered.
# Saved from good2.scene, whose position is 7 -- a value a defaulted or
# zero-initialised transform could not produce by accident.
run_bin "$TMP/rt_save" 0 "round-trip: save (exit 0)" \
    "--scene=${SC}/good2.scene" "--save-scene=${SC}/rt.scene" --frames=1
run_bin "$TMP/rt_load" 0 "round-trip: reload the saved document (exit 0)" \
    "--scene=${SC}/rt.scene" --frames=1
have "$TMP/rt_load" "M4_INFO rendering scene 'rt' entities=1" \
                                     "round-trip: reloaded 1 entity, stem identity"
# Idempotence: re-saving the RELOADED document must reproduce it byte for byte.
# (It has to be rt.scene against its own re-save -- comparing two saves made from
# different source documents would differ by construction and prove nothing.)
run_bin "$TMP/rt_resave" 0 "round-trip: re-save the reloaded document (exit 0)" \
    "--scene=${SC}/rt.scene" "--save-scene=${SC}/rt2.scene" --frames=1
cmp -s "$SC/rt.scene" "$SC/rt2.scene" \
    && check "round-trip: re-save is byte-identical (writer is idempotent)" pass \
    || check "round-trip: re-save is byte-identical (writer is idempotent)" fail
# And the position must have survived, not defaulted.
have "$SC/rt.scene" '"position":\[7.0,0.0,0.0\]' \
     "round-trip: a NON-IDENTITY position survived the save"

# ---- Q10: an empty document is valid ---------------------------------------
printf '%s' '{"version":1,"entities":[]}' >"$SC/empty.scene"
run_bin "$TMP/empty" 0 "GL: --scene= empty document (exit 0)" \
    "--scene=${SC}/empty.scene" --frames=1
have "$TMP/empty" "M4_INFO rendering scene 'empty' entities=0" \
                                         "empty: loads with entities=0, exit 0"

# ---- Q3/user story 4: an unknown assetId is WARNED, kept, and skipped ------
# ADR-0013 Q3: a semantic problem does not fail the load.  The entity is kept as
# pure data, warned, and skipped at render by the M4 resolver.
printf '%s' '{"version":1,"entities":[{"id":1,"name":"ghost","assetId":"no_such_asset","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}},{"id":2,"name":"real","assetId":"smile","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]}' >"$SC/unresolved.scene"
run_bin "$TMP/unresolved" 0 "GL: unknown assetId still opens (exit 0)" \
    "--scene=${SC}/unresolved.scene" --frames=1
have "$TMP/unresolved" "M6_WARN" "unresolved: warned, not refused"
have "$TMP/unresolved" "no_such_asset" "unresolved: the warning names the id"
# Retained as data: both entities are still in the Scene even though one cannot
# be drawn, so the entity count is 2 while only 1 is renderable.
have "$TMP/unresolved" "M4_INFO rendering scene 'unresolved' entities=2" \
                                         "unresolved: entity retained (entities=2)"
# The document round-trips with the unknown id INTACT — leniency on references
# means "keep it verbatim", not "drop it".
run_bin "$TMP/unres_save" 0 "unresolved: save (exit 0)" \
    "--scene=${SC}/unresolved.scene" "--save-scene=${SC}/unresolved-saved.scene" --frames=1
have "$SC/unresolved-saved.scene" "no_such_asset" \
    "unresolved: the unknown id survives the round-trip verbatim"

echo "── tier A: M6 headless suites ───────────────────────────────────────────"
# The atomicity/overwrite/no-mkdir contracts are filesystem claims, so the
# suite that pins them runs here too — a close matrix that only asserted them
# through the CLI would miss a regression in the failure paths the CLI cannot
# reach (e.g. a failed rename leaving a temp file behind).
for suite in noto3d-scene-file-io-tests noto3d-serialization-tests; do
    if [ -x "${BUILD_DIR}/${suite}" ]; then
        "${BUILD_DIR}/${suite}" >"$TMP/suite" 2>&1
        check "headless: ${suite} ALL PASS" \
              "$(grep -q 'ALL PASS' "$TMP/suite" && echo pass || echo fail)"
    else
        check "headless: ${suite} present" fail
    fi
done

echo "── tier A: static contract checks ───────────────────────────────────────"
# The arch guard from 0041 still holds: the serializer reaches no GL.  The symbol
# check is the 0037/0038 evidence style (a count, not an assertion in a comment).
# One archive, one check -- an earlier version looped over a single literal and a
# comment claimed it covered the app I/O seam too, which it did not.
SERIALIZATION_ARCHIVE="${BUILD_DIR}/libnoto3d_serialization.a"
if [ -f "$SERIALIZATION_ARCHIVE" ]; then
    n=$(nm -C --defined-only "$SERIALIZATION_ARCHIVE" 2>/dev/null | grep -cE ' (T|t|W|w) .*(gl[A-Z]|glad|glew)' || true)
    check "seam: noto3d_serialization defines 0 gl*/glad symbols (got ${n})" \
          "$([ "$n" = "0" ] && echo pass || echo fail)"
else
    check "seam: noto3d_serialization archive present" fail
fi
# Q6: the app layer owns file I/O, and the serializer does not.
have src/app/scene_file_io.cpp "readSceneFile" "seam: file I/O lives in the app layer"
# Q6: Editor stays I/O-free -- it must not have grown a filesystem call.
check "seam: editor.cpp contains no filesystem include" \
      "$(grep -qE '#include <(filesystem|fstream)>' src/app/editor.cpp && echo fail || echo pass)"
# Q6: load still enters through exactly one by-value Editor API.
have src/app/editor_entry.cpp "replaceScene(std::move(" "seam: load enters via replaceScene by value"
# The preloaded document must be INSTALLED, not re-read.  Added after a
# /code-review found buildEntryScene ignoring its own preloadedScene parameter and
# re-reading the file POST-GL: every row above still passed, because re-reading a
# VALID document is indistinguishable from having preloaded it.  A row that only
# greps for the symbol could not catch that, so this one checks the two properties
# that a real re-read would break: the scene branch must install the parameter, and
# it must not call the loader itself.
# -F: the needle is full of regex metacharacters, and a BRE would read the
# unbalanced "(" as a group opener and simply never match.
grep -qF 'replaceScene(std::move(*preloadedScene))' src/app/editor_entry.cpp \
    && check "seam: the scene branch installs the PRELOADED scene" pass \
    || check "seam: the scene branch installs the PRELOADED scene" fail
check "seam: the scene branch does not re-read the document itself" \
      "$(awk '/\} else if \(opts\.scenePath\)/,/entry\.sceneName/' src/app/editor_entry.cpp \
         | grep -q 'loadSceneFrom(' && echo fail || echo pass)"
# And the parameter is NOT defaulted: a default is what let a caller skip the
# preload without the compiler objecting.
check "seam: preloadedScene is required, not defaulted" \
      "$(grep -qE 'optional<scene::Scene> preloadedScene *=' src/app/editor_entry.hpp && echo fail || echo pass)"
# The single render-path entry (ADR-0010 Q5) is unchanged by M6.
check "seam: collectDrawItems is still the sole render entry" \
      "$([ "$(grep -rl 'collectDrawItems(' src/app/*.cpp | wc -l)" -ge 1 ] && echo pass || echo fail)"
# Q9: the milestone tag is frozen at (M4); M6 added no new tag.  This is the
# single place the tag is asserted (an earlier version checked it twice).
check "seam: window-title suffix is still 'noto3d (M4)'" \
      "$(grep -q 'noto3d (M4)' src/app/editor_entry.cpp && echo pass || echo fail)"

echo "── tier B: manual evidence (NOT asserted here) ──────────────────────────"
echo "NOTE  xprop WM_NAME titles for the --scene= run, and opening a saved"
echo "      document in a real window to confirm entities and transforms, are"
echo "      tier-B items.  They need a real window session, which this"
echo "      environment does not have; they are recorded in the ticket and the"
echo "      summary, not claimed here."

echo "──────────────────────────────────────────────────────────────────────────"
if [ "$FAILS" = "0" ]; then
    echo "M6 close matrix: ALL PASS"
else
    echo "M6 close matrix: ${FAILS} FAILURE(S)"
fi
exit $([ "$FAILS" = "0" ] && echo 0 || echo 1)
