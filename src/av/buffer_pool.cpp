#include "av/buffer_pool.hpp"

namespace sc
{

auto BufferPoolDeleter::operator()(AVBufferPool* ptr) const noexcept -> void
{
    av_buffer_pool_uninit(&ptr);
}

} // namespace sc
