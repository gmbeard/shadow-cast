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

    auto operator()(MediaChunk const& chunk) -> void;

private:
    BorrowedPtr<AVFormatContext> format_context_;
    BorrowedPtr<AVCodecContext> codec_context_;
    BorrowedPtr<AVStream> stream_;
    FramePtr frame_;
    std::size_t total_samples_written_ { 0 };
};

auto send_frame(AVFrame* frame,
                AVCodecContext* ctx,
                AVFormatContext* fmt,
                AVStream* stream) -> void;

} // namespace sc

#endif // SHADOW_CAST_HANDLERS_AUDIO_CHUNK_WRITER_HPP_INCLUDED
