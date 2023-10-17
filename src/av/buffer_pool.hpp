#ifndef SHADOW_CAST_AV_BUFFER_POOL_HPP_INCLUDED
#define SHADOW_CAST_AV_BUFFER_POOL_HPP_INCLUDED

#include "av/fwd.hpp"
#include <memory>

namespace sc
{
struct BufferPoolDeleter
{
    auto operator()(AVBufferPool* ptr) const noexcept -> void;
};

using BufferPoolPtr = std::unique_ptr<AVBufferPool, BufferPoolDeleter>;
} // namespace sc
#endif // SHADOW_CAST_AV_BUFFER_POOL_HPP_INCLUDED
