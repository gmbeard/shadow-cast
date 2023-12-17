#ifndef SHADOW_CAST_SERVICES_DRM_VIDEO_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_DRM_VIDEO_SERVICE_HPP_INCLUDED

#include "config.hpp"

#ifdef SHADOW_CAST_ENABLE_METRICS
#include "services/metrics_service.hpp"
#include "utils/elapsed.hpp"
#endif

#include "drm/messaging.hpp"
#include "io/process.hpp"
#include "io/unix_socket.hpp"
#include "nvidia.hpp"
#include "platform/egl.hpp"
#include "platform/wayland.hpp"
#include "services/readiness.hpp"
#include "services/service.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/receiver.hpp"
#include <cstdint>
#include <optional>
#include <signal.h>

namespace sc
{

struct DRMVideoService final : Service
{
    using CaptureFrameReceiverType =
        Receiver<void(CUarray, NvCuda const&, std::uint64_t)>;

    explicit DRMVideoService(
        NvCuda nvcuda,
        CUcontext cuda_ctx,
        EGL const& egl,
        Wayland const& wayland,
        WaylandEGL const& platform_egl
            SC_METRICS_PARAM_DECLARE(MetricsService*)) noexcept;

    template <typename F>
    auto set_capture_frame_handler(F&& handler) -> void
    {
        frame_handler_ = CaptureFrameReceiverType { std::forward<F>(handler) };
    }

protected:
    auto on_init(ReadinessRegister) -> void override;
    auto on_uninit() noexcept -> void override;

private:
    static auto dispatch_frame(Service&) -> void;

private:
    NvCuda nvcuda_;
    CUcontext cuda_ctx_;
    BorrowedPtr<EGL const> egl_;
    BorrowedPtr<Wayland const> wayland_;
    BorrowedPtr<WaylandEGL const> platform_egl_;
    UnixSocket drm_socket_;
    Process drm_process_;
    sigset_t drm_proc_mask_;
    std::optional<CaptureFrameReceiverType> frame_handler_;
    CUgraphicsResource cuda_gfx_resource_ { nullptr };
    CUarray cuda_array_ { nullptr };
    SC_METRICS_MEMBER_DECLARE(MetricsService*, metrics_service_);
    SC_METRICS_MEMBER_DECLARE(Elapsed, frame_timer_);
    SC_METRICS_MEMBER_DECLARE(std::size_t, metrics_start_time_);
    std::uint64_t frame_time_ { 0 };
};

} // namespace sc

#endif // SHADOW_CAST_SERVICES_DRM_VIDEO_SERVICE_HPP_INCLUDED
