#include "services/readiness.hpp"
#include "services/service.hpp"
#include <algorithm>

namespace sc
{

FrameTimeRatio::FrameTimeRatio(std::size_t n, std::size_t d) noexcept
    : num { n }
    , denom { d }
{
}

ReadinessRegister::ReadinessRegister(Service& svc,
                                     NotifyRegisterType& reg,
                                     FrameTickRegisterType& freg,
                                     std::size_t ftime) noexcept
    : current_svc_ { &svc }
    , notify_reg_ { &reg }
    , frame_tick_reg_ { &freg }
    , frame_time_ { ftime }
{
}

auto ReadinessRegister::operator()(RegisterType val, ServiceDispatch dispatch)
    -> void
{
    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, int>) {
                notify_reg_->emplace(arg, Readiness { current_svc_, dispatch });
            }
            else {
                auto expiry = static_cast<std::size_t>(
                    static_cast<float>(frame_time_) *
                    (static_cast<float>(arg.num) / arg.denom));

                auto insert_pos =
                    std::lower_bound(frame_tick_reg_->begin(),
                                     frame_tick_reg_->end(),
                                     expiry,
                                     [&](auto const& a, auto const& b) {
                                         return a.frame_time < b;
                                     });

                frame_tick_reg_->emplace(
                    insert_pos,
                    FrameTickReadiness {
                        { current_svc_, dispatch }, expiry, 0 });
            }
        },
        val);
}

auto ReadinessRegister::frame_time() const noexcept -> std::size_t
{
    return frame_time_;
}
} // namespace sc
