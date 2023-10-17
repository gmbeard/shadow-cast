#include "./codec.hpp"

namespace sc
{
auto CodecContextDeleter::operator()(AVCodecContext* ptr) noexcept -> void
{
    avcodec_free_context(&ptr);
}
} // namespace sc
