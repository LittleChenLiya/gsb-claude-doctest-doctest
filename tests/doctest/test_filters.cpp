#include <doctest/parts/private/filters.h>
#include <functional>
#include <vector>

using doctest::String;
using doctest::detail::matchesAny;
using doctest::detail::tokenizeFilters;
using doctest::detail::wildcmp;

TEST_SUITE("Comparing a single string to a glob") {

    using std::placeholders::_1;

    TEST_CASE("Empty glob") {
        const auto glob = std::bind(wildcmp, _1, "", true);
        CHECK(glob("") == 1);
        CHECK(glob("nonempty") == 0);
    }

    TEST_CASE("Exact glob") {
        SUBCASE("Case-sensitive") {
            const auto glob = std::bind(wildcmp, _1, "test", true);
            CHECK(glob("") == 0);
            CHECK(glob("tes") == 0);
            CHECK(glob("test") == 1);
            CHECK(glob("Test") == 0);
            CHECK(glob("lest") == 0);
            CHECK(glob("text") == 0);
            CHECK(glob("tess") == 0);
            CHECK(glob("test!") == 0);
            CHECK(glob("retesting") == 0);
        }

        SUBCASE("Case-insensitive") {
            const auto glob = std::bind(wildcmp, _1, "test", false);
            CHECK(glob("") == 0);
            CHECK(glob("tes") == 0);
            CHECK(glob("test") == 1);
            CHECK(glob("Test") == 1);
            CHECK(glob("lest") == 0);
            CHECK(glob("text") == 0);
            CHECK(glob("tess") == 0);
            CHECK(glob("test!") == 0);
        }
    }

    TEST_CASE("Single-char wildcard") {
        SUBCASE("Leading wildcard") {
            const auto glob = std::bind(wildcmp, _1, "?est", true);
            CHECK(glob("") == 0);
            CHECK(glob("tes") == 0);
            CHECK(glob("test") == 1);
            CHECK(glob("Test") == 1);
            CHECK(glob("best") == 1);
            CHECK(glob("this") == 0);
        }

        SUBCASE("Intermediate wildcard") {
            const auto glob = std::bind(wildcmp, _1, "te?t", true);
            CHECK(glob("") == 0);
            CHECK(glob("tex") == 0);
            CHECK(glob("test") == 1);
            CHECK(glob("teSt") == 1);
            CHECK(glob("text") == 1);
            CHECK(glob("text!") == 0);
        }

        SUBCASE("Trailing wildcard") {
            const auto glob = std::bind(wildcmp, _1, "tes?", true);
            CHECK(glob("") == 0);
            CHECK(glob("tes") == 0);
            CHECK(glob("test") == 1);
            CHECK(glob("tesT") == 1);
            CHECK(glob("tess") == 1);
            CHECK(glob("test!") == 0);
        }
    }

    TEST_CASE("Multi-char wildcard") {
        SUBCASE("Only wildcard") {
            const auto glob = std::bind(wildcmp, _1, "*", true);
            CHECK(glob("") == 1);
            CHECK(glob("test") == 1);
            CHECK(glob("string") == 1);
        }

        SUBCASE("Only multiple wildcards") {
            const auto glob = std::bind(wildcmp, _1, "**", true);
            CHECK(glob("") == 1);
            CHECK(glob("test") == 1);
            CHECK(glob("string") == 1);
        }

        SUBCASE("Leading wildcard") {
            const auto glob = std::bind(wildcmp, _1, "*.png", true);
            CHECK(glob("") == 0);
            CHECK(glob(".png") == 1);
            CHECK(glob("foo.png") == 1);
            CHECK(glob("foo.bmp") == 0);
        }

        SUBCASE("Intermediate wildcard") {
            const auto glob = std::bind(wildcmp, _1, "img-*.png", true);
            CHECK(glob("") == 0);
            CHECK(glob("img-.png") == 1);
            CHECK(glob("img-0.png") == 1);
            CHECK(glob("img-42.png") == 1);
            CHECK(glob("bad-1.png") == 0);
        }

        SUBCASE("Trailing wildcard") {
            const auto glob = std::bind(wildcmp, _1, "file.*", true);
            CHECK(glob("") == 0);
            CHECK(glob("file.") == 1);
            CHECK(glob("file.png") == 1);
            CHECK(glob("file.PNG") == 1);
            CHECK(glob("file.bmp") == 1);
            CHECK(glob("file.BMP") == 1);
        }

        SUBCASE("Leading and trailing wildcard") {
            const auto glob = std::bind(wildcmp, _1, "* 2 *", true);
            CHECK(glob("1") == 0);
            CHECK(glob("2") == 0);
            CHECK(glob("3") == 0);
            CHECK(glob("1 2 3") == 1);
            CHECK(glob("_ 2 3") == 1);
            CHECK(glob("1 2 _") == 1);
        }
    }
}

TEST_SUITE("Comparing a single string to multiple globs") {

    using std::placeholders::_1;

    TEST_CASE("Empty set of filters") {
        const auto filters = std::vector<String>{};
        const auto caseSensitive = true;

        SUBCASE("With matchesEmpty disabled") {
            const auto matchesEmpty = false;
            CHECK(matchesAny("test", filters, matchesEmpty, caseSensitive) == false);
        }

        SUBCASE("With matchesEmpty enabled") {
            const auto matchesEmpty = true;
            CHECK(matchesAny("test", filters, matchesEmpty, caseSensitive) == true);
        }
    }

    TEST_CASE("Non-empty set of filters") {
        const auto filters = std::vector<String>{"foo", "bar", "baz"};
        const auto matches = std::bind(matchesAny, _1, filters, false, true);
        CHECK(matches("foo") == true);
        CHECK(matches("bar") == true);
        CHECK(matches("baz") == true);
        CHECK(matches("quux") == false);
    }
}

TEST_SUITE("Tokenizing filter strings") {

    static std::vector<String> tokenize(const char *str, bool quotedFilters, bool &ok) {
        std::vector<String> res;
        ok = tokenizeFilters(str, res, quotedFilters);
        return res;
    }

    TEST_CASE("Legacy unquoted tokenization is unchanged") {
        bool ok = false;

        CHECK(tokenize("", false, ok) == std::vector<String>{});
        CHECK(ok);

        CHECK(tokenize("foo,bar,,baz", false, ok) == std::vector<String>{"foo", "bar", "baz"});
        CHECK(ok);

        SUBCASE("Escaped commas and backslashes") {
            CHECK(tokenize(R"(this\,test\\)", false, ok) == std::vector<String>{"this,test\\"});
            CHECK(ok);
        }

        SUBCASE("Backslash before other characters is kept as is") {
            CHECK(tokenize(R"(Test that \ works)", false, ok) == std::vector<String>{"Test that \\ works"});
            CHECK(ok);
        }

        SUBCASE("Trailing backslash is kept as is") {
            CHECK(tokenize("foo\\", false, ok) == std::vector<String>{"foo\\"});
            CHECK(ok);
        }

        SUBCASE("Quotes have no special meaning without the option") {
            CHECK(tokenize(R"("foo,bar")", false, ok) == std::vector<String>{R"("foo)", R"(bar")"});
            CHECK(ok);
        }
    }

    TEST_CASE("Quoted tokenization") {
        bool ok = false;

        SUBCASE("Commas inside quotes are part of the filter") {
            CHECK(tokenize(R"("foo,bar",baz)", true, ok) == std::vector<String>{"foo,bar", "baz"});
            CHECK(ok);
        }

        SUBCASE("Quoted sections compose with unquoted parts") {
            CHECK(tokenize(R"(pre"fix,ed"post)", true, ok) == std::vector<String>{"prefix,edpost"});
            CHECK(ok);
        }

        SUBCASE("Escaped quotes take the single escape path") {
            CHECK(tokenize(R"("say \"hi\", ok")", true, ok) == std::vector<String>{R"(say "hi", ok)"});
            CHECK(ok);
        }

        SUBCASE("Escaped backslash inside quotes") {
            CHECK(tokenize(R"("a\\b")", true, ok) == std::vector<String>{"a\\b"});
            CHECK(ok);
        }

        SUBCASE("Escaped quote outside quotes is a literal quote") {
            CHECK(tokenize(R"(a\"b,c)", true, ok) == std::vector<String>{R"(a"b)", "c"});
            CHECK(ok);
        }

        SUBCASE("Unquoted expressions behave like the legacy ones") {
            CHECK(tokenize("foo,bar", true, ok) == std::vector<String>{"foo", "bar"});
            CHECK(ok);
            CHECK(tokenize(R"(this\,that)", true, ok) == std::vector<String>{"this,that"});
            CHECK(ok);
        }

        SUBCASE("Unterminated quote is reported as an error") {
            CHECK(tokenize(R"("foo,bar)", true, ok) == std::vector<String>{"foo,bar"});
            CHECK(!ok);
        }
    }
}
