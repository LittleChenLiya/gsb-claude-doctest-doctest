#!/usr/bin/env bash
# Acceptance runner for quoted filter expressions (--quoted-filters / -qf).
#
# Builds the four-test-case fixture in quoted_filters.cpp (test names contain
# commas and quotes) and verifies:
#   A. quoted selection of a test name containing a comma
#   B. escaped quotes inside a quoted filter (single escape path)
#   C. exclude filters keep priority over include filters
#   D. legacy unquoted expressions are unchanged (option off and on)
#   E. an unterminated quote errors out before any test case is executed
#
# Usage:  bash tests/quoted_filters/run_acceptance.sh
# Env:    CXX                     compiler (default: c++)
#         DOCTEST_QF_BUILD_DIR    build dir (default: tests/quoted_filters/build)
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${DOCTEST_QF_BUILD_DIR:-$ROOT/tests/quoted_filters/build}"
CXX="${CXX:-c++}"
mkdir -p "$BUILD_DIR"

echo "== building fixture with $CXX =="
"$CXX" -std=c++11 -static-libgcc -static-libstdc++ -I"$ROOT" \
    -DDOCTEST_CONFIG_IMPLEMENT_WITH_MAIN \
    "$ROOT/tests/quoted_filters/quoted_filters.cpp" -o "$BUILD_DIR/quoted_filters" || exit 1
BIN="$BUILD_DIR/quoted_filters"
[ -x "$BIN" ] || BIN="$BIN.exe" # MinGW appends .exe

failures=0
run() { "$BIN" --no-intro --no-version "$@" 2>&1; }

check_contains() { # <desc> <output> <needle>
    if [[ "$2" == *"$3"* ]]; then echo "PASS: $1"; else
        echo "FAIL: $1 - expected to find [$3] in:"; echo "$2"; failures=$((failures + 1)); fi
}
check_not_contains() { # <desc> <output> <needle>
    if [[ "$2" != *"$3"* ]]; then echo "PASS: $1"; else
        echo "FAIL: $1 - expected NOT to find [$3] in:"; echo "$2"; failures=$((failures + 1)); fi
}
check_exit() { # <desc> <actual> <expected>
    if [ "$2" -eq "$3" ]; then echo "PASS: $1"; else
        echo "FAIL: $1 - expected exit code $3, got $2"; failures=$((failures + 1)); fi
}

echo "== A: quoted selection of a name with a comma =="
out=$(run --quoted-filters '--test-case="alpha, beta"'); code=$?
check_exit "A: exit code" "$code" 0
check_contains "A: exactly one test case ran" "$out" "test cases: 1 | 1 passed"
out=$(run --quoted-filters '--test-case="alpha, beta"' --list-test-cases)
check_contains "A: selected test is [alpha, beta]" "$out" "alpha, beta"
check_contains "A: exactly one test case listed" "$out" "passing the current filters: 1"

echo "== B: escaped quotes take the single escape path =="
out=$(run --quoted-filters '--test-case="gamma, \"delta\""' --list-test-cases)
check_contains "B: selected test is [gamma, \"delta\"]" "$out" 'gamma, "delta"'
check_contains "B: exactly one test case listed" "$out" "passing the current filters: 1"

echo "== C: exclude filters keep priority over include filters =="
out=$(run --quoted-filters --test-case='*' '--test-case-exclude="alpha, beta","gamma, \"delta\""' --list-test-cases)
check_contains "C: [epsilon \"zeta\"] still selected" "$out" 'epsilon "zeta"'
check_contains "C: [plain] still selected" "$out" "plain"
check_not_contains "C: [alpha, beta] excluded" "$out" "alpha, beta"
check_contains "C: exactly two test cases listed" "$out" "passing the current filters: 2"

echo "== D: legacy unquoted expressions are unchanged =="
out=$(run --test-case='plain,epsilon*' --list-test-cases)
check_contains "D1: legacy list without the option" "$out" "passing the current filters: 2"
out=$(run --quoted-filters --test-case='plain,epsilon*' --list-test-cases)
check_contains "D2: legacy list with the option enabled" "$out" "passing the current filters: 2"
out=$(run '--test-case="alpha, beta"'); code=$?
check_exit "D3: exit code" "$code" 0
check_contains "D3: quotes are literal without the option (0 matches)" "$out" "test cases: 0 |"

echo "== E: unterminated quote errors out before any test case runs =="
out=$(run --quoted-filters '--test-case="unterminated'); code=$?
check_exit "E: failing exit code" "$code" 1
check_contains "E: error is reported" "$out" "unterminated quote"
check_not_contains "E: zero test cases executed" "$out" "test cases:"

echo
if [ "$failures" -eq 0 ]; then
    echo "ACCEPTANCE PASSED"
else
    echo "ACCEPTANCE FAILED ($failures failing checks)"
fi
exit "$failures"
