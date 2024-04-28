#ifndef SHADOW_CAST_GPU_TRAITS_HPP_INCLUDED
#define SHADOW_CAST_GPU_TRAITS_HPP_INCLUDED

#include "av/codec.hpp"
#include <type_traits>

namespace sc
{
template <typename Gpu>
struct GpuTraits
{
    using GpuType = std::decay_t<Gpu>;
    using GpuFrameType = typename GpuType::FrameType;

    template <typename DesktopEnvironment>
    static auto create_encoder(GpuType const& gpu,
                               std::string_view codec_name,
                               DesktopEnvironment const& desktop,
                               FrameTime frame_time) -> CodecContextPtr;

    template <typename DesktopEnvironment, typename Completion>
    static auto capture_frame(GpuType const& gpu,
                              DesktopEnvironment const& desktop,
                              Completion&& completion) -> void;
};

template <typename Gpu>
template <typename DesktopEnvironment>
auto GpuTraits<Gpu>::create_encoder(GpuType const& gpu,
                                    std::string_view codec_name,
                                    DesktopEnvironment const& desktop,
                                    FrameTime frame_time) -> CodecContextPtr
{
    return gpu.create_encoder(codec_name, desktop, frame_time);
}

template <typename Gpu>
template <typename DesktopEnvironment, typename Completion>
auto GpuTraits<Gpu>::capture_frame(GpuType const& gpu,
                                   DesktopEnvironment const& desktop,
                                   Completion&& completion) -> void
{
    gpu.capture_frame(desktop, std::forward<Completion>(completion));
}

} // namespace sc

#endif // SHADOW_CAST_GPU_TRAITS_HPP_INCLUDED
