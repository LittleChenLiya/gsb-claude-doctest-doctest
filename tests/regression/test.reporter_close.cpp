// Regression test for the reporter close guarantee in Context::run():
// every reporter that finished initialization is closed exactly once - on normal
// completion, on fail-fast early exits and when an exception escapes the run.
// A reporter that failed to initialize is never closed, and a failure while
// closing must not mask the original failure. Uses only the existing public
// reporter registration API (doctest::registerReporter / doctest::IReporter).

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

// small in-memory fixture recording the reporter callback order
std::vector<std::string> &events() {
    static std::vector<std::string> data;
    return data;
}

std::string joined_events() {
    std::string out;
    for (const auto &e: events()) {
        if (!out.empty())
            out += ",";
        out += e;
    }
    return out;
}

int count_closes() {
    int n = 0;
    for (const auto &e: events())
        if (e.compare(0, 6, "close:") == 0)
            ++n;
    return n;
}

// no-op implementation of the reporter interface
struct ReporterBase : public doctest::IReporter {
    explicit ReporterBase(const doctest::ContextOptions &) {}
    void report_query(const doctest::QueryData &) override {}
    void test_run_start() override {}
    void test_run_end(const doctest::TestRunStats &) override {}
    void test_case_start(const doctest::TestCaseData &) override {}
    void test_case_reenter(const doctest::TestCaseData &) override {}
    void test_case_end(const doctest::CurrentTestCaseStats &) override {}
    void test_case_exception(const doctest::TestCaseException &) override {}
    void subcase_start(const doctest::SubcaseSignature &) override {}
    void subcase_end() override {}
    void log_assert(const doctest::AssertData &) override {}
    void log_message(const doctest::MessageData &) override {}
    void test_case_skipped(const doctest::TestCaseData &) override {}
};

// counting reporter - records every callback including its destruction (the close callback)
struct CountingReporter : public ReporterBase {
    explicit CountingReporter(const doctest::ContextOptions &o) : ReporterBase(o) {
        events().push_back("init:counting");
    }
    ~CountingReporter() override { events().push_back("close:counting"); }
    void test_run_start() override { events().push_back("run_start"); }
    void test_run_end(const doctest::TestRunStats &) override { events().push_back("run_end"); }
    void test_case_start(const doctest::TestCaseData &) override { events().push_back("case_start"); }
    void test_case_end(const doctest::CurrentTestCaseStats &) override { events().push_back("case_end"); }
};

// reporter whose initialization fails - must never be closed
struct ThrowingInitReporter : public ReporterBase {
    explicit ThrowingInitReporter(const doctest::ContextOptions &o) : ReporterBase(o) {
        events().push_back("init:throwing-init");
        throw std::runtime_error("init boom");
    }
    ~ThrowingInitReporter() override { events().push_back("close:throwing-init"); }
};

// reporter that throws from a callback while the run is in progress
struct ThrowingCallbackReporter : public ReporterBase {
    explicit ThrowingCallbackReporter(const doctest::ContextOptions &o) : ReporterBase(o) {
        events().push_back("init:throwing-callback");
    }
    ~ThrowingCallbackReporter() override { events().push_back("close:throwing-callback"); }
    void test_run_start() override {
        events().push_back("run_start");
        throw std::runtime_error("callback boom");
    }
};

// registered through the unchanged public API; priority orders construction
const int reg_counting =
        doctest::registerReporter<CountingReporter>("rc-counting", 0, true);
const int reg_throwing_init =
        doctest::registerReporter<ThrowingInitReporter>("rc-throwing-init", 1, true);
const int reg_throwing_callback =
        doctest::registerReporter<ThrowingCallbackReporter>("rc-throwing-callback", 0, true);

int run_inner(const char *reporters, const char *test_case, int abort_after = 0) {
    doctest::Context ctx;
    ctx.setOption("no-skip", true); // the inner cases carry the skip decorator
    ctx.setOption("no-intro", true);
    ctx.setOption("no-version", true);
    ctx.setOption("reporters", reporters);
    ctx.setOption("test-case", test_case);
    if (abort_after > 0)
        ctx.setOption("abort-after", abort_after);
    return ctx.run();
}

} // namespace

// only executed by the inner contexts above (skipped in the outer run)
TEST_CASE("rc-inner-passing" * doctest::skip()) { CHECK(1 == 1); }
TEST_CASE("rc-inner-failing" * doctest::skip()) { CHECK(1 == 2); }

TEST_CASE("reporter closed exactly once: normal completion, fail-fast and execution exception") {
    // normal completion - full callback sequence, closed exactly once at the end
    events().clear();
    CHECK(run_inner("rc-counting", "rc-inner-passing") == EXIT_SUCCESS);
    CHECK(joined_events() == "init:counting,run_start,case_start,case_end,run_end,close:counting");
    CHECK(count_closes() == 1);

    // fail-fast early exit - the run still ends orderly and closes the reporter exactly once
    events().clear();
    CHECK(run_inner("rc-counting", "rc-inner-failing", 1) == EXIT_FAILURE);
    CHECK(joined_events() == "init:counting,run_start,case_start,case_end,run_end,close:counting");
    CHECK(count_closes() == 1);

    // exception escaping the run - closed exactly once during unwinding, no run_end,
    // and the original exception propagates unchanged
    events().clear();
    CHECK_THROWS_WITH_AS(run_inner("rc-throwing-callback", "rc-inner-passing"),
                         "callback boom", std::runtime_error);
    CHECK(joined_events() == "init:throwing-callback,run_start,close:throwing-callback");
    CHECK(count_closes() == 1);
}

TEST_CASE("reporter close causality: initialization failure and original failure preserved") {
    // initialization failure - the half-initialized reporter is never closed, the
    // already-initialized one is closed exactly once and the init exception propagates
    events().clear();
    CHECK_THROWS_WITH_AS(run_inner("rc-counting,rc-throwing-init", "rc-inner-passing"),
                         "init boom", std::runtime_error);
    CHECK(joined_events() == "init:counting,init:throwing-init,close:counting");
    CHECK(count_closes() == 1);

    // a failing run keeps its failure exit code after closing - the original
    // failure is not masked by the close path
    events().clear();
    CHECK(run_inner("rc-counting", "rc-inner-failing", 1) == EXIT_FAILURE);
    CHECK(events().back() == "close:counting");
    CHECK(count_closes() == 1);
}
