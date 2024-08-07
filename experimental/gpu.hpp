#ifndef SHADOW_CAST_GPU_HPP_INCLUDED
#define SHADOW_CAST_GPU_HPP_INCLUDED

#include "desktop.hpp"
#include "nvidia_gpu.hpp"
#include <EGL/egl.h>
#include <variant>
namespace sc
{
using SupportedGpu = std::variant<NvidiaGpu>;

namespace detail
{
auto determine_gpu(EGLDisplay) -> SupportedGpu;
}

template <typename Desktop>
auto determine_gpu(Desktop const& desktop) -> SupportedGpu
{
    return detail::determine_gpu(desktop.egl_display());
}

auto determine_gpu(SupportedDesktop const& desktop) -> SupportedGpu;

} // namespace sc
#endif // SHADOW_CAST_GPU_HPP_INCLUDED
