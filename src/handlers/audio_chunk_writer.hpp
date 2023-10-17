#ifndef SHADOW_CAST_HANDLERS_AUDIO_CHUNK_WRITER_HPP_INCLUDED
#define SHADOW_CAST_HANDLERS_AUDIO_CHUNK_WRITER_HPP_INCLUDED

#include "av.hpp"
#include "utils/borrowed_ptr.hpp"

namespace sc
{

struct ChunkWriter
{
    explicit ChunkWriter(AVFormatContext* format_context,
                         AVCodecContext* codec_context,
                         AVStream* stream) noexcept;

    auto operator()(sc::MediaChunk chunk) -> void;

private:
    sc::BorrowedPtr<AVFormatContext> format_context_;
    sc::BorrowedPtr<AVCodecContext> codec_context_;
    sc::BorrowedPtr<AVStream> stream_;
    sc::FramePtr frame_;
};

auto send_frame(AVFrame* frame,
                AVCodecContext* ctx,
                AVFormatContext* fmt,
                int stream_index) -> void;

} // namespace sc

#endif // SHADOW_CAST_HANDLERS_AUDIO_CHUNK_WRITER_HPP_INCLUDED
