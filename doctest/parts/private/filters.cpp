#include "doctest/parts/private/prelude.h"
#include "doctest/parts/private/filters.h"

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_PUSH

namespace doctest {
namespace detail {

int wildcmp(const char *str, const char *wild, bool caseSensitive) {
    const char *cp = str;
    const char *mp = wild;

    while ((*str) && (*wild != '*')) {
        if ((caseSensitive ? (*wild != *str) : (tolower(*wild) != tolower(*str))) && (*wild != '?')) {
            return 0;
        }
        wild++;
        str++;
    }

    while (*str) {
        if (*wild == '*') {
            if (!*++wild) {
                return 1;
            }
            mp = wild;
            cp = str + 1;
        } else if ((caseSensitive ? (*wild == *str) : (tolower(*wild) == tolower(*str))) || (*wild == '?')) {
            wild++;
            str++;
        } else {
            wild = mp;
            str = cp++;
        }
    }

    while (*wild == '*') {
        wild++;
    }
    return !*wild;
}

bool matchesAny(const char *name, const std::vector<String> &filters, bool matchEmpty, bool caseSensitive) {
    if (filters.empty() && matchEmpty)
        return true;
    for (auto &curr: filters)
        if (wildcmp(name, curr.c_str(), caseSensitive))
            return true;
    return false;
}

SubcasePathPlan compileSubcasePathPlan(const std::vector<String> &pathFilters) {
    SubcasePathPlan plan;
    for (const auto &filter: pathFilters) {
        const char *curr = filter.c_str();
        if (*curr == '\0')
            continue;
        std::vector<String> path;
        const char *begin = curr;
        for (;; ++curr) {
            if (*curr == '/' || *curr == '\0') {
                path.emplace_back(begin, static_cast<String::size_type>(curr - begin));
                if (*curr == '\0')
                    break;
                begin = curr + 1;
            }
        }
        plan.push_back(path);
    }
    return plan;
}

bool subcasePathSelected(const std::vector<String> &chain, const SubcasePathPlan &plan, bool caseSensitive) {
    if (plan.empty())
        return true;
    for (const auto &path: plan) {
        const size_t shared = chain.size() < path.size() ? chain.size() : path.size();
        size_t level = 0;
        for (; level < shared; ++level)
            if (!wildcmp(chain[level].c_str(), path[level].c_str(), caseSensitive))
                break;
        if (level == shared)
            return true;
    }
    return false;
}

} // namespace detail
} // namespace doctest

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_POP
