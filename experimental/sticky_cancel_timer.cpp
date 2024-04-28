#include "./sticky_cancel_timer.hpp"

namespace sc
{
StickyCancelTimer::StickyCancelTimer(exios::Context ctx)
    : ctx_ { ctx }
    , inner_ { ctx_ }
{
}

auto StickyCancelTimer::cancel() -> void
{
    inner_.cancel();
    cancelled_ = true;
}

} // namespace sc
