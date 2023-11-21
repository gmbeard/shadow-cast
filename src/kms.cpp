#include "drm.hpp"
#include "io.hpp"
#include "utils.hpp"

#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <libdrm/drm_mode.h>
#include <linux/limits.h>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <system_error>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

using IncomingMessage = sc::DRMRequest;
using OutgoingMessage = sc::DRMResponse;

auto constexpr kEnableLogging = false;

struct NullLogger
{
};

template <typename T>
auto operator<<(NullLogger& logger, T const&) noexcept -> NullLogger&
{
    return logger;
}

auto null_logger() -> NullLogger&
{
    static NullLogger logger {};
    return logger;
}

decltype(auto) log()
{
    if constexpr (kEnableLogging) {
        return (std::cerr);
    }
    else {
        return null_logger();
    }
}

[[nodiscard]] auto is_drm_available() -> bool { return drmAvailable() == 1; }

[[nodiscard]] auto get_drm_device_path() -> std::string
{
    int const max_devices = 10;
    std::string device_path;
    for (int i = 0; i < max_devices; ++i) {
        char path[PATH_MAX];
        if (auto const result =
                std::snprintf(path, PATH_MAX, DRM_DEV_NAME, DRM_DIR_NAME, i);
            result < 0 || result >= PATH_MAX)
            throw std::runtime_error { "Failed to construct DRM device path" };

        log() << "[DRM] Checking DRM path: " << path << '\n';

        auto drm_fd = open(path, O_RDONLY);
        if (drm_fd < 0)
            continue;

        SC_SCOPE_GUARD([&] { close(drm_fd); });

        auto const ver = drmGetVersion(drm_fd);
        if (!ver)
            continue;

        SC_SCOPE_GUARD([&] { drmFreeVersion(ver); });

        std::string_view name { ver->name };
        std::string_view desc { ver->desc };

        log() << "[DRM] Found: " << name << ", " << desc << '\n';

        auto const resources = drmModeGetResources(drm_fd);
        if (!resources)
            continue;

        SC_SCOPE_GUARD([&] { drmModeFreeResources(resources); });

        log() << "[DRM] count_fbs: " << resources->count_fbs << "\n";

        drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

        auto const planes = drmModeGetPlaneResources(drm_fd);
        if (!planes)
            continue;

        SC_SCOPE_GUARD([&] { drmModeFreePlaneResources(planes); });

        for (uint32_t j = 0; j < planes->count_planes; ++j) {
            auto const plane = drmModeGetPlane(drm_fd, planes->planes[j]);
            if (!plane)
                continue;

            SC_SCOPE_GUARD([&] { drmModeFreePlane(plane); });

            log() << "[DRM] Checking plane:\n"
                  << "  plane_id: " << plane->plane_id << '\n'
                  << "  count_formats: " << plane->count_formats << '\n'
                  << "  fb_id: " << plane->fb_id << '\n'
                  << "  crtc_id: " << plane->crtc_id << '\n'
                  << "  crtc_x: " << plane->crtc_x
                  << ", crtc_y: " << plane->crtc_y << '\n'
                  << "  x: " << plane->x << ", y: " << plane->y << '\n';

            if (plane->fb_id) {
                log() << "[DRM] Found FB\n";
                return path;
            }
            else
                log() << "[DRM] No FB found in plane " << planes->planes[j]
                      << '\n';
        }
    }

    if (!device_path.size())
        throw std::runtime_error { "Failed to find a valid DRM device path" };

    return device_path;
}

auto get_connectors(int drm_fd)
{
    auto const resources = drmModeGetResources(drm_fd);
    if (!resources)
        throw std::runtime_error { "drmModeGetResources failed" };

    SC_SCOPE_GUARD([&] { drmModeFreeResources(resources); });

    for (auto i = 0; i < resources->count_connectors; ++i) {
        auto const connector =
            drmModeGetConnectorCurrent(drm_fd, resources->connectors[i]);
        if (!connector)
            continue;

        SC_SCOPE_GUARD([&] { drmModeFreeConnector(connector); });

        log() << "[DRM] Properties:\n";
        for (auto j = 0; j < connector->count_props; ++j) {
            auto const prop = drmModeGetProperty(drm_fd, connector->props[j]);
            if (!prop)
                continue;

            SC_SCOPE_GUARD([&] { drmModeFreeProperty(prop); });
            log() << "  " << prop->name << '\n';
        }
    }
}

auto get_plane_properties(int drm_fd,
                          std::uint32_t plane_id,
                          sc::PlaneDescriptor& descriptor)
{
    drmModeObjectPropertiesPtr props =
        drmModeObjectGetProperties(drm_fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!props)
        throw std::runtime_error { "drmModeObjectGetProperties failed" };

    SC_SCOPE_GUARD([&] { drmModeFreeObjectProperties(props); });

    log() << "[DRM] Plane properties:\n";

    int x = 0;
    int y = 0;

    for (uint32_t i = 0; i < props->count_props; ++i) {
        drmModePropertyPtr prop = drmModeGetProperty(drm_fd, props->props[i]);
        if (!prop)
            continue;

        SC_SCOPE_GUARD([&] { drmModeFreeProperty(prop); });

        std::string_view prop_name { prop->name };
        log() << "[DRM] Property: " << prop_name
              << ", Values: " << prop->count_values << '\n';

        const uint32_t type = prop->flags & (DRM_MODE_PROP_LEGACY_TYPE |
                                             DRM_MODE_PROP_EXTENDED_TYPE);

        if ((type & DRM_MODE_PROP_SIGNED_RANGE) && prop_name == "CRTC_X") {
            x = props->prop_values[i];
        }
        else if ((type & DRM_MODE_PROP_SIGNED_RANGE) && prop_name == "CRTC_Y") {
            y = props->prop_values[i];
        }
        else if ((type & DRM_MODE_PROP_RANGE) && prop_name == "SRC_X") {
            descriptor.x = (props->prop_values[i] >> 16);
        }
        else if ((type & DRM_MODE_PROP_RANGE) && prop_name == "SRC_Y") {
            descriptor.y = (props->prop_values[i] >> 16);
        }
        else if ((type & DRM_MODE_PROP_RANGE) && prop_name == "SRC_W") {
            descriptor.src_w = (props->prop_values[i] >> 16);
        }
        else if ((type & DRM_MODE_PROP_RANGE) && prop_name == "SRC_H") {
            descriptor.src_h = (props->prop_values[i] >> 16);
        }
        else if ((type & DRM_MODE_PROP_ENUM) && prop_name == "type") {
            const uint64_t current_enum_value = props->prop_values[i];
            for (int j = 0; j < prop->count_enums; ++j) {
                if (prop->enums[j].value == current_enum_value &&
                    strcmp(prop->enums[j].name, "Cursor") == 0) {
                    descriptor.set_flag(sc::plane_flags::IS_CURSOR);
                    break;
                }
            }
        }
    }

    if (descriptor.is_flag_set(sc::plane_flags::IS_CURSOR)) {
        descriptor.x = x;
        descriptor.y = y;
    }
}

auto get_fb(int drm_fd) -> OutgoingMessage
{
    auto const resources = drmModeGetPlaneResources(drm_fd);
    if (!resources)
        throw std::runtime_error { "drmModeGetPlaneResources failed" };

    SC_SCOPE_GUARD([&] { drmModeFreePlaneResources(resources); });

    OutgoingMessage msg {};

    for (auto i = 0u;
         i < resources->count_planes && i < sc::kMaxPlaneDescriptors;
         ++i) {
        auto const plane = drmModeGetPlane(drm_fd, resources->planes[i]);
        SC_SCOPE_GUARD([&] {
            if (plane)
                drmModeFreePlane(plane);
        });

        if (!plane || !plane->fb_id)
            continue;

        log() << "[DRM] Got Plane\n";

        /* Requires SYS_CAP_ADMIN from here, E.g...
         *
         *   $ setcap cap_sys_admin+ep <EXECUTABLE>
         *
         */
        drmModeFB2Ptr const fb = drmModeGetFB2(drm_fd, plane->fb_id);
        SC_SCOPE_GUARD([&] {
            if (fb)
                drmModeFreeFB2(fb);
        });

        if (!fb || !fb->handles[0])
            throw std::runtime_error { "drmModeGetFB2 failed" };

        int fb_fd = -1;
        if (auto const result =
                drmPrimeHandleToFD(drm_fd, fb->handles[0], O_RDONLY, &fb_fd);
            result != 0 || fb_fd == -1)
            throw std::runtime_error { "drmPrimeHandleToFD failed" };

        SC_SCOPE_GUARD([&] {
            for (auto h : fb->handles) {
                if (!h)
                    continue;

                drmCloseBufferHandle(drm_fd, h);
            }
        });

        msg.descriptors[i].fd = fb_fd;
        msg.descriptors[i].width = fb->width;
        msg.descriptors[i].height = fb->height;
        msg.descriptors[i].pitch = fb->pitches[0];
        msg.descriptors[i].offset = fb->offsets[0];
        msg.descriptors[i].pixel_format = fb->pixel_format;
        msg.descriptors[i].modifier = fb->modifier;

        log() << "[DRM] Got FB FD: " << fb_fd << '\n';
        log() << "[DRM] FB: " << fb->handles[0] << ' ' << fb->handles[1] << ' '
              << fb->handles[2] << ' ' << fb->handles[3] << '\n';

        msg.num_fds += 1;
    }

    return msg;
}

auto get_plane_descriptors(int drm_fd) -> OutgoingMessage
{
    return get_fb(drm_fd);
}

auto app(std::span<char const*> args) -> void
{
    sc::block_signals({ SIGINT });

    if (!is_drm_available())
        throw std::runtime_error { "DRM not available" };

    auto const device_path = get_drm_device_path();

    auto const drm_fd = open(device_path.c_str(), O_RDONLY);
    if (drm_fd < 0)
        throw std::runtime_error { "Failed to open DRM device" };

    SC_SCOPE_GUARD([&] {
        log() << "[DRM] Closing DRM FD...\n";
        close(drm_fd);
    });
    if (auto const result =
            drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
        result != 0)
        throw std::runtime_error { "drmSetClientCap failed" };

    if (auto const result = drmSetClientCap(drm_fd, DRM_CLIENT_CAP_ATOMIC, 1);
        result != 0)
        throw std::runtime_error { "drmSetClientCap failed" };

    /* We still need a sigaction so that the
     * default SIGINT action isn't taken in
     * when the `ppoll()` calls unblock it.
     * We simply do a no-op...
     */
    auto ignore = [](int) {};
    struct sigaction sa = {};
    sa.sa_handler = ignore;
    sigaction(SIGINT, &sa, nullptr);

    /* This set of signals is what `ppoll()`
     * will set the signal mask to when checking
     * for events...
     */
    sigset_t emptysigset;
    sigemptyset(&emptysigset);

    log() << "[DRM] Connecting to: " << args[0] << '\n';

    auto socket = sc::socket::connect(args[0]);

    while (true) {
        IncomingMessage req;
        auto const receive_result =
            socket.use_with(sc::MessageReceiver<IncomingMessage> {
                req, sc::timeout::kInfinite, &emptysigset });

        if (!receive_result) {
            if (sc::get_error(receive_result).value() == EINTR)
                break;

            throw std::system_error { sc::get_error(receive_result) };
        }

        if (sc::get_value(receive_result) < sizeof(IncomingMessage))
            break;

        if (req.type == sc::drm_request::kStop)
            break;

        if (req.type != sc::drm_request::kGetPlanes) {
            log() << "[DRM] Warning - ignoring unknown request type: "
                  << req.type << '\n';
            continue;
        }

        auto response = get_plane_descriptors(drm_fd);

        SC_SCOPE_GUARD([&] {
            log() << "[DRM] Closing " << response.num_fds << " fds\n";
            for (auto i = 0u; i < response.num_fds; ++i)
                close(response.descriptors[i].fd);
        });

        auto const send_result = socket.use_with(sc::DRMResponseSender {
            response, sc::timeout::kInfinite, &emptysigset });

        if (!send_result) {
            log() << "[DRM] Failed to send: "
                  << std::strerror(sc::get_error(send_result).value()) << '\n';
            if (sc::get_error(send_result).value() == EINTR)
                break;

            throw std::system_error { sc::get_error(send_result) };
        }

        if (sc::get_value(send_result) < sizeof(response)) {
            break;
        }
    }

    log() << "[DRM] Stopping\n";
}

auto main(int argc, char const** argv) -> int
{
    try {
        auto const args =
            std::span { argv + 1, static_cast<std::size_t>(argc - 1) };
        if (!args.size())
            throw std::runtime_error { "Missing argument: socket path" };

        app(args);
    }
    catch (std::exception const& e) {
        log() << "ERROR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
