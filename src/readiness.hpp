#ifndef SHADOW_CAST_READINESS_HPP_INCLUDED
#define SHADOW_CAST_READINESS_HPP_INCLUDED

#include <cinttypes>
#include <cstddef>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace sc
{

struct Service;

using ServiceDispatch = auto(*)(Service&) -> void;

struct Readiness
{
    Service* svc;
    ServiceDispatch dispatch;
};

struct FrameTickReadiness : Readiness
{
    std::size_t frame_time;
    std::size_t remaining;
};

struct FrameTimeRatio
{
    explicit FrameTimeRatio(std::size_t n, std::size_t d = 1) noexcept;

    std::size_t num;
    std::size_t denom = 1;
};

using RegisterType = std::variant<int, FrameTimeRatio>;

struct ReadinessRegister
{
    using NotifyRegisterType = std::unordered_map<int, Readiness>;
    using FrameTickRegisterType = std::vector<FrameTickReadiness>;

    explicit ReadinessRegister(Service&,
                               NotifyRegisterType&,
                               FrameTickRegisterType&,
                               std::size_t) noexcept;

    auto operator()(RegisterType val, ServiceDispatch dispatch) -> void;

    auto frame_time() const noexcept -> std::size_t;

private:
    Service* current_svc_;
    NotifyRegisterType* notify_reg_;
    FrameTickRegisterType* frame_tick_reg_;
    std::size_t frame_time_;
};

} // namespace sc

#endif // SHADOW_CAST_READINESS_HPP_INCLUDED
