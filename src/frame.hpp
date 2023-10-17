#ifndef SHADOW_CAPTURE_FRAME_HPP_INCLUDED
#define SHADOW_CAPTURE_FRAME_HPP_INCLUDED

extern "C" {
#include "libavutil/buffer.h"
#include "libavutil/frame.h"
}

#include <memory>
namespace sc
{
struct BufferDeleter
{
    auto operator()(AVBufferRef* ptr) const noexcept -> void;
};

struct FrameDeleter
{
    auto operator()(AVFrame* ptr) const noexcept -> void;
};

using BufferPtr = std::unique_ptr<AVBufferRef, BufferDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
} // namespace sc

#endif // SHADOW_CAPTURE_FRAME_HPP_INCLUDED
