#ifndef SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED

#include "services/service_registry.hpp"
#include "utils/frame_time.hpp"
#include <atomic>

namespace sc
{

struct Context
{
    explicit Context(FrameTime const&) noexcept;
    explicit Context(std::uint32_t = 60) noexcept;
    Context(Context const&) = delete;
    auto operator=(Context const&) -> Context& = delete;

    auto run() -> void;
    auto services() noexcept -> ServiceRegistry&;
    auto request_stop() noexcept -> void;

private:
    std::uint64_t frame_time_;
    std::atomic<bool> stop_requested_ { false };
    ServiceRegistry reg_;
    int event_fd_ { -1 };
};

} // namespace sc

#endif // SHADOW_CAST_SERVICES_CONTEXT_HPP_INCLUDED
