#ifndef SHADOW_CAST_GPU_HPP_INCLUDED
#define SHADOW_CAST_GPU_HPP_INCLUDED

#include "desktop.hpp"
#include "nvidia_gpu.hpp"
#include <EGL/egl.h>
#include <string_view>
#include <variant>
namespace sc
{
using SupportedGpu = std::variant<NvidiaGpu>;

namespace detail
{
auto determine_gpu(std::string_view vendor) -> SupportedGpu;
}

auto determine_gpu(SupportedDesktop const& desktop) -> SupportedGpu;

} // namespace sc
#endif // SHADOW_CAST_GPU_HPP_INCLUDED
