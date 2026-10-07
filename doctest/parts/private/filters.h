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

// tokenizes a comma separated filter string into individual filters - when 'quotedFilters' is
// enabled, sections in double quotes are grouped into a single filter (so that commas can be
// part of a filter) and '\"' produces a literal quote through the same single escape path as
// the other backslash escapes - returns false when a quoted section is left unterminated
bool tokenizeFilters(const char *filtersString, std::vector<String> &res, bool quotedFilters);

} // namespace detail
} // namespace doctest

#endif // DOCTEST_CONFIG_DISABLE

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_POP

#endif // DOCTEST_PARTS_PRIVATE_FILTERS
