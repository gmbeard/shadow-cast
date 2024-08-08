#include "drm_cuda_capture_source.hpp"
#include "av/codec.hpp"
#include "cuda.hpp"
#include "drm/messaging.hpp"
#include "io/accept_handler.hpp"
#include "io/message_sender.hpp"
#include "io/unix_socket.hpp"
#include "platform/egl.hpp"
#include "services/color_converter.hpp"
#include "utils/cmd_line.hpp"
#include <GL/gl.h>
#include <filesystem>

namespace
{
char constexpr kSocketPath[] = "/tmp/shadow-cast.sock";
std::size_t constexpr kDRMConnectTimeoutMs = 1'000;
std::size_t constexpr kDRMDataTimeoutMs = 1'000;
char constexpr kDRMBin[] = "shadow-cast-kms";

auto get_drm_data(sc::UnixSocket& socket, std::size_t timeout, sigset_t* mask)
    -> sc::Result<sc::DRMResponse, std::error_code>
{
    sc::DRMRequest request { sc::drm_request::kGetPlanes };
    sc::DRMResponse response {};

    auto const send_result = socket.use_with(
        sc::MessageSender<sc::DRMRequest> { request, timeout, mask });

    if (!send_result) {
        return send_result.error();
    }

    if (sc::get_value(send_result) < sizeof(request)) {
        return sc::result_error(
            std::error_code { EAGAIN, std::system_category() });
    }

    auto const recv_result =
        socket.use_with(sc::DRMResponseReceiver { response, timeout, mask });

    if (!recv_result) {
        return recv_result.error();
    }

    if (sc::get_value(recv_result) < sizeof(response)) {
        return sc::result_error(
            std::error_code { EAGAIN, std::system_category() });
    }

    return sc::result_ok(response);
}

auto register_gl_texture_in_cuda(CUcontext ctx,
                                 GLuint texture_name,
                                 CUgraphicsResource& cuda_gfx_resource,
                                 CUarray& cuda_array) -> void
{
    using namespace std::literals::string_literals;

    char const* err_str = "unknown";

    CUcontext old_ctx;
    /*CUresult res = */ sc::cuda().cuCtxPushCurrent_v2(ctx);
    SC_SCOPE_GUARD([&] { sc::cuda().cuCtxPopCurrent_v2(&old_ctx); });

    if (auto const r = sc::cuda().cuGraphicsGLRegisterImage(
            &cuda_gfx_resource,
            texture_name,
            GL_TEXTURE_2D,
            CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY);
        r != CUDA_SUCCESS) {
        sc::cuda().cuGetErrorString(r, &err_str);
        throw std::runtime_error {
            "CUDA: Failed to register image with CUDA - "s + err_str
        };
    }

    if (auto const r = sc::cuda().cuGraphicsResourceSetMapFlags(
            cuda_gfx_resource, CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY);
        r != CUDA_SUCCESS) {
        sc::cuda().cuGetErrorString(r, &err_str);
        throw std::runtime_error { "CUDA: Failed to set map flags - "s +
                                   err_str };
    }

    if (auto const r =
            sc::cuda().cuGraphicsMapResources(1, &cuda_gfx_resource, 0);
        r != CUDA_SUCCESS) {
        sc::cuda().cuGetErrorString(r, &err_str);
        throw std::runtime_error { "CUDA: cuGraphicsMapResources failed - "s +
                                   err_str };
    }

    if (auto const r = sc::cuda().cuGraphicsSubResourceGetMappedArray(
            &cuda_array, cuda_gfx_resource, 0, 0);
        r != CUDA_SUCCESS) {
        sc::cuda().cuGetErrorString(r, &err_str);
        throw std::runtime_error {
            "CUDA: cuGraphicsSubResourceGetMappedArray dailed - "s + err_str
        };
    }
}
auto find_drm_helper_binary()
{
    using namespace std::string_literals;
    namespace fs = std::filesystem;

    /* Construct a path to the KMS binary using the path
     * of the main executable...
     */
    auto kms_bin_dir = fs::read_symlink("/proc/self/exe");
    kms_bin_dir.remove_filename();
    auto const kms_bin_path = kms_bin_dir / kDRMBin;
    if (!fs::exists(kms_bin_path))
        throw new std::runtime_error { "Couldn't locate DRM helper at "s +
                                       kms_bin_path.string() };

    return kms_bin_path;
}
} // namespace

namespace sc
{

DRMCudaCaptureSource::DRMCudaCaptureSource(exios::Context context,
                                           Parameters const& params,
                                           VideoOutputSize output_size,
                                           VideoOutputScale output_scale,
                                           CUcontext cuda_ctx,
                                           EGLDisplay egl_display) noexcept
    : ctx_ { context }
    , timer_ { context }
    , frame_interval_ { params.frame_time.value() }
    , cuda_ctx_ { cuda_ctx }
    , egl_display_ { std::move(egl_display) }
    , color_converter_ { output_size.width,
                         output_size.height,
                         output_scale.width,
                         output_scale.height }
{
}

auto DRMCudaCaptureSource::context() const noexcept -> exios::Context const&
{
    return ctx_;
}

auto DRMCudaCaptureSource::cancel() noexcept -> void { timer_.cancel(); }

auto DRMCudaCaptureSource::timer() noexcept -> StickyCancelTimer&
{
    return timer_;
}

auto DRMCudaCaptureSource::interval() const noexcept -> std::chrono::nanoseconds
{
    return frame_interval_;
}

auto DRMCudaCaptureSource::init() -> void
{
    /* SIGCHLD should be blocked before `on_init()` is
     * called...
     */
    sigemptyset(&drm_proc_mask_);
    sigaddset(&drm_proc_mask_, SIGCHLD);

    color_converter_.initialize();

    auto server_socket = sc::socket::bind(kSocketPath);

    /* We don't need the listening socket outside
     * of this function; The child process either
     * connects successfully, in which case we don't
     * accept any more connections, or it fails
     * to connect and we can't proceed anyway...
     */
    SC_SCOPE_GUARD([&] {
        server_socket.close();
        unlink(kSocketPath);
    });

    server_socket.listen();

    /* Spawn the DRM child process and wait for it
     * to attach itself...
     */
    std::vector<std::string> args { find_drm_helper_binary(), kSocketPath };
    drm_process_ = sc::spawn_process(std::span { args.data(), args.size() });
    auto socket_result = server_socket.use_with(
        sc::AcceptHandler { kDRMConnectTimeoutMs, &drm_proc_mask_ });

    if (!socket_result)
        throw std::system_error { sc::get_error(socket_result) };

    drm_socket_ = UnixSocket { sc::get_value(socket_result) };

    egl().eglSwapInterval(egl_display_, 0);
}

auto DRMCudaCaptureSource::deinit() -> void
{
    static_cast<void>(drm_process_.terminate_and_wait());
    drm_socket_.close();
}

auto DRMCudaCaptureSource::capture_(
    AVFrame* frame,
    auto (*completion)(DRMCudaCaptureSource&, AVFrame*, void*)->void,
    void* data) -> void
{
    /* WARN:
     *  This function _must_ complete synchronously. It must not do any "stack
     * ripping" because `data` is a pointer to a callback in the current stack
     * frame.
     */

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
    auto const frame_start = global_elapsed.nanosecond_value();
#endif

    auto const r =
        get_drm_data(drm_socket_, kDRMDataTimeoutMs, &drm_proc_mask_);

    if (!r) {
        if (sc::get_error(r).value() == EINTR)
            return;

        throw std::system_error { r.error() };
    }

    auto const drm_data = sc::get_value(r);

    if (!drm_data.num_fds)
        throw std::runtime_error { "No DRM planes received" };

    SC_SCOPE_GUARD([&] {
        /* If we received any dma-buf fds then it is
         * our responsibility to close them...
         */
        for (decltype(drm_data.num_fds) i = 0; i < drm_data.num_fds; ++i)
            ::close(drm_data.descriptors[i].fd);
    });

    auto begin_descriptors = std::begin(drm_data.descriptors);
    auto end_descriptors =
        std::next(std::begin(drm_data.descriptors), drm_data.num_fds);

    auto const& descriptor = std::max_element(
        begin_descriptors, end_descriptors, [](auto const& a, auto const& b) {
            return (a.width * a.height) < (b.width * b.height);
        });

    auto const mouse_plane_position =
        std::find_if(begin_descriptors, end_descriptors, [](auto const& plane) {
            return plane.is_flag_set(sc::plane_flags::IS_CURSOR);
        });

    auto const has_mouse_plane = mouse_plane_position != end_descriptors;

    // clang-format off
    std::intptr_t const img_attr[] = {
        EGL_LINUX_DRM_FOURCC_EXT, descriptor->pixel_format,
        EGL_WIDTH, descriptor->width,
        EGL_HEIGHT, descriptor->height,
        EGL_DMA_BUF_PLANE0_FD_EXT, descriptor->fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, descriptor->offset,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, descriptor->pitch,
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<std::uint32_t>(descriptor->modifier & 0xffffffffull),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<std::uint32_t>(descriptor->modifier >> 32ull),
        EGL_NONE
    };
    // clang-format on

    EGLImage input_image =
        egl().eglCreateImage(egl_display_,
                             EGL_NO_CONTEXT,
                             EGL_LINUX_DMA_BUF_EXT,
                             static_cast<EGLClientBuffer>(nullptr),
                             img_attr);

    if (input_image == EGL_NO_IMAGE) {
        throw std::runtime_error { "eglCreateImage input failed: " +
                                   std::to_string(egl().eglGetError()) };
    }

    opengl::bind(opengl::TextureTarget<GL_TEXTURE_EXTERNAL_OES> {},
                 color_converter_.input_texture(),
                 [&](auto) {
                     gl().glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES,
                                                       input_image);
                 });

    EGLImage mouse_image = EGL_NO_IMAGE;

    std::optional<MouseParameters> mouse_params {};

    if (has_mouse_plane) {
        auto const& mouse_descriptor = *mouse_plane_position;
        // clang-format off
        std::intptr_t const mouse_img_attr[] = {
            EGL_LINUX_DRM_FOURCC_EXT, mouse_descriptor.pixel_format,
            EGL_WIDTH, mouse_descriptor.width,
            EGL_HEIGHT, mouse_descriptor.height,
            EGL_DMA_BUF_PLANE0_FD_EXT, mouse_descriptor.fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, mouse_descriptor.offset,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, mouse_descriptor.pitch,
            EGL_NONE
        };
        // clang-format on

        mouse_image =
            egl().eglCreateImage(egl_display_,
                                 EGL_NO_CONTEXT,
                                 EGL_LINUX_DMA_BUF_EXT,
                                 static_cast<EGLClientBuffer>(nullptr),
                                 mouse_img_attr);

        if (mouse_image == EGL_NO_IMAGE) {
            throw std::runtime_error { "eglCreateImage input failed: " +
                                       std::to_string(egl().eglGetError()) };
        }

        opengl::bind(opengl::TextureTarget<GL_TEXTURE_EXTERNAL_OES> {},
                     color_converter_.mouse_texture(),
                     [&](auto) {
                         gl().glEGLImageTargetTexture2DOES(
                             GL_TEXTURE_EXTERNAL_OES, mouse_image);
                     });

        mouse_params = MouseParameters { .width = mouse_descriptor.width,
                                         .height = mouse_descriptor.height,
                                         .x = mouse_descriptor.x,
                                         .y = mouse_descriptor.y };
    }

    color_converter_.convert(mouse_params);

    SC_SCOPE_GUARD([&] {
        egl().eglDestroyImage(egl_display_, input_image);
        if (has_mouse_plane) {
            egl().eglDestroyImage(egl_display_, mouse_image);
        }
    });

    CUgraphicsResource cuda_gfx_resource { nullptr };
    CUarray cuda_array { nullptr };

    register_gl_texture_in_cuda(
        cuda_ctx_,                                /* in */
        color_converter_.output_texture().name(), /* in */
        cuda_gfx_resource,                        /* out */
        cuda_array);                              /* out */

    SC_SCOPE_GUARD([&] {
        CUcontext old_ctx;
        cuda().cuCtxPushCurrent_v2(cuda_ctx_);
        SC_SCOPE_GUARD([&] { cuda().cuCtxPopCurrent_v2(&old_ctx); });

        if (cuda_gfx_resource) {
            cuda().cuGraphicsUnmapResources(1, &cuda_gfx_resource, 0);
            cuda().cuGraphicsUnregisterResource(cuda_gfx_resource);
            cuda_gfx_resource = nullptr;
        }
    });

    CUDA_MEMCPY2D memcpy_struct {};

    memcpy_struct.srcXInBytes = 0;
    memcpy_struct.srcY = 0;
    memcpy_struct.srcMemoryType = CU_MEMORYTYPE_ARRAY;
    memcpy_struct.dstXInBytes = 0;
    memcpy_struct.dstY = 0;
    memcpy_struct.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    memcpy_struct.srcArray = cuda_array;
    memcpy_struct.dstDevice = reinterpret_cast<CUdeviceptr>(frame->data[0]);
    memcpy_struct.dstPitch = frame->linesize[0];
    memcpy_struct.WidthInBytes = frame->linesize[0];
    memcpy_struct.Height = frame->height;

    if (auto const cuda_result = cuda().cuMemcpy2D_v2(&memcpy_struct);
        cuda_result != CUDA_SUCCESS) {
        char const* err = "unknown";
        cuda().cuGetErrorString(cuda_result, &err);
        throw std::runtime_error {
            std::to_string(frame_number_) +
            std::string { " Failed to copy CUDA buffer: " } + err
        };
    }

    frame->pts = frame_number_++;

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
    metrics::add_frame_time(metrics::video_metrics,
                            global_elapsed.nanosecond_value() - frame_start);
#endif

    completion(*this, frame, data);
}

} // namespace sc
