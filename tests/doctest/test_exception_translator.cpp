// Our CI/CD helpfully runs this test with -fno-exceptions,
// which obviously means we can't test exception translation!
// -fno-exceptions is annoying to detect, but thankfully
// our runner also sets DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#if !defined(DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)

#include <doctest/doctest.h>
#include <type_traits>

namespace {

using doctest::detail::ExceptionTranslator;

/** Example exceptions... */
struct exception1 {};
struct exception2 {};
struct exception3 {};

/** ...and their translators */
// NOLINTBEGIN(bugprone-throwing-static-initialization, cert-err58-cpp)
const ExceptionTranslator<exception1> translator1([](exception1) { return doctest::String("exception1"); });
const ExceptionTranslator<exception2> translator2([](exception2) { return doctest::String("exception2"); });
const ExceptionTranslator<exception3> translator3([](exception3) { return doctest::String("exception3"); });
// NOLINTEND(bugprone-throwing-static-initialization, cert-err58-cpp)

/**
 * Utility to raise an exception, call the functor, then resolve the exception
 * I would normally have parametrized the return-type to `result_of<Fn()>::type`,
 * but this is not available on a few compilers in our toolchain, hence ignoring this.
 * */
template <typename Ex, typename Fn>
inline doctest::String with_ambient_exception(Ex e, Fn f) {
    try {
        throw e; // NOLINT(hicpp-exception-baseclass)
    } catch (...) { return f(); }
}

inline bool should_skip() noexcept {
    // TODO: For some reason ubuntu-22.04, gcc-6 fails these tests
    return (DOCTEST_GCC >= DOCTEST_COMPILER(6, 0, 0)) && (DOCTEST_GCC < DOCTEST_COMPILER(7, 0, 0));
}

} // namespace

TEST_CASE("Translating custom exceptions" * doctest::skip(should_skip())) {
    SUBCASE("Throwing exception1") {
        auto result = with_ambient_exception(exception1{}, [] {
            auto result_ = doctest::String();
            REQUIRE(translator1.translate(result_));
            REQUIRE(!translator2.translate(result_));
            REQUIRE(!translator3.translate(result_));
            return result_;
        });

        CHECK(result == "exception1");
    }

    SUBCASE("Throwing exception1") {
        auto result = with_ambient_exception(exception2{}, [] {
            auto result_ = doctest::String();
            REQUIRE(!translator1.translate(result_));
            REQUIRE(translator2.translate(result_));
            REQUIRE(!translator3.translate(result_));
            return result_;
        });

        CHECK(result == "exception2");
    }

    SUBCASE("Throwing exception1") {
        auto result = with_ambient_exception(exception3{}, [] {
            auto result_ = doctest::String();
            REQUIRE(!translator1.translate(result_));
            REQUIRE(!translator2.translate(result_));
            REQUIRE(translator3.translate(result_));
            return result_;
        });

        CHECK(result == "exception3");
    }
}

namespace {

/** Exceptions and translators for the reentrancy-guard test. */
struct recursive_exception {
    int id;
};
struct normal_exception {};

int recursive_translation_attempts = 0;

// This translator re-enters translateActiveException() while a translation is
// already in progress. The reentrancy guard must turn that nested call into a
// controlled category instead of recursing into the translator chain again.
doctest::String translate_recursive(recursive_exception) {
    ++recursive_translation_attempts;
    return doctest::detail::translateActiveException();
}

doctest::String translate_normal(normal_exception) {
    return doctest::String("normal");
}

// Registration order defines priority: recursive first, normal second.
// NOLINTBEGIN(bugprone-throwing-static-initialization, cert-err58-cpp)
const int reg_recursive = doctest::registerExceptionTranslator(translate_recursive);
const int reg_normal    = doctest::registerExceptionTranslator(translate_normal);
// NOLINTEND(bugprone-throwing-static-initialization, cert-err58-cpp)

} // namespace

TEST_CASE("Reentrant exception translation is guarded") {
    SUBCASE("Recursive translator does not recurse infinitely") {
        recursive_translation_attempts = 0;
        try {
            throw recursive_exception{42}; // NOLINT(hicpp-exception-baseclass)
        } catch (const recursive_exception &ex) {
            auto result = doctest::detail::translateActiveException();
            CHECK(result == "exception translation already in progress");
            // The recursive translator ran once and was not re-invoked.
            CHECK(recursive_translation_attempts == 1);
            // The original exception object is unchanged.
            CHECK(ex.id == 42);
        }
    }

    SUBCASE("Normal translation still works afterwards") {
        auto result = with_ambient_exception(normal_exception{}, [] {
            return doctest::detail::translateActiveException();
        });
        CHECK(result == "normal");
    }

    SUBCASE("Built-in fallback still works afterwards") {
        auto result = with_ambient_exception(exception3{}, [] {
            return doctest::detail::translateActiveException();
        });
        CHECK(result == "unknown exception");
    }
}

#endif // !defined(DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)
