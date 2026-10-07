#include <doctest/doctest.h>

#include "doctest/parts/private/subcase_plan.h"
#include "doctest/parts/private/traversal.h"

#include <map>
#include <string>
#include <vector>

using doctest::String;
using doctest::SubcaseSignature;
using doctest::detail::SubcasePathPlan;
using doctest::detail::TraversalState;

namespace {

// Enters a subcase only when the compiled path plan allows it - mirrors what
// Subcase::checkFilters() does during a real run.
bool tryEnterFiltered(TraversalState &traversal, const SubcasePathPlan &plan,
                      const SubcaseSignature &signature) {
    if (!plan.allows(traversal.activeSubcaseNames(), signature.m_name))
        return false;
    return traversal.tryEnterSubcase(signature);
}

TEST_CASE("SubcasePathPlan compiles without executing test bodies") {
    size_t bodyExecutions = 0; // a test body would increment this - compiling must not

    SubcasePathPlan plan;
    CHECK_FALSE(plan.active());
    // an inactive plan imposes no selection - the default filtering is retained
    CHECK(plan.allows({}, String("anything")));
    CHECK(plan.allows({String("a")}, String("b")));

    plan.compile({String("parent/child"), String("other/*")}, false);

    // compiling the selection plan is a pure string operation - no test body ran
    CHECK(bodyExecutions == 0);
    REQUIRE(plan.active());
    CHECK(plan.pathCount() == 2);

    // a prefix of a selected path is entered on the way to the target
    CHECK(plan.allows({}, String("parent")));
    CHECK(plan.allows({String("parent")}, String("child")));
    // a same-named child under a different parent is rejected
    CHECK_FALSE(plan.allows({}, String("child")));
    CHECK_FALSE(plan.allows({String("stranger")}, String("child")));
    // descendants of a fully matched path belong to the selected subtree
    CHECK(plan.allows({String("parent"), String("child")}, String("grandchild")));
    // wildcards are matched per segment
    CHECK(plan.allows({}, String("other")));
    CHECK(plan.allows({String("other")}, String("whatever")));
    // a chain unrelated to every selected path is rejected
    CHECK_FALSE(plan.allows({String("stranger")}, String("parent")));
}

TEST_CASE("path selection executes only the target chain with same-named children") {
    // two parents, each with a nested subcase named "child"
    const SubcaseSignature parent_a = SubcaseSignature{"parent_a", "test_subcase_plan.cpp", 10};
    const SubcaseSignature child_a  = SubcaseSignature{"child", "test_subcase_plan.cpp", 11};
    const SubcaseSignature parent_b = SubcaseSignature{"parent_b", "test_subcase_plan.cpp", 20};
    const SubcaseSignature child_b  = SubcaseSignature{"child", "test_subcase_plan.cpp", 21};

    struct RunStats {
        size_t runs = 0;
        std::vector<std::string> executedBodies;
    };

    auto executeAllPaths = [&](const SubcasePathPlan &plan) {
        TraversalState traversal;
        traversal.resetForTestCase();
        RunStats stats;

        do { /* NOLINT(cppcoreguidelines-avoid-do-while) */
            traversal.resetForRun();
            stats.runs++;

            if (tryEnterFiltered(traversal, plan, parent_a)) {
                stats.executedBodies.emplace_back("parent_a");
                if (tryEnterFiltered(traversal, plan, child_a)) {
                    stats.executedBodies.emplace_back("parent_a/child");
                    traversal.leaveSubcase();
                }
                traversal.leaveSubcase();
            }

            if (tryEnterFiltered(traversal, plan, parent_b)) {
                stats.executedBodies.emplace_back("parent_b");
                if (tryEnterFiltered(traversal, plan, child_b)) {
                    stats.executedBodies.emplace_back("parent_b/child");
                    traversal.leaveSubcase();
                }
                traversal.leaveSubcase();
            }
        } while (traversal.advance());

        return stats;
    };

    SUBCASE("normal execution without a path plan is unchanged") {
        const SubcasePathPlan noPlan; // inactive - default filtering retained
        const RunStats stats = executeAllPaths(noPlan);

        CHECK(stats.runs == 2);
        CHECK(stats.executedBodies == std::vector<std::string>{
            "parent_a", "parent_a/child", "parent_b", "parent_b/child"});
    }

    SUBCASE("selecting one path executes only the target chain") {
        SubcasePathPlan plan;
        plan.compile({String("parent_b/child")}, false);
        const RunStats stats = executeAllPaths(plan);

        // the same-named child under parent_a is never entered
        CHECK(stats.runs == 1);
        CHECK(stats.executedBodies == std::vector<std::string>{"parent_b", "parent_b/child"});
    }

    SUBCASE("the plain name filter cannot disambiguate the same-named children") {
        // documents the limitation that --subcase-path solves: matching only the last
        // name selects the nested "child" under both parents
        TraversalState traversal;
        traversal.resetForTestCase();
        size_t childBodies = 0;

        do { /* NOLINT(cppcoreguidelines-avoid-do-while) */
            traversal.resetForRun();

            if (traversal.tryEnterSubcase(parent_a)) {
                if (String("child") == child_a.m_name && traversal.tryEnterSubcase(child_a)) {
                    childBodies++;
                    traversal.leaveSubcase();
                }
                traversal.leaveSubcase();
            }

            if (traversal.tryEnterSubcase(parent_b)) {
                if (String("child") == child_b.m_name && traversal.tryEnterSubcase(child_b)) {
                    childBodies++;
                    traversal.leaveSubcase();
                }
                traversal.leaveSubcase();
            }
        } while (traversal.advance());

        CHECK(childBodies == 2); // both same-named children got mixed in
    }
}

} // namespace

// Fixture for the --subcase-path command line entry point: two parents (the test case
// itself and the "parent" subcase) each have a child named "child". Driven with
// dedicated ctest invocations from tests/CMakeLists.txt:
// - no filter:                    both children run (3 assertions)
// - --subcase-path=parent/child:  only the nested target chain runs (1 assertion)
// - --subcase=child:              the last name alone cannot reach the nested child -
//                                 it enters the top-level "child" instead (2 assertions)
TEST_CASE("subcase path fixture") {
    int marker = 0;

    SUBCASE("child") {
        marker = 1;
        CHECK(marker == 1);
        CHECK(marker + 1 == 2);
    }

    SUBCASE("parent") {
        marker = 2;
        SUBCASE("child") {
            CHECK(marker == 2);
        }
    }
}
