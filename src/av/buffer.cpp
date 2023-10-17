#include "av/buffer.hpp"

namespace sc
{

auto BufferDeleter::operator()(AVBufferRef* ptr) const noexcept -> void
{
    av_buffer_unref(&ptr);
}

auto initialize_writable_buffer(FramePtr::pointer frame) -> void
{
    char error_buffer[AV_ERROR_MAX_STRING_SIZE];
    if (auto averr = av_frame_get_buffer(frame, 0); averr < 0) {
        av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, averr);
        throw std::runtime_error { error_buffer };
    }
    if (av_frame_make_writable(frame) < 0)
        throw std::runtime_error { "Couldn't make frame writable" };
}
} // namespace sc
