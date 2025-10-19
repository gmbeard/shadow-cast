#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_DMABUF_EXPORTER_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_DMABUF_EXPORTER_HPP_INCLUDED

#include "av/media_chunk.hpp"
#include "drm/dmabuf_reply_message.hpp"
#include "drm_device.hpp"
#include "exios/unix_socket.hpp"
#include "frame_time_detector.hpp"
#include "framebuffer_descriptor.hpp"
#include "sticky_cancel_timer.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

struct dmabuf_exporter
{
    using request_type = std::uint32_t;
    using reply_type = sc::dmabuf_reply_message;

    dmabuf_exporter(exios::UnixSocket& socket,
                    std::string_view socket_name,
                    sc::StickyCancelTimer& delay_timer,
                    drm_device const& device,
                    sc::framebuffer_descriptor_sequence& shared_mem,
                    frame_time_estimation const& estimation,
                    duration_type frame_time) noexcept;

    auto start() && -> void;

private:
    auto read_request() && -> void;
    auto detect_phase_correction(request_type) && -> void;
    auto send_reply(std::pair<request_type, std::uint32_t>) && -> void;

    exios::UnixSocket& socket_;
    std::string_view socket_name_;
    sc::StickyCancelTimer& delay_timer_;
    drm_device const& device_;
    sc::framebuffer_descriptor_sequence& shared_mem_;
    frame_time_estimation const& estimation_;
    std::int64_t frame_time_ns_;
    sc::DynamicBuffer buffer_;
    std::vector<iovec> iovec_buffer_;
    std::vector<char> msghdr_buffer_;
    std::size_t export_count_ { 0 };
};

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_DMABUF_EXPORTER_HPP_INCLUDED
