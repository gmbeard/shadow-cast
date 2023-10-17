#ifndef SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED

#include "services/service_registry.hpp"

namespace sc
{

struct Context
{
    Context() noexcept = default;
    Context(Context const&) = delete;
    auto operator=(Context const&) -> Context& = delete;

    auto run() -> void;
    auto services() noexcept -> ServiceRegistry&;

private:
    ServiceRegistry reg_;
};

} // namespace sc

#endif // SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED
