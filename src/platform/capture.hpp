#ifndef SHADOW_CAST_PLATFORM_CAPTURE_HPP_INCLUDED
#define SHADOW_CAST_PLATFORM_CAPTURE_HPP_INCLUDED

#include "nvidia.hpp"
#include "platform/display.hpp"
#include "platform/gpu.hpp"
#include <variant>

namespace sc
{

struct NvFBCCapture
{
    NvFBCCapture(NvFBCLib, NvFBC&&, NvFBCSessionHandlePtr&&) noexcept;
    ~NvFBCCapture();

    NvFBCCapture(NvFBCCapture&&) noexcept = default;
    auto operator=(NvFBCCapture&&) noexcept -> NvFBCCapture& = default;

    NvFBCLib nvfbclib;
    NvFBC nvfbc_instance;
    NvFBCSessionHandlePtr nvfbc_session;
};

struct EGLCapture
{
    EGL egl;
    WaylandEGL egl_objects;
};

using Capture = std::variant<NvFBCCapture, EGLCapture>;

auto select_capture(Parameters const&, DisplayEnvironment const&, GPU const&)
    -> Capture;

} // namespace sc
#endif // SHADOW_CAST_PLATFORM_CAPTURE_HPP_INCLUDED
