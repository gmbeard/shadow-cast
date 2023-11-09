#include "handlers/stream_finalizer.hpp"
#include "error.hpp"
#include "handlers/audio_chunk_writer.hpp"
#include <stdexcept>

namespace sc
{

StreamFinalizer::StreamFinalizer(
    BorrowedPtr<AVFormatContext> format_context,
    BorrowedPtr<AVCodecContext> audio_codec_context,
    BorrowedPtr<AVCodecContext> video_codec_context,
    BorrowedPtr<AVStream> audio_stream,
    BorrowedPtr<AVStream> video_stream)
    : format_context_ { format_context }
    , audio_codec_context_ { audio_codec_context }
    , video_codec_context_ { video_codec_context }
    , audio_stream_ { audio_stream }
    , video_stream_ { video_stream }
{
}

auto StreamFinalizer::operator()() const -> void
{
    /* Send a end marker to the audio stream...
     */
    sc::send_frame(nullptr,
                   audio_codec_context_.get(),
                   format_context_.get(),
                   audio_stream_.get());

    /* Hijack this handler to send an end marker to the video stream, too...
     */
    sc::send_frame(nullptr,
                   video_codec_context_.get(),
                   format_context_.get(),
                   video_stream_.get());
}
} // namespace sc
