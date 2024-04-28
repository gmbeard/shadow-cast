#ifndef SHADOW_CAST_VIDEO_CAPTURE_HPP_INCLUDED
#define SHADOW_CAST_VIDEO_CAPTURE_HPP_INCLUDED

#include "av/codec.hpp"
#include "media_container.hpp"
#include "sticky_cancel_timer.hpp"
#include "utils/frame_time.hpp"
#include "video_capture_loop_operation.hpp"
#include <libavcodec/codec.h>
#include <utility>

namespace sc
{

template <typename Desktop, typename Gpu>
struct VideoCapture
{
    explicit VideoCapture(exios::Context execution_context,
                          Desktop&& desktop,
                          Gpu&& gpu,
                          std::string const& codec_name,
                          FrameTime frame_time)
        : ctx_ { execution_context }
        , timer_ { execution_context }
        , desktop_ { std::move(desktop) }
        , gpu_ { std::move(gpu) }
        , frame_time_ { frame_time }
        , codec_ { gpu_.create_encoder(codec_name, desktop_.size()) }
    {
    }

    VideoCapture(VideoCapture const&) = delete;
    auto operator=(VideoCapture const&) -> VideoCapture& = delete;

    template <typename Completion>
    auto run(MediaContainer& container, Completion&& completion) -> void
    {
        VideoCaptureLoopOperation(ctx_,
                                  desktop_,
                                  gpu_,
                                  codec_.get(),
                                  container,
                                  timer_,
                                  std::move(completion))
            .initiate();
    }

    auto cancel() noexcept -> void { timer_.cancel(); }

    auto codec() const noexcept -> AVCodecContext* { return codec_.get(); }

private:
    exios::Context ctx_;
    StickyCancelTimer timer_;
    Desktop desktop_;
    Gpu gpu_;
    FrameTime frame_time_;
    CodecContextPtr codec_;
};

} // namespace sc
#endif // SHADOW_CAST_VIDEO_CAPTURE_HPP_INCLUDED
