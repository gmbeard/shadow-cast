#ifndef SHADOW_CAST_VIDEO_CAPTURE_HPP_INCLUDED
#define SHADOW_CAST_VIDEO_CAPTURE_HPP_INCLUDED

#include "av/codec.hpp"
#include "capture_sink.hpp"
#include "capture_source.hpp"
#include "media_container.hpp"
#include "sticky_cancel_timer.hpp"
#include "utils/cmd_line.hpp"
#include "utils/frame_time.hpp"
#include "video_capture_loop_operation.hpp"
#include <libavcodec/codec.h>
#include <utility>

namespace sc
{

template <CaptureSink Sink, CaptureSource<typename Sink::input_type> Source>
struct VideoCapture
{
    explicit VideoCapture(exios::Context execution_context,
                          Sink&& sink,
                          Source&& source,
                          Parameters const& params)
        : timer_ { execution_context }
        , sink_ { std::move(sink) }
        , source_ { std::move(source) }
        , frame_time_ { params.frame_time }
    {
    }

    VideoCapture(VideoCapture const&) = delete;
    auto operator=(VideoCapture const&) -> VideoCapture& = delete;

    template <typename Completion>
    auto run(Completion&& completion) -> void
    {
        VideoCaptureLoopOperation(
            sink_, source_, timer_, frame_time_, std::move(completion))
            .initiate();
    }

    auto cancel() noexcept -> void { timer_.cancel(); }

private:
    StickyCancelTimer timer_;
    Sink sink_;
    Source source_;
    FrameTime frame_time_;
};

} // namespace sc
#endif // SHADOW_CAST_VIDEO_CAPTURE_HPP_INCLUDED
