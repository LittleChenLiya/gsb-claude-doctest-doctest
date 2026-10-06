// Our CI/CD helpfully runs this test with -fno-exceptions,
// which obviously means we can't test exception translation!
// -fno-exceptions is annoying to detect, but thankfully
// our runner also sets DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#if !defined(DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)

#include <doctest/doctest.h>
#include <type_traits>

// translateActiveException() is a private interface (not declared without
// DOCTEST_CONFIG_IMPLEMENT) - forward-declare it so the re-entrancy guard can be
// tested directly. The symbol is provided by the linked doctest implementation.
namespace doctest {
namespace detail {
String translateActiveException() noexcept;
} // namespace detail
} // namespace doctest

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

/** Exceptions and translators for the re-entrancy guard tests */
struct recursive_exception {
    int value;
};
struct normal_exception {};

/**
 * A translator which re-enters the translation chain while a translation is
 * already in progress (e.g. by translating a nested exception of its own).
 * Without a re-entrancy guard this would recurse indefinitely.
 */
struct RecursiveTranslator : doctest::detail::IExceptionTranslator {
    mutable int invocations = 0;
    bool translate(doctest::String &res) const override {
        try {
            throw;
        } catch (const recursive_exception &) {
            ++invocations;
            res = doctest::detail::translateActiveException(); // re-enter the chain
            return true;
        } catch (...) {} // NOLINT(bugprone-empty-catch)
        return false;
    }
};

/** A plain translator used to verify that normal translation still works. */
struct NormalTranslator : doctest::detail::IExceptionTranslator {
    bool translate(doctest::String &res) const override {
        try {
            throw;
        } catch (const normal_exception &) {
            res = "normal exception";
            return true;
        } catch (...) {} // NOLINT(bugprone-empty-catch)
        return false;
    }
};

// NOLINTBEGIN(bugprone-throwing-static-initialization, cert-err58-cpp)
RecursiveTranslator recursiveTranslator;
NormalTranslator normalTranslator;
// NOLINTEND(bugprone-throwing-static-initialization, cert-err58-cpp)

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

TEST_CASE("Re-entrant exception translation guard" * doctest::skip(should_skip())) {
    // registration is idempotent - safe to repeat for each subcase run
    doctest::detail::registerExceptionTranslatorImpl(&recursiveTranslator);
    doctest::detail::registerExceptionTranslatorImpl(&normalTranslator);
    recursiveTranslator.invocations = 0;

    SUBCASE("recursive translation terminates with a controlled category") {
        recursive_exception ex{42};
        auto result = with_ambient_exception(ex, [] {
            return doctest::detail::translateActiveException();
        });

        // the re-entrant call returned the controlled category instead of recursing
        CHECK(result == "<recursive exception translation>");
        // the recursive translator was not invoked again by the re-entrant pass
        CHECK(recursiveTranslator.invocations == 1);
        // the original exception object is unchanged
        CHECK(ex.value == 42);
    }

    SUBCASE("normal translation still works after a guarded recursion") {
        auto recursive_result = with_ambient_exception(recursive_exception{7}, [] {
            return doctest::detail::translateActiveException();
        });
        CHECK(recursive_result == "<recursive exception translation>");

        // the guard was reset - the next translation follows the original priority order
        auto normal_result = with_ambient_exception(normal_exception{}, [] {
            return doctest::detail::translateActiveException();
        });
        CHECK(normal_result == "normal exception");

        // ...and a later recursive translation is guarded again, not stuck
        recursiveTranslator.invocations = 0;
        auto second_recursive = with_ambient_exception(recursive_exception{8}, [] {
            return doctest::detail::translateActiveException();
        });
        CHECK(second_recursive == "<recursive exception translation>");
        CHECK(recursiveTranslator.invocations == 1);
    }
}

#endif // !defined(DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)
