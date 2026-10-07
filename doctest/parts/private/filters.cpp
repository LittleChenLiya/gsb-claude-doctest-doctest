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

bool tokenizeFilters(const char *filtersString, std::vector<String> &res, bool quotedFilters) {
    // tokenize with "," as a separator, unless escaped with backslash or inside a quoted section
    std::ostringstream s;
    auto flush = [&s, &res]() {
        auto string = s.str();
        if (!string.empty()) {
            res.emplace_back(string.c_str());
        }
        s.str("");
    };

    bool seenBackslash = false;
    bool inQuotes = false;
    const char *current = filtersString;
    const char *end = current + strlen(current);
    while (current != end) {
        const char character = *current++;
        if (seenBackslash) {
            // a single escape path: a backslash always quotes the very next character - inside
            // quoted sections this is how literal quotes/backslashes are produced, and outside
            // of them the legacy escapes ("\," and "\\") keep working exactly as before
            seenBackslash = false;
            if (inQuotes || character == ',' || character == '\\' ||
                (quotedFilters && character == '"')) {
                s.put(character);
            } else {
                s.put('\\');
                s.put(character);
            }
            continue;
        }
        if (character == '\\') {
            seenBackslash = true;
        } else if (quotedFilters && character == '"') {
            inQuotes = !inQuotes;
        } else if (character == ',' && !inQuotes) {
            flush();
        } else {
            s.put(character);
        }
    }

    if (seenBackslash) {
        s.put('\\');
    }
    flush();
    return !inQuotes; // an unterminated quoted section is a malformed filter expression
}

} // namespace detail
} // namespace doctest

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_POP
