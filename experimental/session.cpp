#include "session.hpp"
#include "av/codec.hpp"
#include "drm_cuda_capture_source.hpp"
#include "nvenc_encoder_sink.hpp"
#include "nvfbc_capture_source.hpp"

namespace sc
{
namespace detail
{
auto create_video_capture(exios::Context const& execution_context,
                          Parameters const params,
                          MediaContainer& container,
                          sc::SupportedDesktop const& desktop,
                          sc::SupportedGpu const& gpu) -> Capture
{
    struct Visitor
    {
        auto operator()(WaylandDesktop const& desktop,
                        NvidiaGpu const& gpu) const -> Capture
        {
            VideoOutputSize dimensions = desktop.size();
            if (params.resolution) {
                dimensions.width = params.resolution->width;
                dimensions.height = params.resolution->height;
            }

            sc::DRMCudaCaptureSource video_source { execution_context,
                                                    params,
                                                    dimensions,
                                                    gpu.cuda_context(),
                                                    desktop.egl_display() };
            sc::NvencEncoderSink video_sink {
                execution_context, gpu.cuda_context(),
                container,         params,
                desktop.size(),    NvencEncoderSink::PixelFormat::rgba
            };

            return Capture { std::move(video_source), std::move(video_sink) };
        }

        auto operator()(X11Desktop const& desktop, NvidiaGpu const& gpu) const
            -> Capture
        {
            sc::NvfbcCaptureSource video_source { execution_context,
                                                  params,
                                                  desktop.size() };
            sc::NvencEncoderSink video_sink {
                execution_context, gpu.cuda_context(),
                container,         params,
                desktop.size(),    NvencEncoderSink::PixelFormat::bgra
            };

            return Capture { std::move(video_source), std::move(video_sink) };
        }

        exios::Context const& execution_context;
        Parameters const& params;
        MediaContainer& container;
    };

    return std::visit(
        Visitor { execution_context, params, container }, desktop, gpu);
}
} // namespace detail
} // namespace sc
