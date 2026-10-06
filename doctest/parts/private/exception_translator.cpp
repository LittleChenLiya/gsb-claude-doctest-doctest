#include "doctest/parts/private/prelude.h"
#include "doctest/parts/private/exception_translator.h"

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_PUSH

#ifndef DOCTEST_CONFIG_DISABLE

namespace doctest {
namespace detail {

DOCTEST_DEFINE_INTERFACE(IExceptionTranslator)

void registerExceptionTranslatorImpl(const IExceptionTranslator *et) noexcept {
    if (std::find(getExceptionTranslators().begin(), getExceptionTranslators().end(), et) ==
        getExceptionTranslators().end())
        getExceptionTranslators().push_back(et);
}

std::vector<const IExceptionTranslator *> &getExceptionTranslators() noexcept {
    static std::vector<const IExceptionTranslator *> data;
    return data;
}

String translateActiveException() noexcept {
#ifndef DOCTEST_CONFIG_NO_EXCEPTIONS
    // Reentrancy guard: if a translator re-triggers exception translation while a
    // translation is already in progress, return a controlled category instead of
    // recursing into the same translator chain (the translator that caused the
    // recursion is not invoked again). The guard resets on scope exit, so the next
    // normal translation proceeds as usual.
    static bool translationInProgress = false;
    if (translationInProgress)
        return "exception translation already in progress";
    struct TranslationGuard {
        bool &flag;
        explicit TranslationGuard(bool &f) noexcept
                : flag(f) {
            flag = true;
        }
        ~TranslationGuard() { flag = false; }
    } guard(translationInProgress);

    String res;
    auto &translators = getExceptionTranslators();
    for (auto &curr: translators)
        if (curr->translate(res))
            return res;
    // clang-format off
    DOCTEST_GCC_SUPPRESS_WARNING_WITH_PUSH("-Wcatch-value")
    try {
        throw;
    } catch (std::exception &ex) {
        return ex.what();
    } catch (std::string &msg) {
        return msg.c_str();
    } catch (const char *msg) {
        return msg;
    } catch (...) {
        return "unknown exception";
    }
    DOCTEST_GCC_SUPPRESS_WARNING_POP
    // clang-format on
#else  // DOCTEST_CONFIG_NO_EXCEPTIONS
    return "";
#endif // DOCTEST_CONFIG_NO_EXCEPTIONS
}

} // namespace detail
} // namespace doctest

#endif // DOCTEST_CONFIG_DISABLE

DOCTEST_SUPPRESS_PRIVATE_WARNINGS_POP
