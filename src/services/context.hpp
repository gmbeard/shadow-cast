#ifndef SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED

#include "services/service_registry.hpp"
#include <atomic>

namespace sc
{

struct Context
{
    Context() noexcept = default;
    Context(Context const&) = delete;
    auto operator=(Context const&) -> Context& = delete;

    auto run() -> void;
    auto services() noexcept -> ServiceRegistry&;
    auto request_stop() noexcept -> void;

private:
    std::atomic<bool> stop_requested_ { false };
    ServiceRegistry reg_;
};

} // namespace sc

#endif // SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED
