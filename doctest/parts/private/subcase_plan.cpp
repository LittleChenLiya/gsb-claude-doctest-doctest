#include "doctest/parts/private/prelude.h"
#include "doctest/parts/private/subcase_plan.h"
#include "doctest/parts/private/filters.h"

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_PUSH

#ifndef DOCTEST_CONFIG_DISABLE

namespace doctest {
namespace detail {

void SubcasePathPlan::compile(const std::vector<String> &pathFilters, bool caseSensitive) {
    m_paths.clear();
    m_caseSensitive = caseSensitive;

    for (const auto &filter: pathFilters) {
        std::vector<String> segments;
        const char *current = filter.c_str();
        const char *end = current + std::strlen(current);
        while (current != end) {
            const char *sep =
                static_cast<const char *>(std::memchr(current, '/', static_cast<size_t>(end - current)));
            const char *segmentEnd = sep ? sep : end;
            // skip empty segments (leading, trailing or repeated '/')
            if (segmentEnd != current)
                segments.emplace_back(current, static_cast<String::size_type>(segmentEnd - current));
            current = sep ? sep + 1 : end;
        }
        if (!segments.empty())
            m_paths.push_back(segments);
    }
}

bool SubcasePathPlan::allows(const std::vector<String> &ancestors, const String &candidate) const {
    if (!active())
        return true;

    const size_t chainLength = ancestors.size() + 1;
    for (const auto &path: m_paths) {
        const size_t prefixLength = chainLength < path.size() ? chainLength : path.size();
        bool matches = true;
        for (size_t i = 0; i < prefixLength; ++i) {
            const String &actual = i < ancestors.size() ? ancestors[i] : candidate;
            if (!wildcmp(actual.c_str(), path[i].c_str(), m_caseSensitive)) {
                matches = false;
                break;
            }
        }
        if (matches)
            return true;
    }
    return false;
}

} // namespace detail
} // namespace doctest

#endif // DOCTEST_CONFIG_DISABLE

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_POP
