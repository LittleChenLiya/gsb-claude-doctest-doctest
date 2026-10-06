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
    // Re-entrancy guard: if a translator (directly or indirectly) triggers another exception
    // translation while one is already in progress on this thread, the translators are not
    // invoked again - a controlled category is returned instead so that the chain cannot
    // recurse indefinitely. The flag is reset when the outermost translation finishes, so
    // subsequent translations proceed normally with the original priority order.
    static DOCTEST_THREAD_LOCAL bool translationInProgress = false;
    if (translationInProgress)
        return "<recursive exception translation>";
    struct TranslationGuard {
        ~TranslationGuard() { translationInProgress = false; }
    } guard;
    translationInProgress = true;
    static_cast<void>(guard);
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
