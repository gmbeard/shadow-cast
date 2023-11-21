#include "services/drm_video_service.hpp"
#include "drm.hpp"
#include "io/accept_handler.hpp"
#include "io/message_receiver.hpp"
#include "io/message_sender.hpp"
#include "utils/result.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <libdrm/drm_fourcc.h>
#include <system_error>
#include <vector>

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

auto register_egl_image_in_cuda(sc::NvCuda const& nvcuda,
                                CUcontext ctx,
                                EGLImage egl_image,
                                CUgraphicsResource& cuda_gfx_resource,
                                CUarray& cuda_array) -> void
{
    using namespace std::literals::string_literals;

    char const* err_str = "unknown";

    CUcontext old_ctx;
    /*CUresult res = */ nvcuda.cuCtxPushCurrent_v2(ctx);
    SC_SCOPE_GUARD([&] { nvcuda.cuCtxPopCurrent_v2(&old_ctx); });

    if (auto const r = nvcuda.cuGraphicsEGLRegisterImage(
            &cuda_gfx_resource,
            egl_image,
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

    if (auto const r = nvcuda.cuGraphicsSubResourceGetMappedArray(
            &cuda_array, cuda_gfx_resource, 0, 0);
        r != CUDA_SUCCESS) {
        nvcuda.cuGetErrorString(r, &err_str);
        throw std::runtime_error { "CUDA: Failed to get mapped array - "s +
                                   err_str };
    }
}

} // namespace

namespace sc
{

DRMVideoService::DRMVideoService(NvCuda nvcuda,
                                 CUcontext cuda_ctx,
                                 EGL& egl,
                                 Wayland& wayland,
                                 WaylandEGL& plaform_egl) noexcept
    : nvcuda_ { nvcuda }
    , cuda_ctx_ { cuda_ctx }
    , egl_ { &egl }
    , wayland_ { &wayland }
    , platform_egl_ { &plaform_egl }
{
}

auto DRMVideoService::on_init(ReadinessRegister reg) -> void
{
    /* SIGCHLD should be blocked before `on_init()` is
     * called...
     */
    sigemptyset(&drm_proc_mask_);
    sigaddset(&drm_proc_mask_, SIGCHLD);

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
    std::vector<std::string> args { kDRMBin, kSocketPath };
    drm_process_ = sc::spawn_process(std::span { args.data(), args.size() });
    auto socket_result = server_socket.use_with(
        sc::AcceptHandler { kDRMConnectTimeoutMs, &drm_proc_mask_ });

    if (!socket_result)
        throw std::system_error { sc::get_error(socket_result) };

    drm_socket_ = UnixSocket { sc::get_value(socket_result) };

    egl_->eglSwapInterval(platform_egl_->egl_display.get(), 0);

    reg(FrameTimeRatio(1), &dispatch_frame);
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
    const std::intptr_t img_attr[] = {
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888 /*descriptor.pixel_format*/,
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

    EGLImage image =
        self.egl_->eglCreateImage(self.platform_egl_->egl_display.get(),
                                  EGL_NO_CONTEXT,
                                  EGL_LINUX_DMA_BUF_EXT,
                                  static_cast<EGLClientBuffer>(nullptr),
                                  img_attr);

    if (image == EGL_NO_IMAGE) {
        throw std::runtime_error { "eglCreateImage failed: " +
                                   std::to_string(self.egl_->eglGetError()) };
    }

    SC_SCOPE_GUARD([&] {
        self.egl_->eglDestroyImage(self.platform_egl_->egl_display.get(),
                                   image);
    });

    register_egl_image_in_cuda(self.nvcuda_,            /* in */
                               self.cuda_ctx_,          /* in */
                               image,                   /* in */
                               self.cuda_gfx_resource_, /* out */
                               self.cuda_array_);       /* out */

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

    (*self.frame_handler_)(self.cuda_array_, self.nvcuda_);
}

} // namespace sc
