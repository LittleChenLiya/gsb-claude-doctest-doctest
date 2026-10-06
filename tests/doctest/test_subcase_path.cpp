#include <doctest/doctest.h>

#include "doctest/parts/private/filters.h"
#include "doctest/parts/private/traversal.h"

#include <string>
#include <vector>

using doctest::String;
using doctest::SubcaseSignature;
using doctest::detail::SubcasePathPlan;
using doctest::detail::TraversalState;
using doctest::detail::compileSubcasePathPlan;
using doctest::detail::subcasePathSelected;

namespace {

// Fixture modeling a test case whose two parent subcases each contain a child
// subcase with the same name:
//
//     TEST_CASE("...") {
//         SUBCASE("parent A") { SUBCASE("child") { ... } }
//         SUBCASE("parent B") { SUBCASE("child") { ... } }
//     }
//
// The body of a subcase only runs when the subcase is entered, so recording the
// leaf chains shows exactly which chains were executed.
struct SameNamedChildrenFixture {
    int body_runs = 0;
    std::vector<std::string> executed_leaves;

    static std::string join(const std::vector<String> &chain) {
        std::string out;
        for (const auto &level: chain) {
            if (!out.empty())
                out += '/';
            out += level.c_str();
        }
        return out;
    }

    // mirrors Subcase: reject via the (path) filters first, then enter through the
    // regular traversal machinery and run the body only while entered
    template <typename F>
    void enterSubcase(
        TraversalState &traversal, const SubcasePathPlan &plan, std::vector<String> &chain,
        const char *name, F &&body
    ) {
        chain.emplace_back(name);
        const bool selected = subcasePathSelected(chain, plan, true);
        chain.pop_back();
        if (!selected)
            return;

        const SubcaseSignature signature{name, "test_subcase_path.cpp", 0};
        if (!traversal.tryEnterSubcase(signature))
            return;

        chain.emplace_back(name);
        body();
        chain.pop_back();
        traversal.leaveSubcase();
    }

    void runChild(TraversalState &traversal, const SubcasePathPlan &plan, std::vector<String> &chain) {
        enterSubcase(traversal, plan, chain, "child", [&] {
            executed_leaves.push_back(join(chain));
        });
    }

    void runBody(TraversalState &traversal, const SubcasePathPlan &plan) {
        ++body_runs;
        std::vector<String> chain;
        enterSubcase(traversal, plan, chain, "parent A", [&] { runChild(traversal, plan, chain); });
        enterSubcase(traversal, plan, chain, "parent B", [&] { runChild(traversal, plan, chain); });
    }

    void run(const SubcasePathPlan &plan) {
        TraversalState traversal;
        traversal.resetForTestCase();
        do { // NOLINT(cppcoreguidelines-avoid-do-while)
            traversal.resetForRun();
            runBody(traversal, plan);
        } while (traversal.advance());
    }
};

} // namespace

TEST_CASE("subcase selection by parent chain") {
    SUBCASE("compiling the selection plan does not execute any test body") {
        SameNamedChildrenFixture fixture;

        const SubcasePathPlan plan = compileSubcasePathPlan({String("parent B/child")});

        REQUIRE(plan.size() == 1);
        REQUIRE(plan[0].size() == 2);
        CHECK(plan[0][0] == "parent B");
        CHECK(plan[0][1] == "child");
        CHECK(fixture.body_runs == 0);
        CHECK(fixture.executed_leaves.empty());
    }

    SUBCASE("selecting one path executes only the targeted chain") {
        const SubcasePathPlan plan = compileSubcasePathPlan({String("parent B/child")});

        SameNamedChildrenFixture fixture;
        fixture.run(plan);

        // the same-named child under "parent A" must not be mixed in
        CHECK(fixture.executed_leaves == std::vector<std::string>{"parent B/child"});
    }

    SUBCASE("without a path plan the default traversal count is unchanged") {
        SameNamedChildrenFixture fixture;
        fixture.run(SubcasePathPlan{});

        CHECK(
            fixture.executed_leaves ==
            std::vector<std::string>{"parent A/child", "parent B/child"}
        );
        CHECK(fixture.body_runs == 2);
    }
}
