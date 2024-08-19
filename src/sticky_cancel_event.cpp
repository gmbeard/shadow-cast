#include "sticky_cancel_event.hpp"

namespace sc
{
auto StickyCancelEvent::cancel() noexcept -> void
{
    event_.cancel();
    cancelled_ = true;
}
} // namespace sc
