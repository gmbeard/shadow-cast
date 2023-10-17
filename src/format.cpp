#include "./format.hpp"

namespace sc
{

auto FormatContextDeleter::operator()(AVFormatContext* ptr) noexcept -> void
{
    avformat_free_context(ptr);
}

} // namespace sc
