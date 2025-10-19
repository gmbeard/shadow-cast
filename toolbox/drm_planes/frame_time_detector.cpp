#include "frame_time_detector.hpp"
#include "frame_timer.hpp"
#include "logging.hpp"
#include <utility>

frame_time_detector::frame_time_detector(
    exios::Event& new_frame_event_source,
    frame_time_estimation& estimation) noexcept
    : new_frame_event_source_ { new_frame_event_source }
    , estimation_ { estimation }
{
}

auto frame_time_detector::start() && -> void
{
    std::move(*this).wait_for_new_frame();
}

auto frame_time_detector::wait_for_new_frame() && -> void
{
    auto& e = new_frame_event_source_;
    auto cb =
        [self = std::move(*this)](exios::TimerOrEventIoResult result) mutable {
            time_point_type ts = sc::frame_timer::now();
            if (!result) {
                if (result.error() == std::errc::operation_canceled)
                    return;

                sc::log(sc::LogLevel::error,
                        "(frame_time_detector) Event wait failed");
                throw std::system_error(result.error());
            }

            self.frame_count_ += 1;
            std::move(self).measure(std::move(ts));
        };

    e.wait_for_event(std::move(cb));
}

auto frame_time_detector::measure(time_point_type ts) && -> void
{
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;

    auto& avg_frame_time = estimation_.avg_frame_time;
    auto& last_frame_time = estimation_.last_frame_ts;

    auto const prev_ts = std::exchange(last_frame_time, ts);
    avg_frame_time.add(duration_cast<nanoseconds>(ts - prev_ts).count());

    std::move(*this).wait_for_new_frame();
}
