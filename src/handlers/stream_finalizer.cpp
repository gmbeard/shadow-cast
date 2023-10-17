#include "handlers/stream_finalizer.hpp"
#include "error.hpp"
#include "handlers/audio_chunk_writer.hpp"
#include <stdexcept>

namespace sc
{

StreamFinalizer::StreamFinalizer(
    BorrowedPtr<AVFormatContext> format_context,
    BorrowedPtr<AVCodecContext> audio_codec_context,
    BorrowedPtr<AVCodecContext> video_codec_context)
    : format_context_ { format_context }
    , audio_codec_context_ { audio_codec_context }
    , video_codec_context_ { video_codec_context }
{
}

auto StreamFinalizer::operator()() const -> void
{
    /* Send a end marker to the audio stream...
     */
    sc::send_frame(
        nullptr, audio_codec_context_.get(), format_context_.get(), 0);

    /* Hijack this handler to send an end marker to the video stream, too...
     */
    sc::send_frame(
        nullptr, video_codec_context_.get(), format_context_.get(), 1);

    if (auto const ret = av_write_trailer(format_context_.get()); ret < 0)
        throw std::runtime_error { "Failed to write trailer: " +
                                   sc::av_error_to_string(ret) };
}
} // namespace sc
