#!/usr/bin/env bash
# Acceptance runner for quoted filter lexing. Compiles the fixture into a temp
# dir (override with QUOTED_FILTERS_TMP, defaults to D:/Codex/tmp) and checks:
#   1. quoted select of a name containing a comma
#   2. quoted select of a name containing quotes (escaped via the single '\' path)
#   3. quoted select of a name containing both
#   4. exclusion filters keep priority over inclusion filters
#   5. old unquoted expressions are unchanged when the option is off
#   6. an unterminated quote errors out before any test case executes
set -u
cd "$(dirname "$0")"
ROOT="../.."
TMP="${QUOTED_FILTERS_TMP:-D:/Codex/tmp}/quoted_filters"
mkdir -p "$TMP"
BIN="$TMP/quoted_filters.exe"

echo "== compiling fixture =="
g++ -std=c++11 -static -I "$ROOT" main.cpp -o "$BIN" || exit 1

fail=0
# expect_run <label> <expected-test-cases-run> <expected-exit> [args...]
expect_run() {
    local label="$1" want_cases="$2" want_exit="$3"; shift 3
    local out rc
    out="$("$BIN" --dt-no-colors=true "$@" 2>&1)"
    rc=$?
    local ran
    ran="$(printf '%s\n' "$out" | sed -n 's/.*test cases: *\([0-9]*\).*/\1/p' | head -1)"
    ran="${ran:-0}"
    if [ "$ran" = "$want_cases" ] && [ "$rc" = "$want_exit" ]; then
        echo "PASS: $label (ran=$ran, exit=$rc)"
    else
        echo "FAIL: $label (ran=$ran want=$want_cases, exit=$rc want=$want_exit)"
        printf '%s\n' "$out" | tail -5
        fail=1
    fi
}

echo "== 1. quoted select: comma inside test name =="
expect_run "select \"alpha,beta\"" 1 0 --dt-quoted-filters '--dt-test-case="alpha,beta"'

echo "== 2. quoted select: escaped quotes inside test name =="
expect_run 'select "say \"hi\""' 1 0 --dt-quoted-filters '--dt-test-case="say \"hi\""'

echo "== 3. quoted select: comma and quotes inside test name =="
expect_run 'select "gamma,\"delta\""' 1 0 --dt-quoted-filters '--dt-test-case="gamma,\"delta\""'

echo "== 4. exclusion keeps priority over inclusion =="
expect_run 'include * but exclude "alpha,beta"' 3 0 --dt-quoted-filters '--dt-test-case=*' '--dt-test-case-exclude="alpha,beta"'

echo "== 5. old expressions unchanged (option off) =="
expect_run "comma still splits: plain,alpha" 1 0 '--dt-test-case=plain,alpha'
expect_run 'backslash-comma escape: alpha\,beta' 1 0 '--dt-test-case=alpha\,beta'
expect_run "plain wildcard still works" 4 0 '--dt-test-case=*'

echo "== 6. unterminated quote: error before any test case runs =="
expect_run 'bad quote "unterminated' 0 1 --dt-quoted-filters '--dt-test-case="unterminated'

exit $fail
