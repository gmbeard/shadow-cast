#ifndef SHADOW_CAST_VIDEO_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_VIDEO_SERVICE_HPP_INCLUDED

#include "config.hpp"

#include "nvidia.hpp"
#include "services/service.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/receiver.hpp"
#include <optional>

namespace sc
{
struct VideoService final : Service
{
    friend auto dispatch_frame(Service&) -> void;

    using CaptureFrameReceiverType =
        Receiver<void(CUdeviceptr, NVFBC_FRAME_GRAB_INFO, std::uint64_t)>;

    VideoService(NvFBC,
                 BorrowedPtr<std::remove_pointer_t<CUcontext>>,
                 NVFBC_SESSION_HANDLE) noexcept;

    template <typename F>
    auto set_capture_frame_handler(F&& handler) -> void
    {
        receiver_ = CaptureFrameReceiverType { std::forward<F>(handler) };
    }

protected:
    auto on_init(ReadinessRegister) -> void override;

private:
    NvFBC nvfbc_;
    BorrowedPtr<std::remove_pointer_t<CUcontext>> nvcuda_ctx_;
    NVFBC_SESSION_HANDLE nvfbc_session_;

    std::optional<CaptureFrameReceiverType> receiver_;
    std::uint64_t frame_time_ { 0 };
};

auto dispatch_frame(Service&) -> void;

} // namespace sc
#endif // SHADOW_CAST_VIDEO_SERVICE_HPP_INCLUDED
