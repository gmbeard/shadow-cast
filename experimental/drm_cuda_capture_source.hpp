#ifndef SHADOW_CAST_DRM_CUDA_CAPTURE_SOURCE_HPP_INCLUDED
#define SHADOW_CAST_DRM_CUDA_CAPTURE_SOURCE_HPP_INCLUDED

#include "av/codec.hpp"
#include "capture_source.hpp"
#include "cuda.hpp"
#include "exios/context.hpp"
#include "io/process.hpp"
#include "io/unix_socket.hpp"
#include "nvidia/cuda.hpp"
#include "platform/wayland.hpp"
#include "services/color_converter.hpp"
#include "sticky_cancel_timer.hpp"
#include "utils/cmd_line.hpp"
#include "utils/scope_guard.hpp"
#include <EGL/egl.h>
#include <libavutil/frame.h>
#include <type_traits>

namespace sc
{
namespace cu
{
struct GraphicsResource
{
    GraphicsResource() noexcept = default;
    explicit GraphicsResource(CUgraphicsResource value) noexcept;
    GraphicsResource(GraphicsResource&& other) noexcept;
    ~GraphicsResource();
    auto operator=(GraphicsResource&& other) noexcept -> GraphicsResource&;
    operator bool() const noexcept;
    operator CUgraphicsResource() const noexcept;

private:
    CUgraphicsResource value_ { nullptr };
};

auto graphics_gl_register_image(unsigned int texture_name,
                                unsigned int texture_target,
                                unsigned int flags) -> GraphicsResource;

template <typename F>
auto with_cuda_context(CUcontext context, F f) -> void
{
    CUcontext old_ctx;
    /*CUresult res = */ sc::cuda().cuCtxPushCurrent_v2(context);
    SC_SCOPE_GUARD([&] { sc::cuda().cuCtxPopCurrent_v2(&old_ctx); });

    f();
}

template <typename F>
auto graphics_map_resource_array(GraphicsResource& resource,
                                 unsigned int map_flags,
                                 F f) -> void
{
    using namespace std::string_literals;

    char const* err_str = "unknown";
    CUgraphicsResource val = resource;
    if (auto const r =
            cuda().cuGraphicsResourceSetMapFlags(resource, map_flags);
        r != CUDA_SUCCESS) {
        sc::cuda().cuGetErrorString(r, &err_str);
        throw std::runtime_error { "CUDA: Failed to set map flags - "s +
                                   err_str };
    }

    if (auto const r = sc::cuda().cuGraphicsMapResources(1, &val, 0);
        r != CUDA_SUCCESS) {
        sc::cuda().cuGetErrorString(r, &err_str);
        throw std::runtime_error { "CUDA: cuGraphicsMapResources failed - "s +
                                   err_str };
    }

    SC_SCOPE_GUARD([&] { cuda().cuGraphicsUnmapResources(1, &val, 0); });

    CUarray cuda_array;
    if (auto const r = sc::cuda().cuGraphicsSubResourceGetMappedArray(
            &cuda_array, val, 0, 0);
        r != CUDA_SUCCESS) {
        sc::cuda().cuGetErrorString(r, &err_str);
        throw std::runtime_error {
            "CUDA: cuGraphicsSubResourceGetMappedArray dailed - "s + err_str
        };
    }

    f(cuda_array);
}

} // namespace cu

struct DRMCudaCaptureSource
{
    using CaptureResultType = exios::Result<AVFrame*, std::error_code>;

    DRMCudaCaptureSource(exios::Context context,
                         Parameters const& params,
                         VideoOutputSize output_size,
                         VideoOutputScale output_scale,
                         CUcontext cuda_ctx,
                         EGLDisplay egl_display,
                         EGLSurface egl_surface) noexcept;

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
    EGLSurface egl_surface_;
    std::size_t frame_number_ { 0 };
    sigset_t drm_proc_mask_ {};
    Process drm_process_;
    UnixSocket drm_socket_;
    ColorConverter color_converter_;
    cu::GraphicsResource cuda_gfx_resource_;
};

} // namespace sc
#endif // SHADOW_CAST_DRM_CUDA_CAPTURE_SOURCE_HPP_INCLUDED
