#include "cpu_usage.hpp"
#include "logging.hpp"
#include <bits/types/struct_rusage.h>
#include <chrono>
#include <cmath>
#include <sys/resource.h>

namespace sc
{
auto get_cpu_usage() noexcept -> std::uint64_t
{
    namespace ch = std::chrono;

    rusage metrics;
    if (auto const r = getrusage(RUSAGE_SELF, &metrics); r < 0) {
        log(LogLevel::warn, "Couldn't read CPU usage");
        return 0;
    }

    auto const usage_user_ns =
        ch::duration_cast<ch::nanoseconds>(ch::seconds(metrics.ru_utime.tv_sec))
            .count() +
        ch::duration_cast<ch::nanoseconds>(
            ch::microseconds(metrics.ru_utime.tv_usec))
            .count();

    auto const usage_sys_ns =
        ch::duration_cast<ch::nanoseconds>(ch::seconds(metrics.ru_stime.tv_sec))
            .count() +
        ch::duration_cast<ch::nanoseconds>(
            ch::microseconds(metrics.ru_stime.tv_usec))
            .count();

    return usage_user_ns + usage_sys_ns;
}

} // namespace sc
