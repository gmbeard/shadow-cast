#ifndef SHADOW_CAST_DRM_DMABUF_REPLY_MESSAGE_HPP_INCLUDED
#define SHADOW_CAST_DRM_DMABUF_REPLY_MESSAGE_HPP_INCLUDED

#include <cstdint>

namespace sc
{

struct dmabuf_reply_message
{
    std::uint32_t fb_id { 0 };
    std::uint32_t fd_and_sync_pair_count { 0 };
    std::size_t fd_and_sync_stride { 1 };
    int fb_fd { -1 };
    int sync_fd { -1 };
    std::uint32_t phase_offset_nanoseconds { 0 };
};

} // namespace sc
#endif // SHADOW_CAST_DRM_DMABUF_REPLY_MESSAGE_HPP_INCLUDED
