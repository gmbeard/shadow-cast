#include "exios/exios.hpp"
#include "frame_capture.hpp"
#include "testing.hpp"
#include <chrono>
#include <memory>
#include <system_error>

struct TestSource
{
    template <typename Completion>
    auto capture(std::unique_ptr<int> val, Completion&& completion) -> void
    {
        auto const alloc = exios::select_allocator(completion);
        auto f = [val = std::move(val),
                  completion = std::move(completion)]() mutable {
            *val += 1;
            std::move(completion)(
                exios::Result<std::unique_ptr<int>, std::error_code> {
                    exios::result_ok(std::move(val)) });
        };

        ctx.post(std::move(f), alloc);
    }

    exios::Context ctx;
};

struct TestSink
{
    auto prepare() noexcept { return std::make_unique<int>(value); }

    template <typename Completion>
    auto write(std::unique_ptr<int> captured_value, Completion&& completion)
        -> void
    {
        auto const alloc = exios::select_allocator(completion);
        auto f = [this,
                  captured_value = std::move(captured_value),
                  completion = std::move(completion)]() mutable {
            value = *captured_value;
            std::move(completion)(exios::Result<std::error_code> {});
        };

        ctx.post(std::move(f), alloc);
    }

    exios::Context ctx;
    int value;
};

auto should_run_capture_frame_op() -> void
{
    exios::ContextThread execution_context;
    TestSource source { execution_context };
    TestSink sink { execution_context, 42 };

    sc::frame_capture(
        source, sink, [&](sc::FrameCaptureResult result) { EXPECT(result); });

    static_cast<void>(execution_context.run());
    EXPECT(sink.value == 43);
}

/* Just some sanity checks around `std::chrono::duration`
 * arithmetic...
 */
auto should_result_in_correct_missed_frames() -> void
{
    namespace ch = std::chrono;

    std::size_t n = ch::seconds(21) / ch::milliseconds(5000);

    EXPECT(n == 4);

    n = ch::seconds(5) / ch::milliseconds(6001);

    EXPECT(n == 0);
}

auto main() -> int
{
    return testing::run({ TEST(should_run_capture_frame_op),
                          TEST(should_result_in_correct_missed_frames) });
}
