#include "platform/capture.hpp"
#include "nvidia.hpp"
#include "platform/display.hpp"
#include "platform/gpu.hpp"
#include "utils/overloads.hpp"

namespace sc
{
NvFBCCapture::~NvFBCCapture()
{
    if (nvfbc_session)
        sc::destroy_nvfbc_capture_session(nvfbc_session.get(), nvfbc_instance);
}
NvFBCCapture::NvFBCCapture(NvFBCLib lib,
                           NvFBC&& instance,
                           NvFBCSessionHandlePtr&& session) noexcept
    : nvfbclib { lib }
    , nvfbc_instance { std::move(instance) }
    , nvfbc_session { std::move(session) }
{
}

auto select_capture(Parameters const& params,
                    DisplayEnvironment const& display,
                    GPU const& gpu) -> Capture
{
    auto const nvfbc = [&](X11Environment const&, Nvidia const&) -> Capture {
        auto nvfbclib = load_nvfbc();
        auto nvfbc_instance = nvfbclib.NvFBCCreateInstance();
        auto nvfbc_session = sc::create_nvfbc_session(nvfbc_instance);
        create_nvfbc_capture_session(nvfbc_session.get(),
                                     nvfbc_instance,
                                     params.frame_time,
                                     params.resolution);

        return NvFBCCapture { std::move(nvfbclib),
                              std::move(nvfbc_instance),
                              std::move(nvfbc_session) };
    };

    auto const nvegl = [&](WaylandEnvironment const& env,
                           Nvidia const&) -> Capture {
        auto egl = load_egl();
        auto wayland_egl = initialize_wayland_egl(egl, *env.platform);
        return EGLCapture { std::move(egl), std::move(wayland_egl) };
    };

    auto const unsupported = [&](auto const& /*display*/,
                                 auto const& /*gpu*/) -> Capture {
        throw std::runtime_error {
            "Capture not supported for display environment / GPU"
        };
    };

    return std::visit(Overloads { nvfbc, nvegl, unsupported }, display, gpu);
}
} // namespace sc
