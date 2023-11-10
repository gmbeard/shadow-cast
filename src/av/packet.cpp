#include "av/packet.hpp"

namespace sc
{
auto PacketPtrDeleter::operator()(AVPacket* ptr) const noexcept -> void
{
    av_packet_free(&ptr);
}

PacketUnrefGuard::~PacketUnrefGuard() { av_packet_unref(packet); }

} // namespace sc
