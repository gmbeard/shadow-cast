#ifndef SHADOW_CAST_AV_BUFFER_HPP_INCLUDED
#define SHADOW_CAST_AV_BUFFER_HPP_INCLUDED

#include "av/frame.hpp"
#include "av/fwd.hpp"
#include <memory>
namespace sc
{

struct BufferDeleter
{
    auto operator()(AVBufferRef* ptr) const noexcept -> void;
};

using BufferPtr = std::unique_ptr<AVBufferRef, BufferDeleter>;

auto initialize_writable_buffer(FramePtr::pointer) -> void;

} // namespace sc

#endif // SHADOW_CAST_AV_BUFFER_HPP_INCLUDED
