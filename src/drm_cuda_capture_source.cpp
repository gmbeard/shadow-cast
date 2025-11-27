#include "drm_cuda_capture_source.hpp"
#include "av/codec.hpp"
#include "color_converter.hpp"
#include "config.hpp"
#include "cuda.hpp"
#include "drm/messaging.hpp"
#include "drm/planes.hpp"
#include "frame_timer.hpp"
#include "framebuffer_descriptor.hpp"
#include "gl/object.hpp"
#include "gl/texture.hpp"
#include "io/accept_handler.hpp"
#include "io/message_sender.hpp"
#include "io/unix_socket.hpp"
#include "ipc_ptr.hpp"
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
#include <GL/glext.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <optional>
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

GraphicsResource::operator bool() const noexcept
{
    return value_ != nullptr;
}

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
// char constexpr kSocketPath[] = "/tmp/shadow-cast.sock";
char constexpr kSocketPath[] = "shadow-cast.sock";
std::size_t constexpr kDRMConnectTimeoutMs = 1'000;
std::size_t constexpr kDRMDataTimeoutMs = 1'000;
char constexpr kDRMBin[] = "shadow-cast-kms";
char constexpr kSharedMemoryName[] = "/shadow-cast-shmem-0";
float constexpr kPhaseDriftErrorThreshold = 0.7f;

auto get_drm_data(
    sc::UnixSocket& socket,
    sc::ipc_ptr<sc::framebuffer_descriptor_sequence>& shared_memory,
    std::size_t timeout,
    sigset_t* mask) -> sc::Result<sc::DRMResponse, std::error_code>
{
    static std::size_t constexpr kMaxAttempts = 3;

    sc::framebuffer_descriptor descriptor;
    sc::dmabuf_reply_message response {};

    for (std::size_t n = 0; n < kMaxAttempts; ++n) {

        sc::DRMRequest request { sc::drm_request::kGetPlanes };
        response = {};

        auto const send_result = socket.use_with(
            sc::MessageSender<sc::DRMRequest> { request, timeout, mask });

        if (!send_result) {
            return send_result.error();
        }

        if (sc::get_value(send_result) < sizeof(request)) {
            return sc::result_error(
                std::error_code { EAGAIN, std::system_category() });
        }

        auto const recv_result = socket.use_with(
            sc::dmabuf_reply_message_receiver { response, timeout, mask });

        if (!recv_result) {
            return recv_result.error();
        }

        auto close_fds_guard = sc::ScopeGuard { [&] {
            ::close(response.fb_fd);
            ::close(response.sync_fd);
        } };

        if (sc::get_value(recv_result) < sizeof(response)) {
            return sc::result_error(
                std::error_code { EAGAIN, std::system_category() });
        }

        descriptor = sc::read_next_sequence(*shared_memory);

        if (descriptor.fb_id == response.fb_id) {
            close_fds_guard.deactivate();
            break;
        }

        if ((n + 1) == kMaxAttempts) {
            sc::log(sc::LogLevel::warn,
                    "DRM buffer has changed since last response. Received %u, "
                    "current %u",
                    response.fb_id,
                    descriptor.fb_id);
        }
    }

    sc::DRMResponse combined_response {};
    combined_response.num_fds = 1;
    combined_response.descriptors[0].fb_id = response.fb_id;
    combined_response.descriptors[0].fd = response.fb_fd;
    combined_response.descriptors[0].flags = descriptor.flags;
    combined_response.descriptors[0].height = descriptor.height;
    combined_response.descriptors[0].width = descriptor.width;
    combined_response.descriptors[0].modifier = descriptor.modifier;
    combined_response.descriptors[0].pixel_format = descriptor.pixel_format;
    combined_response.descriptors[0].pitch = descriptor.pitch;
    combined_response.descriptors[0].offset = descriptor.offset;
    combined_response.descriptors[0].sync_fd = response.sync_fd;
    combined_response.descriptors[0].phase_offset_nanoseconds =
        response.phase_offset_nanoseconds;

    return sc::result_ok(combined_response);
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
    auto kms_bin_path = kms_bin_dir / "../toolbox" / "drm_planes";
    // auto kms_bin_path = kms_bin_dir / kDRMBin;
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

    // kms_bin_path = fs::path(sc::KLibExecDir) / kDRMBin;
    // sc::log(sc::LogLevel::debug,
    //         "Checking for %s at %s",
    //         kDRMBin,
    //         kms_bin_path.c_str());
    // if (fs::exists(kms_bin_path)) {
    //     sc::log(sc::LogLevel::debug,
    //             "Found %s at %s",
    //             kDRMBin,
    //             kms_bin_path.c_str());
    //     return kms_bin_path;
    // }

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

    operator bool() const noexcept
    {
        return fence_ != EGL_NO_SYNC;
    }

    operator EGLSync() const noexcept
    {
        return fence_;
    }

private:
    EGLDisplay display_;
    EGLSync fence_ { EGL_NO_SYNC };
};

[[nodiscard]] auto try_wait_sync_fence(EGLDisplay display,
                                       EGLSync fence) noexcept -> bool
{
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;

    if (!sc::egl().eglWaitSync(display, fence, 0)) {
        sc::log(sc::LogLevel::warn, "EGL server sync failed!");
        return false;
    }

    return true;
}

} // namespace

namespace sc
{

DRMCudaCaptureSource::DRMCudaCaptureSource(
    exios::Context context,
    Parameters const& params,
    VideoOutputSize output_size,
    VideoOutputScale output_scale,
    CUcontext cuda_ctx,
    EGLDisplay egl_display,
    std::optional<float> const& phase_drift_threshold) noexcept
    : ctx_ { context }
    , timer_ { context }
    , frame_interval_ { params.frame_time.value() }
    , cuda_ctx_ { cuda_ctx }
    , egl_display_ { std::move(egl_display) }
    , phase_drift_threshold_ { phase_drift_threshold.has_value()
                                   ? *phase_drift_threshold
                                   : kPhaseDriftErrorThreshold }
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

auto DRMCudaCaptureSource::cancel() noexcept -> void
{
    timer_.cancel();
}

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
    log(LogLevel::info,
        "Framerate phase drift threshold: %3.2f",
        phase_drift_threshold_);

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
        // unlink(kSocketPath);
    });

    server_socket.listen();

    /* Spawn the DRM child process and wait for it
     * to attach itself...
     */
    std::vector<std::string> args { find_drm_helper_binary(),
                                    kSocketPath,
                                    kSharedMemoryName };
    drm_process_ = sc::spawn_process(std::span { args.data(), args.size() });
    auto socket_result = server_socket.use_with(
        sc::AcceptHandler { kDRMConnectTimeoutMs, &drm_proc_mask_ });

    if (!socket_result)
        throw std::system_error { sc::get_error(socket_result) };

    drm_socket_ = UnixSocket { sc::get_value(socket_result) };

    shared_memory_.emplace(
        open_ipc_ptr<framebuffer_descriptor_sequence>(kSharedMemoryName));
}

auto DRMCudaCaptureSource::flush_stale_image_buffers() -> void
{
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;

    std::size_t const stale_after_frame = seconds(1) / frame_interval_;

    /* We only flush the back item of the cache to keep this
     * constant time
     */

    if (image_buffer_.size() == 0)
        return;

    auto pos = image_buffer_.rbegin();
    buf::Item& image = std::get<1>(*pos);
    SC_EXPECT(image.used_on_frame_number <= frame_number_);
    auto const unused_for = frame_number_ - image.used_on_frame_number;
    if (unused_for >= stale_after_frame) {
        image_buffer_.erase(--(pos.base()));
    }
}

auto DRMCudaCaptureSource::create_image(PlaneDescriptor const& descriptor)
    -> buf::Item
{
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

    return buf::Item { egl_display_, image, frame_number_ };
}

auto DRMCudaCaptureSource::get_image_buffer(PlaneDescriptor const& descriptor)
    -> buf::Item&
{
    image_buffer_largest_size_ =
        std::max(image_buffer_largest_size_, image_buffer_.size());
    flush_stale_image_buffers();

    auto pos = image_buffer_.find(descriptor.fb_id);
    if (pos == image_buffer_.end()) {
        auto [item, inserted] =
            image_buffer_.insert(descriptor.fb_id, create_image(descriptor));
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
    frame_timer /*frame_budget*/,
    auto (*completion)(DRMCudaCaptureSource&,
                       std::variant<AVFrame*, frame_capture_out_of_phase_error>,
                       void*)
        ->void,
    void* data) -> void
{
    /* WARN:
     * This function _must_ complete synchronously. It must not do any "stack
     * ripping" because `data` is a pointer to a callback in the current stack
     * frame.
     */

    auto const r =
        WITH_PROFILE(metrics::ProfileSectionId::wayland_fetch_drm_data, [&] {
            return get_drm_data(drm_socket_,
                                *shared_memory_,
                                kDRMDataTimeoutMs,
                                &drm_proc_mask_);
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

    auto const offset = descriptor->phase_offset_nanoseconds;
    auto const offset_factor =
        static_cast<float>(offset) /
        std::chrono::duration_cast<std::chrono::nanoseconds>(frame_interval_)
            .count();
    bool const out_of_phase = offset_factor >= phase_drift_threshold_;

    /* There isn't much point in reporting more than one consecutive
     * out-of-phase error. If we're still out-of-phase on the second attempt
     * then we may as well just capture; We're going to be capturing one
     * frame behind anyway, so there's no point it asking upstream to
     * correct us any further...
     */
    if (std::exchange(consecutive_out_of_phase_count_, 0) == 0 &&
        out_of_phase) {

        consecutive_out_of_phase_count_ += 1;
        completion(*this,
                   frame_capture_out_of_phase_error { .phase_offset = offset },
                   data);
        return;
    }

    std::optional<buf::Item> non_cached_image_storage {};
    buf::Item& image = [&]() -> buf::Item& {
        if (image_buffer_.capacity())
            return get_image_buffer(*descriptor);

        return non_cached_image_storage.emplace(create_image(*descriptor));
    }();

    opengl::bind(opengl::TextureTarget<GL_TEXTURE_EXTERNAL_OES> {},
                 color_converter_.input_texture(),
                 [&](auto) {
                     gl().glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES,
                                                       image.image);
                 });

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
        if (!try_wait_sync_fence(egl_display_, fence)) {
            log(LogLevel::warn, "Wait for explicit sync fence failed");
        }
    }

    WITH_PROFILE(metrics::ProfileSectionId::opengl_color_conversion,
                 [&] { color_converter_.convert(mouse_params); });

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
