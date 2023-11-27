#ifndef SHADOW_CAST_VIDEO_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_VIDEO_SERVICE_HPP_INCLUDED

#include "config.hpp"

#ifdef SHADOW_CAST_ENABLE_METRICS
#include "services/metrics_service.hpp"
#include "utils/elapsed.hpp"
#endif

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
        Receiver<void(CUdeviceptr, NVFBC_FRAME_GRAB_INFO)>;

    VideoService(NvFBC,
                 BorrowedPtr<std::remove_pointer_t<CUcontext>>,
                 NVFBC_SESSION_HANDLE SC_METRICS_PARAM_DECLARE(
                     MetricsService*)) noexcept;

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
    SC_METRICS_MEMBER_DECLARE(MetricsService*, metrics_service_);
    SC_METRICS_MEMBER_DECLARE(Elapsed, frame_timer_);
    SC_METRICS_MEMBER_DECLARE(std::size_t, metrics_start_time_);
};

auto dispatch_frame(Service&) -> void;

} // namespace sc
#endif // SHADOW_CAST_VIDEO_SERVICE_HPP_INCLUDED
