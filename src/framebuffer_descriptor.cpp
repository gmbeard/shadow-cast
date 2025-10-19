#include "framebuffer_descriptor.hpp"
#include "utils/contracts.hpp"
#include <atomic>
#include <chrono>

namespace sc
{
auto write_next_sequence(framebuffer_descriptor const& source,
                         framebuffer_descriptor_sequence& target) noexcept
    -> void
{
    auto seq = target.sequence.load(std::memory_order_relaxed);
    auto const next =
        target.sequence.exchange(seq + 1, std::memory_order_release) + 2;

    target.value = source;

    std::atomic_thread_fence(std::memory_order_release);
    target.sequence.store(next, std::memory_order_release);
}

auto read_next_sequence(framebuffer_descriptor_sequence& source,
                        long timeout_milliseconds) noexcept
    -> framebuffer_descriptor
{
    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::milliseconds;

    auto const start_time = high_resolution_clock::now();
    using seqlock_type = decltype(framebuffer_descriptor_sequence::sequence);

    framebuffer_descriptor target {};

    while (true) {
        auto const lock_duration =
            duration_cast<milliseconds>(high_resolution_clock::now() -
                                        start_time)
                .count();
        SC_EXPECT(lock_duration < timeout_milliseconds);

        auto const seq_start = source.sequence.load(std::memory_order_acquire);
        if ((seq_start & seqlock_type(1)) == 1)
            continue;

        target = source.value;

        std::atomic_thread_fence(std::memory_order_acquire);
        auto const seq_end = source.sequence.load(std::memory_order_acquire);
        if (seq_end == seq_start)
            break;
    }

    return target;
}
} // namespace sc
