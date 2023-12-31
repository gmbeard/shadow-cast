#include "services/context.hpp"
#include "services/readiness.hpp"
#include "utils/elapsed.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <errno.h>
#include <limits>
#include <poll.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <system_error>
#include <tuple>
#include <vector>

std::size_t constexpr kNsPerSec = 1'000'000'000;

namespace
{

struct UninitGuard
{
    ~UninitGuard()
    {
        for (auto& [id, svc] : reg)
            svc->uninit();
    }
    sc::ServiceRegistry& reg;
};

struct RestoreSigmaskGuard
{
    ~RestoreSigmaskGuard() { pthread_sigmask(SIG_SETMASK, restore, nullptr); }

    sigset_t* restore;
};

auto from_nanoseconds(std::uint64_t val) noexcept -> timespec
{
    return timespec { .tv_sec = static_cast<time_t>(val / kNsPerSec),
                      .tv_nsec = static_cast<time_t>(val % kNsPerSec) };
}

auto tick_timers(sc::ReadinessRegister::FrameTickRegisterType& timers,
                 std::size_t frame_time,
                 std::size_t max_frame_time) -> std::size_t
{
    auto next_frame_timeout = max_frame_time;

    for (auto& timer : timers) {
        if (frame_time >= timer.remaining) {
            timer.remaining =
                timer.frame_time -
                std::min(frame_time - timer.remaining, timer.frame_time);
            timer.dispatch(*timer.svc);
        }
        else {
            timer.remaining -= frame_time;
        }

        next_frame_timeout = std::min(next_frame_timeout, timer.remaining);
    }

    return next_frame_timeout;
}

} // namespace

namespace sc
{
Context::Context(FrameTime const& ft) noexcept
    : frame_time_ { ft.value() }
{
}

Context::Context(std::uint32_t fps) noexcept
    : frame_time_ { from_fps(fps).value() }
{
}

auto Context::services() noexcept -> ServiceRegistry& { return reg_; }

auto Context::request_stop() noexcept -> void
{
    stop_requested_ = true;
    std::uint64_t event { 1 };
    ::write(event_fd_, &event, sizeof(event));
}

auto Context::run() -> void
{
    using Notifications = ReadinessRegister::NotifyRegisterType;
    using Timers = ReadinessRegister::FrameTickRegisterType;

    if (event_fd_ = ::eventfd(0, EFD_NONBLOCK); event_fd_ < 0)
        throw std::system_error { errno, std::system_category() };

    SC_SCOPE_GUARD([&] { ::close(event_fd_); });

    Notifications notifications;
    Timers timers;

    ServiceRegistryLock registry_lock { reg_ };
    if (!registry_lock) [[unlikely]] {
        assert(false && "Couldn't obtain service registry lock");
    }

    auto initialized_pos = reg_.begin();

    try {
        for (; initialized_pos != reg_.end(); ++initialized_pos) {
            auto& svc = std::get<1>(*initialized_pos);
            svc->init(
                ReadinessRegister { *svc, notifications, timers, frame_time_ });
        }
    }
    catch (...) {
        std::for_each(reg_.begin(), initialized_pos, [](auto& item) {
            auto& svc = std::get<1>(item);
            svc->uninit();
        });

        throw;
    }

    UninitGuard uninit_guard { reg_ };

    if (notifications.size() + timers.size() == 0) {
        /* TODO: We should probably raise an error here...
         */
        return;
    }

    std::vector<pollfd> poll_events;
    poll_events.reserve(notifications.size() + 1);
    poll_events.push_back({ event_fd_, POLLIN, 0 });

    std::transform(begin(notifications),
                   end(notifications),
                   std::back_inserter(poll_events),
                   [](auto const& notification) {
                       return pollfd { std::get<0>(notification), POLLIN, 0 };
                   });

    Elapsed const& elapsed = global_elapsed;
    auto frame_start = elapsed.nanosecond_value();

    while (!stop_requested_) {
        auto const frame_stop = elapsed.nanosecond_value();
        auto const frame_time = frame_stop - frame_start;
        frame_start = frame_stop;

        auto const next_timeout = tick_timers(timers, frame_time, frame_time_);

        auto sleep_for = from_nanoseconds(next_timeout);
        auto const poll_result =
            ppoll(poll_events.data(), poll_events.size(), &sleep_for, nullptr);

        if (poll_result < 0) {
            if (errno != EINTR) {
                throw std::system_error { errno, std::system_category() };
            }
        }

        if (poll_result > 0) {
            for (auto const& ev : poll_events) {

                if (!(ev.revents & POLLIN))
                    continue;

                if (ev.fd == event_fd_) {
                    std::uint64_t discard;
                    if (auto const result =
                            ::read(event_fd_, &discard, sizeof(discard));
                        result < 0)
                        if (errno != EAGAIN)
                            throw std::system_error { errno,
                                                      std::system_category() };

                    continue;
                }

                auto notify = notifications.find(ev.fd);
                if (notify != notifications.end()) {
                    std::get<1>(*notify).dispatch(*std::get<1>(*notify).svc);
                }
            }
        }
    }

    stop_requested_ = false;
}
} // namespace sc
