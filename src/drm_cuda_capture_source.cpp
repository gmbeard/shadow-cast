#include "drm_cuda_capture_source.hpp"
#include "av/codec.hpp"
#include "color_converter.hpp"
#include "config.hpp"
#include "cuda.hpp"
#include "drm/messaging.hpp"
#include "drm/planes.hpp"
#include "frame_timer.hpp"
#include "gl/object.hpp"
#include "gl/texture.hpp"
#include "io/accept_handler.hpp"
#include "io/message_sender.hpp"
#include "io/unix_socket.hpp"
#include "logging.hpp"
#include "metrics/profiling.hpp"
#include "nvidia/cuda.hpp"
#include "platform/egl.hpp"
#include "utils/cmd_line.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace sc::cu
{
GraphicsResource::GraphicsResource(CUgraphicsResource value) noexcept
    : value_ { value }
{
}

GraphicsResource::GraphicsResource(GraphicsResource&& other) noexcept
    : value_ { std::exchange(other.value_, nullptr) }
{
}

GraphicsResource::~GraphicsResource()
{
    if (value_) {
        cuda().cuGraphicsUnregisterResource(value_);
    }
}

auto GraphicsResource::operator=(GraphicsResource&& other) noexcept
    -> GraphicsResource&
{
    using std::swap;
    auto tmp { std::move(other) };

    swap(value_, tmp.value_);
    return *this;
}

GraphicsResource::operator bool() const noexcept { return value_ != nullptr; }

GraphicsResource::operator CUgraphicsResource() const noexcept
{
    return value_;
}

auto graphics_gl_register_image(unsigned int texture_name,
                                unsigned int texture_target,
                                unsigned int flags) -> GraphicsResource
{
    using namespace std::string_literals;

    char const* err_str = "unknown";
    CUgraphicsResource resource;

    if (auto const r = cuda().cuGraphicsGLRegisterImage(
            &resource, texture_name, texture_target, flags);
        r != CUDA_SUCCESS) {
        cuda().cuGetErrorString(r, &err_str);
        throw std::runtime_error {
            "CUDA: Failed to register image with CUDA - "s + err_str
        };
    }

    return GraphicsResource { resource };
}

} // namespace sc::cu

namespace
{
char constexpr kSocketPath[] = "/tmp/shadow-cast.sock";
std::size_t constexpr kDRMConnectTimeoutMs = 1'000;
std::size_t constexpr kDRMDataTimeoutMs = 1'000;
char constexpr kDRMBin[] = "shadow-cast-kms";
std::size_t constexpr kDRMImageIsStaleAfterFramesUnused = 10;

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

auto find_drm_helper_binary()
{
    using namespace std::string_literals;
    namespace fs = std::filesystem;

    /* Construct a path to the KMS binary using the path
     * of the main executable...
     */
    auto kms_bin_dir = fs::read_symlink("/proc/self/exe");
    kms_bin_dir.remove_filename();
    auto kms_bin_path = kms_bin_dir / kDRMBin;
    sc::log(sc::LogLevel::debug,
            "Checking for %s at %s",
            kDRMBin,
            kms_bin_path.c_str());
    if (fs::exists(kms_bin_path)) {
        sc::log(sc::LogLevel::debug,
                "Found %s at %s",
                kDRMBin,
                kms_bin_path.c_str());
        return kms_bin_path;
    }

    kms_bin_path = fs::path(sc::KLibExecDir) / kDRMBin;
    sc::log(sc::LogLevel::debug,
            "Checking for %s at %s",
            kDRMBin,
            kms_bin_path.c_str());
    if (fs::exists(kms_bin_path)) {
        sc::log(sc::LogLevel::debug,
                "Found %s at %s",
                kDRMBin,
                kms_bin_path.c_str());
        return kms_bin_path;
    }

    throw new std::runtime_error { "Couldn't locate DRM helper" };
}

auto copy_texture_to_frame(CUcontext cuda_context,
                           sc::cu::GraphicsResource& cuda_gfx_resource,
                           AVFrame* frame) -> void
{
    sc::cu::with_cuda_context(cuda_context, [&] {
        sc::cu::graphics_map_resource_array(
            cuda_gfx_resource,
            CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY,
            [&](auto cuda_array) {
                CUDA_MEMCPY2D memcpy_struct {};

                memcpy_struct.srcXInBytes = 0;
                memcpy_struct.srcY = 0;
                memcpy_struct.srcMemoryType = CU_MEMORYTYPE_ARRAY;
                memcpy_struct.dstXInBytes = 0;
                memcpy_struct.dstY = 0;
                memcpy_struct.dstMemoryType = CU_MEMORYTYPE_DEVICE;
                memcpy_struct.srcArray = cuda_array;
                memcpy_struct.dstDevice =
                    reinterpret_cast<CUdeviceptr>(frame->data[0]);
                memcpy_struct.dstPitch = frame->linesize[0];
                memcpy_struct.WidthInBytes = frame->linesize[0];
                memcpy_struct.Height = frame->height;

                if (auto const cuda_result =
                        sc::cuda().cuMemcpy2D_v2(&memcpy_struct);
                    cuda_result != CUDA_SUCCESS) {
                    char const* err = "unknown";
                    sc::cuda().cuGetErrorString(cuda_result, &err);
                    throw std::runtime_error {
                        std::string { " Failed to copy CUDA buffer: " } + err
                    };
                }
            });
    });
}

struct SyncFence
{
    SyncFence(SyncFence&&) = delete;

    explicit SyncFence(int fence_fd, EGLDisplay const& display) noexcept
        : display_ { display }
    {
        std::intptr_t const sync_attr[] = { EGL_SYNC_NATIVE_FENCE_FD_ANDROID,
                                            fence_fd,
                                            EGL_NONE };
        fence_ = sc::egl().eglCreateSync(
            display_, EGL_SYNC_NATIVE_FENCE_ANDROID, sync_attr);

        if (fence_ == EGL_NO_SYNC)
            ::close(fence_fd);
    }

    ~SyncFence()
    {
        if (fence_ != EGL_NO_SYNC) {
            sc::egl().eglDestroySync(display_, fence_);
        }
    }

    auto operator=(SyncFence&&) -> SyncFence& = delete;

    operator bool() const noexcept { return fence_ != EGL_NO_SYNC; }

    operator EGLSync() const noexcept { return fence_; }

private:
    EGLDisplay display_;
    EGLSync fence_ { EGL_NO_SYNC };
};

[[nodiscard]] auto
try_wait_sync_fence(EGLDisplay display,
                    EGLSync fence,
                    sc::frame_timer const& frame_budget) noexcept -> bool
{
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;

    if (!sc::egl().eglWaitSync(display, fence, 0)) {
        sc::log(sc::LogLevel::warn, "EGL server sync failed!");
    }

    auto const wait_time_ns =
        duration_cast<nanoseconds>(
            frame_budget.duration_until_next_frame_from(frame_budget.now()))
            .count();

    auto const wait_result = sc::egl().eglClientWaitSync(
        display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT, wait_time_ns);

    if (wait_result == EGL_TIMEOUT_EXPIRED) {
        sc::log(sc::LogLevel::warn, "EGL sync timed out!");
        return false;
    }

    return true;
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
    , image_buffer_ { std::size_t(params.drm_cache_size) }
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

    cu::with_cuda_context(cuda_ctx_, [&] {
        cuda_gfx_resource_ = cu::graphics_gl_register_image(
            color_converter_.output_texture().name(),
            GL_TEXTURE_2D,
            CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY);
    });

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

    /* TODO:
     * Is this needed if we're doing off-screen rendering?...
     * egl().eglSwapInterval(egl_display_, 0);
     */

    auto const r =
        WITH_PROFILE(metrics::ProfileSectionId::wayland_fetch_drm_data, [&] {
            return get_drm_data(
                drm_socket_, kDRMDataTimeoutMs, &drm_proc_mask_);
        });

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
        for (decltype(drm_data.num_fds) i = 0; i < drm_data.num_fds; ++i) {
            ::close(drm_data.descriptors[i].fd);
        }
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

    ::close(descriptor->sync_fd);
    if (has_mouse_plane) {
        /* We don't care about explicit sync for the mouse pointer plane...
         */
        ::close(mouse_plane_position->sync_fd);
    }
}

auto DRMCudaCaptureSource::flush_stale_image_buffers() -> void
{
    for (auto pos = image_buffer_.rbegin(); pos != image_buffer_.rend();
         ++pos) {
        buf::Item& image = std::get<1>(*pos);
        SC_EXPECT(image.used_on_frame_number <= frame_number_);
        auto const unused_for = frame_number_ - image.used_on_frame_number;
        if (unused_for >= kDRMImageIsStaleAfterFramesUnused) {
            log(LogLevel::debug,
                "Removing stale DRM image %u. Unused for %llu frames",
                std::get<0>(*pos),
                unused_for);
            image_buffer_.erase(--(pos.base()));
        }
    };
}

auto DRMCudaCaptureSource::get_image_buffer(PlaneDescriptor const& descriptor)
    -> buf::Item&
{
    auto pos = image_buffer_.find(descriptor.fb_id);
    if (pos == image_buffer_.end()) {

        image_buffer_largest_size_ =
            std::max(image_buffer_largest_size_, image_buffer_.size());
        flush_stale_image_buffers();

        // clang-format off
        std::intptr_t const img_attr[] = {
            EGL_LINUX_DRM_FOURCC_EXT, descriptor.pixel_format,
            EGL_WIDTH, descriptor.width,
            EGL_HEIGHT, descriptor.height,
            EGL_DMA_BUF_PLANE0_FD_EXT, descriptor.fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, descriptor.offset,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, descriptor.pitch,
            EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<std::uint32_t>(descriptor.modifier & 0xffffffffull),
            EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<std::uint32_t>(descriptor.modifier >> 32ull),
            EGL_NONE
        };
        // clang-format on
        auto image = egl().eglCreateImage(egl_display_,
                                          EGL_NO_CONTEXT,
                                          EGL_LINUX_DMA_BUF_EXT,
                                          static_cast<EGLClientBuffer>(nullptr),
                                          img_attr);
        if (image == EGL_NO_IMAGE) {
            throw std::runtime_error { "eglCreateImage input failed: " +
                                       std::to_string(egl().eglGetError()) };
        }

        auto [item, inserted] =
            image_buffer_.insert(descriptor.fb_id,
                                 buf::Item { egl_display_,
                                             image,
                                             opengl::create<opengl::Texture>(),
                                             frame_number_ });
        SC_EXPECT(inserted);
        pos = item;
    }
    else {
        image_buffer_cache_hits_ += 1;
        pos->second.used_on_frame_number = frame_number_;
    }

    return pos->second;
}

auto DRMCudaCaptureSource::deinit() -> void
{
    log(LogLevel::info,
        "DRM image cache hits: %llu/%llu frames (%6.2f%%). Largest "
        "size: %llu/%llu",
        image_buffer_cache_hits_,
        frame_number_,
        ((float)image_buffer_cache_hits_ / frame_number_) * 100,
        image_buffer_largest_size_,
        image_buffer_.capacity());
    static_cast<void>(drm_process_.terminate_and_wait());
    drm_socket_.close();
}

auto DRMCudaCaptureSource::capture_(
    AVFrame* frame,
    frame_timer frame_budget,
    auto (*completion)(DRMCudaCaptureSource&, AVFrame*, void*)->void,
    void* data) -> void
{
    /* WARN:
     * This function _must_ complete synchronously. It must not do any "stack
     * ripping" because `data` is a pointer to a callback in the current stack
     * frame.
     */

    auto const r =
        WITH_PROFILE(metrics::ProfileSectionId::wayland_fetch_drm_data, [&] {
            return get_drm_data(
                drm_socket_, kDRMDataTimeoutMs, &drm_proc_mask_);
        });

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
        for (decltype(drm_data.num_fds) i = 0; i < drm_data.num_fds; ++i) {
            ::close(drm_data.descriptors[i].fd);
        }
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

    SyncFence const fence { descriptor->sync_fd, egl_display_ };
    if (has_mouse_plane) {
        /* We don't care about explicit sync for the mouse pointer plane...
         */
        ::close(mouse_plane_position->sync_fd);
    }

    buf::Item& image = get_image_buffer(*descriptor);

    std::optional<MouseParameters> mouse_params {};

    if (has_mouse_plane) {
        auto const& mouse_descriptor = *mouse_plane_position;
        ::close(mouse_descriptor.sync_fd);
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

        EGLImage mouse_image =
            egl().eglCreateImage(egl_display_,
                                 EGL_NO_CONTEXT,
                                 EGL_LINUX_DMA_BUF_EXT,
                                 static_cast<EGLClientBuffer>(nullptr),
                                 mouse_img_attr);

        if (mouse_image == EGL_NO_IMAGE) {
            throw std::runtime_error { "eglCreateImage input failed: " +
                                       std::to_string(egl().eglGetError()) };
        }

        SC_SCOPE_GUARD(
            [&] { egl().eglDestroyImage(egl_display_, mouse_image); });

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

    if (fence) {
        /* Explicit sync. Ignore the error condition for now; We don't
         * have a reliable path upstream to report this...
         */
        (void)try_wait_sync_fence(egl_display_, fence, frame_budget);
    }

    WITH_PROFILE(metrics::ProfileSectionId::opengl_color_conversion, [&] {
        color_converter_.convert(image.texture, mouse_params);
    });

    /* Ensure all the rendering commands have completed, otherwise we
     * risk copying an old frame.
     */
    gl().glFlush();

    WITH_PROFILE(metrics::ProfileSectionId::cuda_copy_frame, [&] {
        copy_texture_to_frame(cuda_ctx_, cuda_gfx_resource_, frame);
    });

    frame_number_ += 1;

    completion(*this, frame, data);
}

} // namespace sc
