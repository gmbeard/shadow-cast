#include "./nvfbc_capture_source.hpp"
#include "nvfbc.hpp"
#include "nvidia.hpp"
#include "sticky_cancel_timer.hpp"
#include "utils/cmd_line.hpp"

namespace sc
{
NvfbcCaptureSource::NvfbcCaptureSource(exios::Context ctx,
                                       Parameters const& params,
                                       VideoOutputSize desktop_resolution)
    : ctx_ { ctx }
    , timer_ { ctx }
    , frame_interval_ { params.frame_time.value() }
    , nvfbc_session_ { make_nvfbc_session() }
{
    NVFBC_SIZE size { .w = desktop_resolution.width,
                      .h = desktop_resolution.height };
    if (params.resolution) {
        size.w = params.resolution->width;
        size.h = params.resolution->height;
    }

    create_nvfbc_capture_session(
        nvfbc_session_.get(), nvfbc(), params.frame_time, size);
}

NvfbcCaptureSource::~NvfbcCaptureSource()
{
    if (nvfbc_session_)
        destroy_nvfbc_capture_session(nvfbc_session_.get(), nvfbc());
}

auto NvfbcCaptureSource::context() const noexcept -> exios::Context const&
{
    return ctx_;
}

auto NvfbcCaptureSource::interval() const noexcept -> std::chrono::nanoseconds
{
    return frame_interval_;
}

auto NvfbcCaptureSource::timer() noexcept -> StickyCancelTimer&
{
    return timer_;
}

auto NvfbcCaptureSource::cancel() noexcept -> void { timer_.cancel(); }

} // namespace sc
