#ifndef DOCTEST_PARTS_PRIVATE_FILTERS
#define DOCTEST_PARTS_PRIVATE_FILTERS

#include "doctest/parts/private/prelude.h"

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_PUSH

#ifndef DOCTEST_CONFIG_DISABLE

namespace doctest {
namespace detail {

// matching of a string against a wildcard mask (case sensitivity configurable) taken from
// https://www.codeproject.com/Articles/1088/Wildcard-string-compare-globbing
int wildcmp(const char *str, const char *wild, bool caseSensitive);

// checks if the name matches any of the filters (and can be configured what to do when empty)
bool matchesAny(const char *name, const std::vector<String> &filters, bool matchEmpty, bool caseSensitive);

// a compiled subcase path selection plan - each entry is the chain of per-level name
// patterns from a top-level subcase down to the targeted leaf ("parent/child")
using SubcasePathPlan = std::vector<std::vector<String>>;

// compiles path filters into a selection plan - pure string processing, so compiling
// the plan never executes any test body
SubcasePathPlan compileSubcasePathPlan(const std::vector<String> &pathFilters);

// checks whether the chain of subcase names (root first, leaf last) is selected by the
// plan - a chain is selected when some plan path matches it level-by-level, i.e. the
// chain is either a prefix of the plan path (an ancestor on the way to the target) or
// the plan path is a prefix of the chain (inside the targeted leaf's subtree)
bool subcasePathSelected(const std::vector<String> &chain, const SubcasePathPlan &plan, bool caseSensitive);

} // namespace detail
} // namespace doctest

#endif // DOCTEST_CONFIG_DISABLE

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_POP

#endif // DOCTEST_PARTS_PRIVATE_FILTERS
