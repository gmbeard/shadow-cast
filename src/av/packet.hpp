#ifndef SHADOW_CAST_AV_PACKET_HPP_INCLUDED
#define SHADOW_CAST_AV_PACKET_HPP_INCLUDED

#include "./fwd.hpp"
#include <memory>

namespace sc
{

struct PacketPtrDeleter
{
    auto operator()(AVPacket* ptr) const noexcept -> void;
};

struct PacketUnrefGuard
{
    ~PacketUnrefGuard();

    AVPacket* packet;
};

using PacketPtr = std::unique_ptr<AVPacket, PacketPtrDeleter>;

} // namespace sc
#endif // SHADOW_CAST_AV_PACKET_HPP_INCLUDED
