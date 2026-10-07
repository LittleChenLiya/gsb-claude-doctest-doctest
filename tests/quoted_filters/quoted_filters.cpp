// Acceptance fixture for quoted filter expressions (--quoted-filters / -qf):
// four minimal test cases whose names contain commas and/or quotes.
//
// Scenarios (see run_acceptance.sh in this directory):
//   select by comma-containing name:
//       quoted_filters --quoted-filters "--test-case=\"alpha, beta\""
//   select by name with escaped quotes (single escape path):
//       quoted_filters --quoted-filters "--test-case=\"gamma, \\\"delta\\\"\""
//   exclude keeps priority over include:
//       quoted_filters --quoted-filters --test-case=* "--test-case-exclude=\"alpha, beta\""
//   legacy expressions without the option stay unchanged:
//       quoted_filters --test-case=plain,epsilon*
//   malformed (unterminated) quote -> error before any test case runs, exit code 1:
//       quoted_filters --quoted-filters "--test-case=\"unterminated"

#include <doctest/doctest.h>

TEST_CASE("alpha, beta") { CHECK(true); }
TEST_CASE("gamma, \"delta\"") { CHECK(true); }
TEST_CASE("epsilon \"zeta\"") { CHECK(true); }
TEST_CASE("plain") { CHECK(true); }
