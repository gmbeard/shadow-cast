#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_CHECK_LOOP_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_CHECK_LOOP_HPP_INCLUDED

#include "drm_device.hpp"
#include "frame_timer.hpp"
#include "framebuffer_descriptor.hpp"
#include "sticky_cancel_timer.hpp"
#include <cstdint>

struct drm_plane_framebuffer;

struct drm_plane_check_loop
{
    explicit drm_plane_check_loop(drm_device& device,
                                  std::uint32_t plane_id,
                                  sc::StickyCancelTimer& timer,
                                  exios::Event& new_frame_event_sink,
                                  sc::framebuffer_descriptor_sequence& seqlock,
                                  sc::frame_timer interval) noexcept;

    auto start() && -> void;

private:
    auto wait_for_interval() && -> void;
    auto write_descriptor(
        drm_plane_framebuffer const&,
        std::chrono::time_point<sc::frame_timer::clock_type> ts) const noexcept
        -> void;

    auto check_fb_id(drm_plane_framebuffer const&) noexcept -> bool;

    auto tick(exios::TimerOrEventIoResult result) && -> void;

    drm_device& device_;
    std::uint32_t plane_id_;
    sc::StickyCancelTimer& timer_;
    exios::Event& new_frame_event_sink_;
    sc::framebuffer_descriptor_sequence& fb_mb_item_;
    sc::frame_timer interval_;
    std::uint32_t fb_id_ { 0 };
    std::chrono::time_point<sc::frame_timer::clock_type> start_time_ {
        sc::frame_timer::now()
    };
    std::chrono::time_point<sc::frame_timer::clock_type> prev_ts_ {
        sc::frame_timer::now()
    };
};

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_CHECK_LOOP_HPP_INCLUDED
