# Quoted filter lexing — acceptance fixture

Filter expressions (`--dt-test-case=`, `--dt-test-case-exclude=`, etc.) support
double-quote grouping when the new bool option `--dt-quoted-filters` (`-qf`) is
enabled, so commas and quotes can be part of test names:

- `"alpha,beta"` is a single filter containing a comma.
- `\"` yields a literal quote, resolved through the same single backslash-escape
  path as `\,` and `\`.
- An unterminated quote is a hard error reported before any test case executes
  (exit code 1, zero test cases run).
- Exclude filters keep priority over include filters.
- With the option off (the default), lexing is byte-for-byte the old behavior:
  commas split filters and only `\,` / `\` are escapable.

## Test entry point

```sh
bash tests/quoted_filters/check.sh    # builds into D:/Codex/tmp/quoted_filters (QUOTED_FILTERS_TMP overrides)
```

`main.cpp` registers four minimal test cases whose names contain commas/quotes:
`alpha,beta`, `say "hi"`, `gamma,"delta"` and `plain` (control).

## Actual results (2026-10-07, g++ 16.1.0 MinGW, `-std=c++11 -static`)

```
== 1. quoted select: comma inside test name ==
PASS: select "alpha,beta" (ran=1, exit=0)
== 2. quoted select: escaped quotes inside test name ==
PASS: select "say \"hi\"" (ran=1, exit=0)
== 3. quoted select: comma and quotes inside test name ==
PASS: select "gamma,\"delta\"" (ran=1, exit=0)
== 4. exclusion keeps priority over inclusion ==
PASS: include * but exclude "alpha,beta" (ran=3, exit=0)
== 5. old expressions unchanged (option off) ==
PASS: comma still splits: plain,alpha (ran=1, exit=0)
PASS: backslash-comma escape: alpha\,beta (ran=1, exit=0)
PASS: plain wildcard still works (ran=4, exit=0)
== 6. unterminated quote: error before any test case runs ==
PASS: bad quote "unterminated (ran=0, exit=1)
```

Raw output of the bad-quote run (note: zero test cases execute, exit code 1):

```
$ quoted_filters.exe --dt-quoted-filters "--dt-test-case=\"unterminated"
[doctest] unterminated quote in filter expression for option '--dt-test-case='
exit=1
```

Raw output of the exclusion-priority run (`--dt-test-case=*` plus
`--dt-test-case-exclude="alpha,beta"`, listed with `--dt-list-test-cases`):

```
say "hi"
gamma,"delta"
plain
[doctest] unskipped test cases passing the current filters: 3
```
