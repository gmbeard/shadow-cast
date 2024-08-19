#ifndef SHADOW_CAST_STICKY_CANCEL_EVENT_HPP_INCLUDED
#define SHADOW_CAST_STICKY_CANCEL_EVENT_HPP_INCLUDED

#include "exios/exios.hpp"

namespace sc
{

struct StickyCancelEvent
{
    template <typename... Args>
    explicit StickyCancelEvent(exios::Context ctx, Args&&... args)
        : ctx_ { ctx }
        , event_ { ctx, std::forward<Args>(args)... }
        , cancelled_ { false }
    {
    }

    auto cancel() noexcept -> void;

    template <typename Completion>
    auto wait_for_event(Completion&& completion) -> void
    {
        if (!cancelled_) {
            event_.wait_for_event(std::move(completion));
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

    template <typename F>
    auto trigger_with_value(std::uint64_t val, F&& completion) -> void
    {
        if (!cancelled_) {
            event_.trigger_with_value(val, std::move(completion));
            return;
        }

        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

        auto fn = [completion = std::move(completion)]() mutable {
            std::move(completion)(
                exios::Result<std::error_code> { exios::result_error(
                    std::make_error_code(std::errc::operation_canceled)) });
        };

        ctx_.post(std::move(fn), alloc);
    }

private:
    exios::Context ctx_;
    exios::Event event_;
    bool cancelled_ { false };
};

} // namespace sc
#endif // SHADOW_CAST_STICKY_CANCEL_EVENT_HPP_INCLUDED
