#ifndef DOCTEST_PARTS_PRIVATE_SUBCASE_PLAN
#define DOCTEST_PARTS_PRIVATE_SUBCASE_PLAN

#include "doctest/parts/private/prelude.h"

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_PUSH

#ifndef DOCTEST_CONFIG_DISABLE

namespace doctest {
namespace detail {

// A compiled selection plan for path-based subcase filtering (--subcase-path).
// Each selected path is a '/'-separated chain of subcase name patterns (wildcards
// are allowed per segment) matched against the chain of ancestor subcase names from
// the test case root down to the candidate subcase. This makes it possible to select
// one specific nested subcase even when siblings under other parents share its name -
// something the plain name filters (--subcase) cannot express.
// Compiling the plan is a pure string operation - it never executes any test body.
class SubcasePathPlan {
public:
    void compile(const std::vector<String> &pathFilters, bool caseSensitive);

    // true when at least one path was compiled and path filtering should take place
    bool active() const { return !m_paths.empty(); }

    size_t pathCount() const { return m_paths.size(); }

    // ancestors: names of the currently entered subcases from the root downwards;
    // candidate: the name of the subcase that is about to be entered.
    // Returns true when the candidate may be entered: the resulting chain must share a
    // full prefix with at least one selected path (in either direction - ancestors of
    // a selected path are entered on the way to it, and descendants of a fully matched
    // path belong to the selected subtree).
    bool allows(const std::vector<String> &ancestors, const String &candidate) const;

private:
    std::vector<std::vector<String>> m_paths;
    bool m_caseSensitive = false;
};

} // namespace detail
} // namespace doctest

#endif // DOCTEST_CONFIG_DISABLE

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_POP

#endif // DOCTEST_PARTS_PRIVATE_SUBCASE_PLAN
