#ifndef SHADOW_CAST_SERVICES_MEDIA_WRITER_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_MEDIA_WRITER_HPP_INCLUDED

#include "services/context.hpp"
#include "services/encoder_service.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/pool.hpp"

namespace sc
{
struct Encoder
{
    using Frame = typename EncoderService::PoolType::ItemPtr;
    explicit Encoder(Context&) noexcept;

    auto write_frame(Frame&&) -> void;
    auto flush(BorrowedPtr<AVCodecContext>, BorrowedPtr<AVStream>) -> void;

    auto prepare_frame(BorrowedPtr<AVCodecContext>, BorrowedPtr<AVStream>)
        -> Frame;

private:
    sc::BorrowedPtr<EncoderService> svc_;
};
} // namespace sc

#endif // SHADOW_CAST_SERVICES_MEDIA_WRITER_HPP_INCLUDED
