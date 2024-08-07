#ifndef SHADOW_CAST_DRM_CUDA_CAPTURE_SOURCE_HPP_INCLUDED
#define SHADOW_CAST_DRM_CUDA_CAPTURE_SOURCE_HPP_INCLUDED

#include "av/codec.hpp"
#include "capture_source.hpp"
#include "exios/context.hpp"
#include "io/process.hpp"
#include "io/unix_socket.hpp"
#include "nvidia/cuda.hpp"
#include "platform/wayland.hpp"
#include "services/color_converter.hpp"
#include "sticky_cancel_timer.hpp"
#include "utils/cmd_line.hpp"
#include <EGL/egl.h>
#include <libavutil/frame.h>
#include <type_traits>

namespace sc
{
struct DRMCudaCaptureSource
{
    using CaptureResultType = exios::Result<AVFrame*, std::error_code>;

    DRMCudaCaptureSource(exios::Context context,
                         Parameters const& params,
                         VideoOutputSize output_size,
                         CUcontext cuda_ctx,
                         EGLDisplay egl_display) noexcept;

    DRMCudaCaptureSource(DRMCudaCaptureSource&&) noexcept = default;

    auto operator=(DRMCudaCaptureSource&&) noexcept
        -> DRMCudaCaptureSource& = default;

    auto context() const noexcept -> exios::Context const&;
    auto cancel() noexcept -> void;
    auto timer() noexcept -> StickyCancelTimer&;
    auto interval() const noexcept -> std::chrono::nanoseconds;
    auto init() -> void;
    auto deinit() -> void;
    static constexpr auto name() noexcept -> char const*
    {
        return "DRM CUDA capture";
    };

    template <CaptureCompletion<CaptureResultType> Completion>
    auto capture(AVFrame* frame, Completion completion) -> void
    {
        capture_(
            frame, &completion_proxy_<std::decay_t<Completion>>, &completion);
    }

private:
    template <CaptureCompletion<CaptureResultType> Completion>
    static auto completion_proxy_(DRMCudaCaptureSource& self,
                                  AVFrame* frame,
                                  void* data) -> void
    {
        auto& completion = *reinterpret_cast<Completion*>(data);
        auto const alloc = exios::select_allocator(completion);
        auto fn = [frame, completion = std::move(completion)]() mutable {
            std::move(completion)(
                CaptureResultType { exios::result_ok(frame) });
        };

        self.ctx_.post(std::move(fn), alloc);
    }

    auto
    capture_(AVFrame* frame,
             auto (*completion)(DRMCudaCaptureSource&, AVFrame*, void*)->void,
             void* data) -> void;

    exios::Context ctx_;
    StickyCancelTimer timer_;
    std::chrono::nanoseconds frame_interval_;
    CUcontext cuda_ctx_;
    EGLDisplay egl_display_;
    std::size_t frame_number_ { 0 };
    sigset_t drm_proc_mask_ {};
    Process drm_process_;
    UnixSocket drm_socket_;
    ColorConverter color_converter_;
};

} // namespace sc
#endif // SHADOW_CAST_DRM_CUDA_CAPTURE_SOURCE_HPP_INCLUDED
