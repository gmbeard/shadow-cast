#include "gpu.hpp"
#include "logging.hpp"
#include "nvidia_gpu.hpp"
#include "platform/egl.hpp"
#include "platform/opengl.hpp"
#include "utils/contracts.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>

namespace
{
enum struct GpuVendor
{
    nvidia,
    unsupported,
};

auto get_vendor(std::string_view vendor_string) noexcept -> GpuVendor
{
    constexpr std::array<std::pair<std::string_view const, GpuVendor>, 1> lut {
        { { "NVIDIA", GpuVendor::nvidia } }
    };

    for (auto const& val : lut) {
        if (auto pos = std::search(vendor_string.begin(),
                                   vendor_string.end(),
                                   std::get<0>(val).begin(),
                                   std::get<0>(val).end());
            pos != vendor_string.end())
            return std::get<1>(val);
    }

    return GpuVendor::unsupported;
}

} // namespace

namespace sc
{
namespace detail
{
auto determine_gpu(EGLDisplay display) -> SupportedGpu
{
    SC_EXPECT(display);

    std::string_view vendor = egl().eglQueryString(display, EGL_VENDOR);
    log(LogLevel::info, "GPU vendor: %.*s", vendor.size(), vendor.data());

    std::string_view renderer =
        reinterpret_cast<char const*>(sc::gl().glGetString(GL_RENDERER));
    log(LogLevel::info, "GPU: %.*s", renderer.size(), renderer.data());

    switch (get_vendor(vendor)) {
    case GpuVendor::nvidia:
        return NvidiaGpu {};
    default:
        throw std::runtime_error { "Unsupported GPU" };
    }
}
} // namespace detail

auto determine_gpu(SupportedDesktop const& supported_desktop) -> SupportedGpu
{
    return std::visit(
        [](auto const& desktop) {
            return detail::determine_gpu(desktop.egl_display());
        },
        supported_desktop);
}
} // namespace sc