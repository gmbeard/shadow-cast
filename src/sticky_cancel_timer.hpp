#ifndef SHADOW_CAST_STICKY_CANCEL_TIMER_HPP_INCLUDED
#define SHADOW_CAST_STICKY_CANCEL_TIMER_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/context.hpp"
#include "exios/exios.hpp"
#include "exios/work.hpp"
#include <atomic>
#include <functional>
#include <system_error>

namespace sc
{

struct StickyCancelTimer
{
    StickyCancelTimer(exios::Context ctx);

    auto cancel() -> void;

    template <typename Rep, typename Period, typename Completion>
    auto wait_for_expiry_after(std::chrono::duration<Rep, Period> dur,
                               Completion&& completion) -> void
    {
        if (!cancelled_) {
            inner_.wait_for_expiry_after(dur, std::move(completion));
            return;
        }

        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

        auto fn = [completion = std::move(completion)]() mutable {
            std::move(completion)(
                exios::TimerOrEventIoResult { exios::result_error(
                    std::make_error_code(std::errc::operation_canceled)) });
        };

        ctx_.post(std::move(fn), alloc);
    }

private:
    exios::Context ctx_;
    exios::Timer inner_;
    bool cancelled_ { false };
};

} // namespace sc
#endif // SHADOW_CAST_STICKY_CANCEL_TIMER_HPP_INCLUDED
