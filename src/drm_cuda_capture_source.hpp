#ifndef SHADOW_CAST_DRM_CUDA_CAPTURE_SOURCE_HPP_INCLUDED
#define SHADOW_CAST_DRM_CUDA_CAPTURE_SOURCE_HPP_INCLUDED

#include "av/codec.hpp"
#include "capture_source.hpp"
#include "color_converter.hpp"
#include "cuda.hpp"
#include "drm/planes.hpp"
#include "exios/context.hpp"
#include "frame_timer.hpp"
#include "gl/texture.hpp"
#include "io/process.hpp"
#include "io/unix_socket.hpp"
#include "lru_map.hpp"
#include "nvidia/cuda.hpp"
#include "platform/egl.hpp"
#include "sticky_cancel_timer.hpp"
#include "utils/cmd_line.hpp"
#include "utils/scope_guard.hpp"
#include <EGL/egl.h>
#include <chrono>
#include <cstdint>
#include <sys/types.h>
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

namespace buf
{
struct Item
{
    Item(EGLDisplay disp,
         EGLImage img,
         opengl::Texture tex,
         std::size_t frame_number) noexcept
        : display { std::move(disp) }
        , image { std::move(img) }
        , texture { std::move(tex) }
        , used_on_frame_number { frame_number }
    {
        opengl::bind(opengl::TextureTarget<GL_TEXTURE_EXTERNAL_OES> {},
                     texture,
                     [&](auto) {
                         gl().glEGLImageTargetTexture2DOES(
                             GL_TEXTURE_EXTERNAL_OES, image);
                     });
    }

    Item(Item&& other) noexcept
        : display { other.display }
        , image { std::exchange(other.image, EGL_NO_IMAGE) }
        , texture { std::move(other.texture) }
        , used_on_frame_number { other.used_on_frame_number }
    {
    }

    ~Item()
    {
        if (image != EGL_NO_IMAGE)
            sc::egl().eglDestroyImage(display, image);
    }

    friend auto swap(Item& lhs, Item& rhs) noexcept -> void
    {
        using std::swap;
        swap(lhs.display, rhs.display);
        swap(lhs.image, rhs.image);
        swap(lhs.texture, rhs.texture);
        swap(lhs.used_on_frame_number, rhs.used_on_frame_number);
    }

    auto operator=(Item&& rhs) noexcept -> Item&
    {
        Item tmp { std::move(rhs) };
        swap(*this, tmp);
        return *this;
    }

    EGLDisplay display;
    EGLImage image;
    opengl::Texture texture;
    std::size_t used_on_frame_number;
};

} // namespace buf

struct DRMCudaCaptureSource
{
    using CaptureResultType = exios::Result<AVFrame*, std::error_code>;

    DRMCudaCaptureSource(exios::Context context,
                         Parameters const& params,
                         VideoOutputSize output_size,
                         VideoOutputScale output_scale,
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
    auto capture(AVFrame* frame,
                 frame_timer frame_budget,
                 Completion completion) -> void
    {
        capture_(frame,
                 frame_budget,
                 &completion_proxy_<std::decay_t<Completion>>,
                 &completion);
    }

private:
    auto get_image_buffer(PlaneDescriptor const& descriptor) -> buf::Item&;
    auto flush_stale_image_buffers() -> void;

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
             frame_timer frame_budget,
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
    cu::GraphicsResource cuda_gfx_resource_;
    lru_map<std::uint32_t, buf::Item> image_buffer_;
    std::size_t image_buffer_cache_hits_ { 0 };
    std::size_t image_buffer_largest_size_ { 0 };
};

} // namespace sc
#endif // SHADOW_CAST_DRM_CUDA_CAPTURE_SOURCE_HPP_INCLUDED
