#include "./frame.hpp"

namespace sc
{
auto BufferDeleter::operator()(AVBufferRef* ptr) const noexcept -> void
{
    av_buffer_unref(&ptr);
}

auto FrameDeleter::operator()(AVFrame* ptr) const noexcept -> void
{
    av_frame_free(&ptr);
}

} // namespace sc
