#ifndef SHADOW_CAST_HANDLERS_STREAM_FINALIZER_HPP_INCLUDED
#define SHADOW_CAST_HANDLERS_STREAM_FINALIZER_HPP_INCLUDED

#include "av/fwd.hpp"
#include "av/packet.hpp"
#include "utils/borrowed_ptr.hpp"

namespace sc
{

struct StreamFinalizer
{
    StreamFinalizer(BorrowedPtr<AVFormatContext> format_context,
                    BorrowedPtr<AVCodecContext> audio_codec_context,
                    BorrowedPtr<AVCodecContext> video_codec_context,
                    BorrowedPtr<AVStream> audio_stream,
                    BorrowedPtr<AVStream> video_stream);

    auto operator()() const -> void;

private:
    BorrowedPtr<AVFormatContext> format_context_;
    BorrowedPtr<AVCodecContext> audio_codec_context_;
    BorrowedPtr<AVCodecContext> video_codec_context_;
    BorrowedPtr<AVStream> audio_stream_;
    BorrowedPtr<AVStream> video_stream_;
    PacketPtr packet_;
};

} // namespace sc

#endif // SHADOW_CAST_HANDLERS_STREAM_FINALIZER_HPP_INCLUDED
