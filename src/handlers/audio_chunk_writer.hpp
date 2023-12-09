#ifndef SHADOW_CAST_HANDLERS_AUDIO_CHUNK_WRITER_HPP_INCLUDED
#define SHADOW_CAST_HANDLERS_AUDIO_CHUNK_WRITER_HPP_INCLUDED

#include "av.hpp"
#include "services/encoder.hpp"
#include "utils/borrowed_ptr.hpp"

namespace sc
{

struct ChunkWriter
{
    explicit ChunkWriter(AVCodecContext* codec_context,
                         AVStream* stream,
                         Encoder encoder) noexcept;

    auto operator()(MediaChunk const& chunk) -> void;

private:
    BorrowedPtr<AVCodecContext> codec_context_;
    BorrowedPtr<AVStream> stream_;
    Encoder encoder_;
    FramePtr frame_;
    std::size_t total_samples_written_ { 0 };
};

} // namespace sc

#endif // SHADOW_CAST_HANDLERS_AUDIO_CHUNK_WRITER_HPP_INCLUDED
