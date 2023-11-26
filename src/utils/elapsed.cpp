#include "utils/elapsed.hpp"

namespace sc
{

auto Elapsed::value() const noexcept -> std::uint64_t
{
    namespace ch = std::chrono;

    auto const now = ch::steady_clock::now();

    return ch::duration_cast<ch::milliseconds>(now - start_).count();
}

auto Elapsed::nanosecond_value() const noexcept -> std::uint64_t
{
    namespace ch = std::chrono;

    auto const now = ch::steady_clock::now();

    return ch::duration_cast<ch::nanoseconds>(now - start_).count();
}

auto Elapsed::reset() noexcept -> void
{
    start_ = std::chrono::steady_clock::now();
}

} // namespace sc
