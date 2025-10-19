#include "drm_device.hpp"

drm_device::drm_device(std::string const& path) noexcept
    : fd_ { ::open(path.c_str(), O_RDONLY) }
{
    if (fd_ < 0)
        fd_ = kNoDevice;
    else
        drmSetClientCap(fd_, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
}

drm_device::drm_device(drm_device&& other) noexcept
    : fd_(std::__exchange(other.fd_, kNoDevice))
{
}

drm_device::~drm_device()
{
    if (fd_ == kNoDevice)
        return;

    ::close(fd_);
}

drm_device::operator bool() const noexcept
{
    return fd_ != kNoDevice;
}

auto drm_device::native_handle() const noexcept -> int const&
{
    return fd_;
}

drm_device::operator int() const noexcept
{
    return fd_;
}

auto swap(drm_device& lhs, drm_device& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.fd_, rhs.fd_);
}

auto drm_device::operator=(drm_device&& rhs) noexcept -> drm_device&
{
    drm_device tmp { std::move(rhs) };
    swap(*this, tmp);
    return *this;
}

auto operator==(drm_device const& lhs, drm_device const& rhs) noexcept -> bool
{
    return lhs.fd_ == rhs.fd_;
}

auto drm_device::planes() const noexcept -> drm_planes
{
    return drm_planes { *this };
}

auto open_drm_device() -> std::optional<drm_device>
{
    std::string_view const prefix = "/dev/dri/card";
    char const suffixes[] = { '0', '1', '2', '3' };

    std::string buffer(prefix.size() + 1, '\0');

    for (auto suffix : suffixes) {
        auto last = std::copy(prefix.begin(), prefix.end(), buffer.begin());
        *last++ = suffix;

        auto const drm_fd = ::open(buffer.c_str(), O_RDONLY);
        if (drm_fd < 0)
            continue;

        SC_SCOPE_GUARD([&] { ::close(drm_fd); });

        auto const ver = drmGetVersion(drm_fd);
        if (!ver)
            continue;

        SC_SCOPE_GUARD([&] { drmFreeVersion(ver); });

        return drm_device { buffer };
    }

    return std::nullopt;
}
