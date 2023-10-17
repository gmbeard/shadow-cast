#ifndef SHADOW_CAST_AV_FRAME_HPP_INCLUDED
#define SHADOW_CAST_AV_FRAME_HPP_INCLUDED

#include "av/fwd.hpp"
#include "utils/borrowed_ptr.hpp"
#include <memory>

namespace sc
{
struct FrameDeleter
{
    auto operator()(AVFrame* ptr) const noexcept -> void;
};

using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

struct AVFrameUnrefGuard
{
    ~AVFrameUnrefGuard();
    sc::BorrowedPtr<AVFrame> frame;
};

} // namespace sc

#endif // SHADOW_CAST_AV_FRAME_HPP_INCLUDED
