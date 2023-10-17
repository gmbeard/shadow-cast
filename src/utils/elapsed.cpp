#include "utils/elapsed.hpp"

namespace sc
{

auto Elapsed::value() const noexcept -> std::uint64_t
{
    namespace ch = std::chrono;

    auto const now = ch::steady_clock::now();

    return ch::duration_cast<ch::milliseconds>(now - start_).count();
}

} // namespace sc
