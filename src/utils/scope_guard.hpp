#ifndef SHADOW_CAST_UTILS_SCOPE_GUARD_HPP_INCLUDED
#define SHADOW_CAST_UTILS_SCOPE_GUARD_HPP_INCLUDED

#include <utility>

#define SC_GEN_VAR_NAME_IMPL(pfx, line) pfx##line
#define SC_GEN_VAR_NAME(pfx, line) SC_GEN_VAR_NAME_IMPL(pfx, line)
#define SC_SCOPE_GUARD(fn)                                                     \
    auto SC_GEN_VAR_NAME(scope_guard_, __LINE__) = ::sc::ScopeGuard            \
    {                                                                          \
        fn                                                                     \
    }

namespace sc
{

template <typename F>
struct ScopeGuard
{
    explicit ScopeGuard(F&& f) noexcept
        : fn { std::forward<F>(f) }
    {
    }

    ~ScopeGuard()
    {
        if (active)
            fn();
    }

    ScopeGuard(ScopeGuard const&) = delete;
    auto operator=(ScopeGuard const&) -> ScopeGuard& = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    auto operator=(ScopeGuard&&) -> ScopeGuard& = delete;
    auto deactivate() noexcept -> void { active = false; }

    F fn;
    bool active { true };
};

} // namespace sc

#endif // SHADOW_CAST_UTILS_SCOPE_GUARD_HPP_INCLUDED
