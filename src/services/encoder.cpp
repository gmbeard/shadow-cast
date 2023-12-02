#include "services/encoder.hpp"
#include "utils/contracts.hpp"
#include "utils/pool.hpp"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

namespace sc
{
Encoder::Encoder(Context& ctx) noexcept
    : svc_ { ctx.services().use_if<EncoderService>() }
{
    SC_EXPECT(svc_);
}

auto Encoder::write_frame(Frame&& item) -> void
{
    svc_->write_frame(std::move(item));
}

auto Encoder::flush(BorrowedPtr<AVCodecContext> codec_ctx,
                    BorrowedPtr<AVStream> stream) -> void
{
    auto encoder_frame = prepare_frame(codec_ctx, stream);
    encoder_frame->frame.reset();
    svc_->write_frame(std::move(encoder_frame));
}

auto Encoder::prepare_frame(BorrowedPtr<AVCodecContext> codec_ctx,
                            BorrowedPtr<AVStream> stream) -> Frame
{
    auto pool_item = svc_->pool().get();
    pool_item->codec_ctx = codec_ctx.get();
    pool_item->stream = stream.get();
    return pool_item;
}

} // namespace sc
