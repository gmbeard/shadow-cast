#include "./nvfbc_capture_source.hpp"
#include "nvfbc.hpp"
#include "nvfbc_gpu.hpp"
#include "utils/cmd_line.hpp"

namespace sc
{
NvfbcCaptureSource::NvfbcCaptureSource(exios::Context ctx,
                                       Parameters const& params,
                                       VideoOutputSize desktop_resolution)
    : ctx_ { ctx }
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

} // namespace sc
