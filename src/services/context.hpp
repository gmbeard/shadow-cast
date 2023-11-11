#ifndef SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED

#include "services/service_registry.hpp"
#include <atomic>

namespace sc
{

struct Context
{
    explicit Context(std::uint32_t = 60) noexcept;
    Context(Context const&) = delete;
    auto operator=(Context const&) -> Context& = delete;

    auto run() -> void;
    auto services() noexcept -> ServiceRegistry&;
    auto request_stop() noexcept -> void;

private:
    std::atomic<bool> stop_requested_ { false };
    ServiceRegistry reg_;
    std::uint32_t fps_;
};

} // namespace sc

#endif // SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED
