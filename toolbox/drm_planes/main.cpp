#include "dmabuf_exporter.hpp"
#include "drm_device.hpp"
#include "drm_plane.hpp"
#include "drm_plane_check_loop.hpp"
#include "drm_plane_framebuffer.hpp"
#include "exios/context_thread.hpp"
#include "exios/signal.hpp"
#include "exios/unix_socket.hpp"
#include "frame_time_detector.hpp"
#include "frame_timer.hpp"
#include "framebuffer_descriptor.hpp"
#include "io/signals.hpp"
#include "ipc_ptr.hpp"
#include "logging.hpp"
#include "rolling_average.hpp"
#include "sticky_cancel_timer.hpp"
#include "utils/contracts.hpp"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <string_view>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

auto find_primary_plane(drm_device const& device) noexcept
    -> std::optional<drm_plane>
{
    for (drm_plane const& plane : device.planes()) {
        auto const is_primary = plane->fb_id && plane.is_primary_plane();
        if (is_primary)
            return plane;
    }

    return std::nullopt;
}

auto print_error(std::string_view msg) noexcept -> void
{
    std::fprintf(
        stderr, "[ERROR] %.*s\n", static_cast<int>(msg.size()), msg.data());
}

auto print_usage(std::string_view prog_name,
                 std::optional<std::string_view> msg = std::nullopt) noexcept
    -> void
{
    if (msg) {
        print_error(*msg);
    }
    std::fprintf(stderr,
                 "Usage: %.*s <SOCKET_NAME> <SH_MEM_PATH>\n\n",
                 static_cast<int>(prog_name.size()),
                 prog_name.data());
}

struct cmdline_parameters
{
    std::string socket_path;
    std::string shared_memory_path;
};

auto get_cmdline_parameters(std::span<char const*> args) -> cmdline_parameters
{
    cmdline_parameters params {};

    if (args.size() > 0)
        params.socket_path = args[0];

    if (args.size() > 1)
        params.shared_memory_path = args[1];

    return params;
}

auto make_shared_memory(std::string path)
    -> sc::ipc_ptr<sc::framebuffer_descriptor_sequence>
{
    try {
        return sc::make_ipc_ptr<sc::framebuffer_descriptor_sequence>(path);
    }
    catch (std::exception const& e) {
        std::cerr << "[ERROR] " << e.what() << '\n';
        throw e;
    }
}

#if 1
using std::chrono::duration_cast;
using std::chrono::nanoseconds;
using std::chrono::seconds;

auto constexpr kNsPerMs = std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::milliseconds(1));

auto main(int argc, char const** argv) -> int
{
    std::span<char const*> args(argv + 1, argc - 1);
    SC_EXPECT(argc > 0);

    if (args.size() != 2) {
        print_usage(*argv, "Incorrect number of arguments");
        return 1;
    }

    auto const [socket_path, shmem_path] = get_cmdline_parameters(args);
    sc::log(sc::LogLevel::debug,
            "(DRM) socket path: %.*s, shmem path: %.*s",
            static_cast<int>(socket_path.size()),
            socket_path.data(),
            static_cast<int>(shmem_path.size()),
            shmem_path.data());

    sc::block_signals({ SIGINT });

    auto drm_device = open_drm_device();

    if (!drm_device) {
        sc::log(sc::LogLevel::error,
                "(DRM) Couldn't initialize a suitable DRM device");
        return -1;
    }

    auto const primary_plane = find_primary_plane(*drm_device);

    if (!primary_plane) {
        sc::log(sc::LogLevel::error, "(DRM) Couldn't find any screen planes");
        return -1;
    }

    auto seqlock = make_shared_memory(shmem_path);

    exios::ContextThread context;
    sc::StickyCancelTimer timer { context };
    sc::StickyCancelTimer delay_timer { context };
    exios::Signal signal { context, SIGINT };
    exios::Event new_frame_event { context };

    exios::UnixSocket socket { context };

    signal.wait([&](auto) {
        sc::log(sc::LogLevel::debug, "(DRM) stop signal received");
        timer.cancel();
        delay_timer.cancel();
        socket.cancel();
        new_frame_event.cancel();
    });

    auto const& plane = *primary_plane;

    /* This checks for DRM frame buffer swaps and emit an event when detected.
     * It also writes the newly detected frame buffer's metadata to shared
     * memory...
     */
    drm_plane_check_loop(*drm_device,
                         plane->plane_id,
                         timer,
                         new_frame_event,
                         *seqlock,
                         sc::frame_timer(kNsPerMs / 2))
        .start();

    frame_time_estimation estimation { .avg_frame_time { 10 },
                                       .last_frame_ts {} };

    /* This accepts client requests and responds with DRM frame buffer and sync
     * file descriptors...
     */
    dmabuf_exporter(socket,
                    socket_path,
                    delay_timer,
                    *drm_device,
                    *seqlock,
                    estimation,
                    /* TODO:
                     * We need to get the frame time from the client...
                     */
                    duration_cast<nanoseconds>(seconds(1)) / 60)
        .start();

    /* This measures and updates the frame times of the DRM frame buffer swaps.
     * Also records the last buffer presentation* timestamp...
     *
     * (* This isn't a real presentation timestamp, just when we detected the FB
     * ID change)
     */
    frame_time_detector(new_frame_event, estimation).start();

    try {
        static_cast<void>(context.run());
    }
    catch (std::exception const& e) {
        sc::log(sc::LogLevel::error, "(DRM) I/O Loop failed: %s", e.what());
        return -1;
    }

    sc::log(sc::LogLevel::info, "(DRM) Exiting...");

    return 0;
}
#else

auto find_alternative_plane(drm_device const& device,
                            drm_plane const& primary_plane)
    -> std::optional<drm_plane>
{
    auto primary_fb = primary_plane.framebuffer();

    for (auto& plane : device.planes()) {
        if (!plane->fb_id)
            continue;

        auto fb = plane.framebuffer();

        if (fb->fb_id == primary_fb->fb_id)
            continue;

        sc::log(sc::LogLevel::debug,
                "Checking plane: %u %ux%u",
                fb->fb_id,
                fb->width,
                fb->height);

        if (fb->width == primary_fb->width && fb->height == primary_fb->height)
            return plane;
    }

    return std::nullopt;
}

auto main() -> int
{
    auto drm_device = open_drm_device();

    if (!drm_device) {
        sc::log(sc::LogLevel::error,
                "(DRM) Couldn't initialize a suitable DRM device");
        return -1;
    }

    auto const primary_plane = find_primary_plane(*drm_device);

    if (!primary_plane) {
        sc::log(sc::LogLevel::error, "(DRM) Couldn't find any screen planes");
        return -1;
    }

    auto alternative_plane =
        find_alternative_plane(*drm_device, *primary_plane);

    if (alternative_plane) {
        auto fb = alternative_plane->framebuffer();

        auto [success, fd] = fb.get_dmabuf_handle();

        if (success) {
            sc::log(sc::LogLevel::debug,
                    "Found alternative plane from %u: %u",
                    (*primary_plane)->fb_id,
                    fb->fb_id);
        }
    }
}

#endif
