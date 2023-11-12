#ifndef SHADOW_CAST_DRM_MESSAGING_HPP_INCLUDED
#define SHADOW_CAST_DRM_MESSAGING_HPP_INCLUDED

#include "drm/planes.hpp"
#include "io/message_handler.hpp"

namespace sc
{

std::size_t constexpr kMaxPlaneDescriptors = 8;

namespace drm_request
{
[[maybe_unused]] std::uint32_t constexpr kGetPlanes = 1;
[[maybe_unused]] std::uint32_t constexpr kStop = 2;
} // namespace drm_request

struct DRMRequest
{
    std::uint32_t type;
};

struct DRMResponse
{
    std::uint32_t result;
    std::uint32_t num_fds;
    PlaneDescriptor descriptors[kMaxPlaneDescriptors];
};

struct DRMResponseSendHandler
{
    auto operator()(int, msghdr&, DRMResponse const&) noexcept -> ssize_t;
};

struct DRMResponseReceiveHandler
{
    auto operator()(int, msghdr&, DRMResponse&) noexcept -> ssize_t;
};

using DRMResponseSender =
    MessageHandler<DRMResponse, DRMResponseSendHandler, decltype(io::write)>;

using DRMResponseReceiver =
    MessageHandler<DRMResponse, DRMResponseReceiveHandler, decltype(io::write)>;

} // namespace sc

#endif // SHADOW_CAST_DRM_MESSAGING_HPP_INCLUDED
