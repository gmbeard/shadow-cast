#include "av/frame.hpp"
#include <stdexcept>

namespace sc
{

auto FrameDeleter::operator()(AVFrame* ptr) const noexcept -> void
{
    av_frame_free(&ptr);
}


AVFrameUnrefGuard::~AVFrameUnrefGuard()
{
    if (frame)
        av_frame_unref(frame.get());
}

} // namespace sc
