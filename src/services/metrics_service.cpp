#include "services/metrics_service.hpp"
#include "utils/scope_guard.hpp"
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;
using namespace std::literals::string_literals;

namespace sc
{
MetricsService::MetricsService(std::string const& media_output_file)
    : media_output_file_ { media_output_file }
{
}

auto MetricsService::on_init(ReadinessRegister reg) -> void
{
    output_fd_ = ::open((media_output_file_ + ".metrics").c_str(),
                        O_CREAT | O_TRUNC | O_RDWR,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

    if (output_fd_ < 0)
        throw std::runtime_error {
            "Failed to open metrics logfile for writing"s + std::strerror(errno)
        };

    reg(FrameTimeRatio(10, 1), &dispatch);
}

auto MetricsService::on_uninit() noexcept -> void
{
    dispatch(*this);
    ::syncfs(output_fd_);
    ::close(output_fd_);
}

auto MetricsService::post_time_metric(TimeMetric metric) -> void
{
    auto item = pool_.get();
    *item = TimeMetricListItem { metric };
    std::lock_guard lock { data_mutex_ };
    metric_buffer_.push_back(item.release());
}

auto MetricsService::dispatch(Service& svc) -> void
{
    auto& self = static_cast<MetricsService&>(svc);

    IntrusiveList<TimeMetricListItem> tmp;

    {
        std::lock_guard lock { self.data_mutex_ };
        tmp.splice(tmp.begin(),
                   self.metric_buffer_,
                   self.metric_buffer_.begin(),
                   self.metric_buffer_.end());
    }

    ReturnToPoolGuard guard { tmp, self.pool_ };

    for (auto const& metric : tmp) {
        dprintf(self.output_fd_,
                "%u,%lu,%lu,%lu,%lu,%lu\n",
                metric.category,
                metric.id,
                metric.timestamp_ns,
                metric.nanoseconds,
                metric.frame_size,
                metric.frame_count);
    }
}

MetricsService::TimeMetricListItem::TimeMetricListItem(
    TimeMetric const& item) noexcept
    : TimeMetric { item }
    , ListItemBase {}
{
}

} // namespace sc
