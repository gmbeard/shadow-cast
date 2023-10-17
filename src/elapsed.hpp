#ifndef SHADOW_CAPTURE_ELAPSED_HPP_INCLUDED
#define SHADOW_CAPTURE_ELAPSED_HPP_INCLUDED

#include <chrono>
#include <cinttypes>

namespace sc
{

struct Elapsed
{
    auto value() const noexcept -> std::uint64_t;

private:
    std::chrono::time_point<std::chrono::steady_clock> start_ =
        std::chrono::steady_clock::now();
};

Elapsed const global_elapsed;
} // namespace sc

#endif // SHADOW_CAPTURE_ELAPSED_HPP_INCLUDED
