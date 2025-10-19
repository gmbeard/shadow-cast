#include "drm_plane_check_loop.hpp"
#include "drm_plane.hpp"
#include "framebuffer_descriptor.hpp"
#include "logging.hpp"
#include <chrono>

drm_plane_check_loop::drm_plane_check_loop(
    drm_device& device,
    std::uint32_t plane_id,
    sc::StickyCancelTimer& timer,
    exios::Event& new_frame_event_sink,
    sc::framebuffer_descriptor_sequence& fb_mb_item,
    sc::frame_timer interval) noexcept
    : device_ { device }
    , plane_id_ { plane_id }
    , timer_ { timer }
    , new_frame_event_sink_ { new_frame_event_sink }
    , fb_mb_item_ { fb_mb_item }
    , interval_ { interval }
{
    auto const plane = drm_plane(static_cast<int>(device), plane_id);
    SC_EXPECT(plane);
    fb_id_ = plane->fb_id;
}

auto drm_plane_check_loop::start() && -> void
{
    start_time_ = prev_ts_ = sc::frame_timer::now();
    auto const plane = drm_plane(static_cast<int>(device_), plane_id_);
    write_descriptor(plane.framebuffer(), start_time_);

    std::move(*this).wait_for_interval();
}

auto drm_plane_check_loop::wait_for_interval() && -> void
{
    auto next_frame = interval_.duration_until_next_frame_from(interval_.now());
    auto& t = timer_;
    auto cb =
        [self = std::move(*this)](exios::TimerOrEventIoResult result) mutable {
            std::move(self).tick(std::move(result));
        };

    t.wait_for_expiry_after(next_frame, std::move(cb));
}

auto drm_plane_check_loop::write_descriptor(
    drm_plane_framebuffer const& framebuffer,
    std::chrono::time_point<sc::frame_timer::clock_type> ts) const noexcept
    -> void
{
    // clang-format off
        sc::framebuffer_descriptor source {
            .fb_id = framebuffer->fb_id,
            .width = framebuffer->width,
            .height = framebuffer->height,
            .pixel_format = framebuffer->pixel_format,
            .modifier = framebuffer->modifier,
            .flags = framebuffer->flags,
            .pitch = framebuffer->pitches[0],
            .offset = framebuffer->offsets[0],
            .ts = ts
        };
    // clang-format on

    sc::write_next_sequence(source, fb_mb_item_);
}

auto drm_plane_check_loop::check_fb_id(drm_plane_framebuffer const& fb) noexcept
    -> bool
{
    return fb->fb_id && (std::exchange(fb_id_, fb->fb_id) != fb->fb_id);
}

auto drm_plane_check_loop::tick(exios::TimerOrEventIoResult result) && -> void
{
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;

    if (!result) {
        if (result.error() != std::errc::operation_canceled)
            throw std::system_error(result.error());
        else
            return;
    }

    /* Check DRM plane for change...
     */

    auto const plane = drm_plane(static_cast<int>(device_), plane_id_);
    auto const framebuffer = plane.framebuffer();
    if (framebuffer && check_fb_id(framebuffer)) {
        auto const now = interval_.now();
        // auto const prev = std::exchange(prev_ts_, now);
        write_descriptor(framebuffer, now);

        new_frame_event_sink_.trigger([](auto) {});
        // auto const delta_nanoseconds =
        //     duration_cast<nanoseconds>(now - prev).count();

        // auto const fps =
        //     static_cast<float>(duration_cast<nanoseconds>(seconds(1)).count())
        //     / delta_nanoseconds;

        // sc::log(sc::LogLevel::debug,
        //         "(DRM) Estimated FPS: %.3f. (delta %lluns)",
        //         fps,
        //         delta_nanoseconds);
    }

    std::move(*this).wait_for_interval();
}
