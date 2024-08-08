#include "capture.hpp"

namespace sc
{

namespace detail
{
CaptureCallback::CaptureCallback(CaptureCallback&& other) noexcept
    : vtable_ { other.vtable_ }
    , erased_state_ { std::exchange(other.erased_state_, nullptr) }
{
}

CaptureCallback::~CaptureCallback()
{
    SC_EXPECT(vtable_);
    if (erased_state_)
        vtable_->destroy(erased_state_);
}

auto CaptureCallback::operator=(CaptureCallback&& other) noexcept
    -> CaptureCallback&
{
    using std::swap;

    auto tmp { std::move(other) };
    swap(vtable_, tmp.vtable_);
    swap(erased_state_, tmp.erased_state_);

    return *this;
}

auto CaptureCallback::operator()(exios::Result<std::error_code> result) -> void
{
    SC_EXPECT(erased_state_);
    SC_EXPECT(vtable_);

    /* Invoking will also destroy the state, so we must make sure
     * we set state to null in case invoke() throws...
     */
    void* state = std::exchange(erased_state_, nullptr);
    // cppcheck-suppress [nullPointerRedundantCheck]
    vtable_->invoke(state, std::move(result));
}
} // namespace detail

auto Capture::cancel() noexcept -> void
{
    SC_EXPECT(interface_);
    // cppcheck-suppress [nullPointerRedundantCheck]
    interface_->cancel();
}

auto Capture::init() -> void
{
    SC_EXPECT(interface_);
    // cppcheck-suppress [nullPointerRedundantCheck]
    interface_->initialize();
}

Capture::CaptureInterface::~CaptureInterface() {}

} // namespace sc
