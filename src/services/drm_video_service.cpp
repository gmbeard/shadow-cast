#include "services/drm_video_service.hpp"
#include "drm.hpp"
#include "gl/object.hpp"
#include "gl/texture.hpp"
#include "io/accept_handler.hpp"
#include "io/message_receiver.hpp"
#include "io/message_sender.hpp"
#include "nvidia/cuda.hpp"
#include "platform/egl.hpp"
#include "platform/opengl.hpp"
#include "services/color_converter.hpp"
#include "utils/contracts.hpp"
#include "utils/result.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <filesystem>
#include <libdrm/drm_fourcc.h>
#include <system_error>
#include <vector>

#ifdef SHADOW_CAST_ENABLE_METRICS
#include "utils/elapsed.hpp"
#endif

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

auto register_gl_texture_in_cuda(sc::NvCuda const& nvcuda,
                                 CUcontext ctx,
                                 GLuint texture_name,
                                 CUgraphicsResource& cuda_gfx_resource,
                                 CUarray& cuda_array) -> void
{
    using namespace std::literals::string_literals;

    char const* err_str = "unknown";

    CUcontext old_ctx;
    /*CUresult res = */ nvcuda.cuCtxPushCurrent_v2(ctx);
    SC_SCOPE_GUARD([&] { nvcuda.cuCtxPopCurrent_v2(&old_ctx); });

    if (auto const r = nvcuda.cuGraphicsGLRegisterImage(
            &cuda_gfx_resource,
            texture_name,
            GL_TEXTURE_2D,
            CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY);
        r != CUDA_SUCCESS) {
        nvcuda.cuGetErrorString(r, &err_str);
        throw std::runtime_error {
            "CUDA: Failed to register image with CUDA - "s + err_str
        };
    }

    if (auto const r = nvcuda.cuGraphicsResourceSetMapFlags(
            cuda_gfx_resource, CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY);
        r != CUDA_SUCCESS) {
        nvcuda.cuGetErrorString(r, &err_str);
        throw std::runtime_error { "CUDA: Failed to set map flags - "s +
                                   err_str };
    }

    if (auto const r = nvcuda.cuGraphicsMapResources(1, &cuda_gfx_resource, 0);
        r != CUDA_SUCCESS) {
        nvcuda.cuGetErrorString(r, &err_str);
        throw std::runtime_error { "CUDA: Failed to set map flags - "s +
                                   err_str };
    }

    if (auto const r = nvcuda.cuGraphicsSubResourceGetMappedArray(
            &cuda_array, cuda_gfx_resource, 0, 0);
        r != CUDA_SUCCESS) {
        nvcuda.cuGetErrorString(r, &err_str);
        throw std::runtime_error { "CUDA: Failed to get mapped array - "s +
                                   err_str };
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

DRMVideoService::DRMVideoService(
    NvCuda nvcuda,
    CUcontext cuda_ctx,
    EGL& egl,
    Wayland& wayland,
    WaylandEGL& plaform_egl SC_METRICS_PARAM_DEFINE(MetricsService*,
                                                    metrics_service)) noexcept
    : color_converter_ { wayland.output_width, wayland.output_height }
    , nvcuda_ { nvcuda }
    , cuda_ctx_ { cuda_ctx }
    , egl_ { &egl }
    , wayland_ { &wayland }
    , platform_egl_ { &plaform_egl } // clang-format off
    SC_METRICS_MEMBER_USE(metrics_service, metrics_service_)
    SC_METRICS_MEMBER_USE(0, metrics_start_time_)
// clang-format on
{
#ifdef SHADOW_CAST_ENABLE_METRICS
    SC_EXPECT(metrics_service_);
#endif
}

auto DRMVideoService::on_init(ReadinessRegister reg) -> void
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

    egl_->eglSwapInterval(platform_egl_->egl_display.get(), 0);

    reg(FrameTimeRatio(1), &dispatch_frame);
    frame_time_ = reg.frame_time();

#ifdef SHADOW_CAST_ENABLE_METRICS
    metrics_start_time_ = global_elapsed.nanosecond_value();
#endif
}

auto DRMVideoService::on_uninit() noexcept -> void
{
    static_cast<void>(drm_process_.terminate_and_wait());
    drm_socket_.close();
}

auto DRMVideoService::dispatch_frame(Service& svc) -> void
{
    auto& self = static_cast<DRMVideoService&>(svc);
    if (!self.frame_handler_)
        return;

#ifdef SHADOW_CAST_ENABLE_METRICS
    static std::size_t frame_num = 0;
    self.frame_timer_.reset();
    auto const frame_timestamp =
        global_elapsed.nanosecond_value() - self.metrics_start_time_;
#endif

    auto const r =
        get_drm_data(self.drm_socket_, kDRMDataTimeoutMs, &self.drm_proc_mask_);

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

    auto const& descriptor = std::max_element(
        std::begin(drm_data.descriptors),
        std::next(std::begin(drm_data.descriptors), drm_data.num_fds),
        [](auto const& a, auto const& b) {
            return (a.width * a.height) < (b.width * b.height);
        });

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
        self.egl_->eglCreateImage(self.platform_egl_->egl_display.get(),
                                  EGL_NO_CONTEXT,
                                  EGL_LINUX_DMA_BUF_EXT,
                                  static_cast<EGLClientBuffer>(nullptr),
                                  img_attr);

    if (input_image == EGL_NO_IMAGE) {
        throw std::runtime_error { "eglCreateImage input failed: " +
                                   std::to_string(self.egl_->eglGetError()) };
    }

    opengl::bind(opengl::TextureTarget<GL_TEXTURE_EXTERNAL_OES> {},
                 self.color_converter_.input_texture(),
                 [&](auto) {
                     gl().glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES,
                                                       input_image);
                 });

    self.color_converter_.convert();

    SC_SCOPE_GUARD([&] {
        self.egl_->eglDestroyImage(self.platform_egl_->egl_display.get(),
                                   input_image);
    });

    register_gl_texture_in_cuda(
        self.nvcuda_,                                  /* in */
        self.cuda_ctx_,                                /* in */
        self.color_converter_.output_texture().name(), /* in */
        self.cuda_gfx_resource_,                       /* out */
        self.cuda_array_);                             /* out */

    SC_SCOPE_GUARD([&] {
        CUcontext old_ctx;
        self.nvcuda_.cuCtxPushCurrent_v2(self.cuda_ctx_);
        SC_SCOPE_GUARD([&] { self.nvcuda_.cuCtxPopCurrent_v2(&old_ctx); });

        if (self.cuda_gfx_resource_) {
            self.nvcuda_.cuGraphicsUnmapResources(
                1, &self.cuda_gfx_resource_, 0);
            self.nvcuda_.cuGraphicsUnregisterResource(self.cuda_gfx_resource_);
            self.cuda_gfx_resource_ = nullptr;
        }
    });

    (*self.frame_handler_)(self.cuda_array_, self.nvcuda_, self.frame_time_);

#ifdef SHADOW_CAST_ENABLE_METRICS
    self.metrics_service_->post_time_metric(
        { .category = 1,
          .id = ++frame_num,
          .timestamp_ns = frame_timestamp,
          .nanoseconds = self.frame_timer_.nanosecond_value(),
          .frame_size = 1,
          .frame_count = 1 });
#endif
}

} // namespace sc
