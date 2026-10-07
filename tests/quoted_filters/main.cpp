// Acceptance fixture for quoted filter lexing (--dt-quoted-filters).
// Four minimal test cases whose names contain commas and/or quotes.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("alpha,beta")      { CHECK(true); } // comma in the name
TEST_CASE("say \"hi\"")      { CHECK(true); } // quotes in the name
TEST_CASE("gamma,\"delta\"") { CHECK(true); } // comma and quotes in the name
TEST_CASE("plain")           { CHECK(true); } // control case, no special chars
