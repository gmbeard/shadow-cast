#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_FRAME_TIME_DETECTOR_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_FRAME_TIME_DETECTOR_HPP_INCLUDED

#include "exios/event.hpp"
#include "frame_timer.hpp"
#include "rolling_average.hpp"
#include <type_traits>

using time_point_type =
    std::remove_reference_t<decltype(sc::frame_timer::now())>;
using duration_type = decltype(sc::frame_timer::now() - sc::frame_timer::now());

struct frame_time_estimation
{
    rolling_average<std::int64_t> avg_frame_time;
    time_point_type last_frame_ts;
};

struct frame_time_detector
{
    frame_time_detector(exios::Event& new_frame_event_source,
                        frame_time_estimation& estimation) noexcept;

    auto start() && -> void;

private:
    auto wait_for_new_frame() && -> void;
    auto measure(time_point_type ts) && -> void;

private:
    exios::Event& new_frame_event_source_;
    frame_time_estimation& estimation_;
    std::size_t frame_count_ { 0 };
};

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_FRAME_TIME_DETECTOR_HPP_INCLUDED
