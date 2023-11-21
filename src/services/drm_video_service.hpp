#ifndef SHADOW_CAST_SERVICES_DRM_VIDEO_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_DRM_VIDEO_SERVICE_HPP_INCLUDED

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
#include <optional>
#include <signal.h>

namespace sc
{

struct DRMVideoService final : Service
{
    using CaptureFrameReceiverType = Receiver<void(CUarray, NvCuda const&)>;

    explicit DRMVideoService(NvCuda nvcuda,
                             CUcontext cuda_ctx,
                             EGL& egl,
                             Wayland& wayland,
                             WaylandEGL& platform_egl) noexcept;

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
    BorrowedPtr<EGL> egl_;
    BorrowedPtr<Wayland> wayland_;
    BorrowedPtr<WaylandEGL> platform_egl_;
    UnixSocket drm_socket_;
    Process drm_process_;
    sigset_t drm_proc_mask_;
    std::optional<CaptureFrameReceiverType> frame_handler_;
    CUgraphicsResource cuda_gfx_resource_ { nullptr };
    CUarray cuda_array_ { nullptr };
};

} // namespace sc

#endif // SHADOW_CAST_SERVICES_DRM_VIDEO_SERVICE_HPP_INCLUDED
