// Regression test for the reporter close guarantee: every reporter whose
// constructor has completed is closed exactly once - on normal completion, on
// fail-fast early exits and when the run is aborted by an exception - while
// reporters which failed to initialize are never closed and a throwing close
// callback cannot mask the original failure. Scenario 1 doubles as a short
// regression example for the original IReporter interface (event order).

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

DOCTEST_MAKE_STD_HEADERS_CLEAN_FROM_WARNINGS_ON_WALL_BEGIN
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
DOCTEST_MAKE_STD_HEADERS_CLEAN_FROM_WARNINGS_ON_WALL_END

namespace {

std::vector<std::string> &events() {
    static std::vector<std::string> log;
    return log;
}

void record(const char *name, const char *event) {
    events().emplace_back(std::string(name) + ":" + event);
}

size_t count_event(const char *what) {
    size_t n = 0;
    for(const auto &curr: events())
        if(curr == what)
            ++n;
    return n;
}

std::string joined_events() {
    std::string out;
    for(const auto &curr: events()) {
        if(!out.empty())
            out += " ";
        out += curr;
    }
    return out;
}

// counting reporter over the original IReporter interface + the close callback
struct LoggingReporterBase : public doctest::IReporter {
    const char *name;

    explicit LoggingReporterBase(const char *log_name)
            : name(log_name) {
        record(name, "ctor");
    }
    ~LoggingReporterBase() override { record(name, "dtor"); }

    void report_query(const doctest::QueryData &) override {}

    void test_run_start() override { record(name, "run_start"); }
    void test_run_end(const doctest::TestRunStats &) override { record(name, "run_end"); }

    void test_case_start(const doctest::TestCaseData &) override { record(name, "case_start"); }
    void test_case_reenter(const doctest::TestCaseData &) override {}
    void test_case_end(const doctest::CurrentTestCaseStats &) override { record(name, "case_end"); }

    void test_case_exception(const doctest::TestCaseException &) override {}

    void subcase_start(const doctest::SubcaseSignature &) override {}
    void subcase_end() override {}

    void log_assert(const doctest::AssertData &) override {}
    void log_message(const doctest::MessageData &) override {}

    void test_case_skipped(const doctest::TestCaseData &) override {}

    void reporter_close() override { record(name, "close"); }
};

struct CountingReporter : public LoggingReporterBase {
    explicit CountingReporter(const doctest::ContextOptions &)
            : LoggingReporterBase("counting") {}
};

// constructor throws - the reporter never finishes initialization
struct ThrowingCtorReporter : public LoggingReporterBase {
    explicit ThrowingCtorReporter(const doctest::ContextOptions &)
            : LoggingReporterBase("bad_init") {
        throw std::runtime_error("ctor boom");
    }
};

// close callback throws - must not mask a failure which is already in flight
struct ThrowingCloseReporter : public LoggingReporterBase {
    explicit ThrowingCloseReporter(const doctest::ContextOptions &)
            : LoggingReporterBase("bad_close") {}
    void reporter_close() override {
        record(name, "close");
        throw std::runtime_error("close boom");
    }
};

// a reporter callback throws in the middle of the run
struct ThrowingCallbackReporter : public LoggingReporterBase {
    explicit ThrowingCallbackReporter(const doctest::ContextOptions &)
            : LoggingReporterBase("bad_cb") {}
    void test_run_start() override {
        record(name, "run_start");
        throw std::runtime_error("callback boom");
    }
};

} // namespace

REGISTER_REPORTER("counting", 1, CountingReporter);
REGISTER_REPORTER("bad_init", 2, ThrowingCtorReporter);
REGISTER_REPORTER("bad_close", 1, ThrowingCloseReporter);
REGISTER_REPORTER("bad_cb", 2, ThrowingCallbackReporter);

TEST_CASE("reporter_close/passing") { CHECK(1 == 1); }
TEST_CASE("reporter_close/failing_1") { CHECK(false); }
TEST_CASE("reporter_close/failing_2") { CHECK(false); }

int main() {
    int failures = 0;
    const auto expect = [&failures](bool cond, const char *msg) {
        if(cond) {
            std::cout << "ok   - " << msg << "\n";
        } else {
            ++failures;
            std::cout << "FAIL - " << msg << "\n";
        }
    };

    std::ostringstream sink;

    // scenario 1: normal completion - original interface event order + close at the end
    events().clear();
    {
        doctest::Context ctx;
        ctx.setCout(&sink);
        ctx.addFilter("test-case", "reporter_close/passing");
        ctx.addFilter("reporters", "counting");
        const int res = ctx.run();
        expect(res == EXIT_SUCCESS, "normal completion: exit code");
        expect(joined_events() == "counting:ctor counting:run_start counting:case_start "
                                  "counting:case_end counting:run_end counting:close counting:dtor",
               "normal completion: callback order");
    }

    // scenario 2: fail-fast early exit via --abort-after=1
    events().clear();
    {
        doctest::Context ctx;
        ctx.setCout(&sink);
        ctx.addFilter("test-case", "reporter_close/failing*");
        ctx.addFilter("reporters", "counting");
        ctx.setOption("abort-after", 1);
        const int res = ctx.run();
        expect(res == EXIT_FAILURE, "fail-fast: exit code");
        expect(count_event("counting:case_start") == 1, "fail-fast: second test case never started");
        expect(joined_events() == "counting:ctor counting:run_start counting:case_start "
                                  "counting:case_end counting:run_end counting:close counting:dtor",
               "fail-fast: closed exactly once at the end");
    }

    // scenario 3: a reporter whose constructor throws is never closed, the ones
    // which were fully initialized before it are closed exactly once
    events().clear();
    {
        doctest::Context ctx;
        ctx.setCout(&sink);
        ctx.addFilter("test-case", "reporter_close/passing");
        ctx.addFilter("reporters", "counting,bad_init");
        bool caught = false;
        try {
            ctx.run();
        } catch(const std::runtime_error &e) {
            caught = true;
            expect(std::string(e.what()) == "ctor boom", "init failure: original exception propagates");
        }
        expect(caught, "init failure: run() threw");
        expect(count_event("bad_init:close") == 0, "init failure: uninitialized reporter not closed");
        expect(count_event("counting:run_start") == 0, "init failure: run never started");
        expect(count_event("counting:close") == 1 && count_event("counting:dtor") == 1,
               "init failure: initialized reporter closed exactly once");
    }

    // scenario 4: a throwing close callback cannot mask a test failure
    events().clear();
    {
        doctest::Context ctx;
        ctx.setCout(&sink);
        ctx.addFilter("test-case", "reporter_close/failing_1");
        ctx.addFilter("reporters", "bad_close");
        const int res = ctx.run();
        expect(res == EXIT_FAILURE, "throwing close: test failure not masked");
        expect(count_event("bad_close:close") == 1 && count_event("bad_close:dtor") == 1,
               "throwing close: closed exactly once");
    }

    // scenario 5: a throwing close callback cannot mask an exception in flight
    events().clear();
    {
        doctest::Context ctx;
        ctx.setCout(&sink);
        ctx.addFilter("test-case", "reporter_close/passing");
        ctx.addFilter("reporters", "bad_close,bad_cb");
        bool caught = false;
        try {
            ctx.run();
        } catch(const std::runtime_error &e) {
            caught = true;
            expect(std::string(e.what()) == "callback boom",
                   "unwind: exception in flight not masked by throwing close");
        }
        expect(caught, "unwind: run aborted by reporter callback exception");
        expect(count_event("bad_close:close") == 1 && count_event("bad_cb:close") == 1,
               "unwind: both reporters closed exactly once");
        expect(count_event("bad_close:dtor") == 1 && count_event("bad_cb:dtor") == 1,
               "unwind: both reporters destroyed");
    }

    std::cout << (failures == 0 ? "ALL OK" : "FAILURES") << std::endl;
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
