#ifndef SHADOW_CAST_SERVICES_METRICS_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_METRICS_SERVICE_HPP_INCLUDED

#include "services/readiness.hpp"
#include "services/service.hpp"
#include "utils/intrusive_list.hpp"
#include "utils/pool.hpp"
#include <cinttypes>
#include <cstddef>
#include <mutex>

namespace sc
{

struct TimeMetric
{
    std::uint32_t category;
    std::size_t id;
    std::size_t timestamp_ns;
    std::size_t nanoseconds;
    std::size_t frame_size;
    std::size_t frame_count;
};

static_assert(std::is_standard_layout_v<TimeMetric>,
              "TimeMetric must be a standard layout type");

struct MetricsService final : Service
{
    MetricsService(std::string const&);
    MetricsService(MetricsService const&) = delete;
    auto operator=(MetricsService const&) -> MetricsService& = delete;

    auto post_time_metric(TimeMetric) -> void;

protected:
    auto on_init(ReadinessRegister) -> void override;
    auto on_uninit() noexcept -> void override;

private:
    static auto dispatch(Service&) -> void;

private:
    struct TimeMetricListItem : TimeMetric, ListItemBase
    {
        TimeMetricListItem() noexcept = default;
        TimeMetricListItem(TimeMetric const&) noexcept;
    };

    std::string media_output_file_;
    IntrusiveList<TimeMetricListItem> metric_buffer_;
    SynchronizedPool<TimeMetricListItem> pool_;
    std::mutex data_mutex_ {};
    int output_fd_ { -1 };
};

} // namespace sc

#endif // SHADOW_CAST_SERVICES_METRICS_SERVICE_HPP_INCLUDED
