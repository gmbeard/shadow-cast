#include "services/signal_service.hpp"
#include <array>
#include <atomic>
#include <errno.h>
#include <sys/signalfd.h>
#include <system_error>
#include <unistd.h>

namespace sc
{

SignalService::SignalService()
    : notify_fd_ { [] {
        sigset_t empty;
        sigemptyset(&empty);
        return signalfd(-1, &empty, SFD_NONBLOCK);
    }() }
{
    if (notify_fd_ < 0)
        throw std::system_error { errno, std::system_category() };

    if (auto const result =
            pthread_sigmask(SIG_SETMASK, nullptr, &original_mask_);
        result < 0)
        throw std::system_error { errno, std::system_category() };
}

SignalService::SignalService(SignalService&& other) noexcept
    : original_mask_ { other.original_mask_ }
    , notify_fd_ { std::exchange(other.notify_fd_, -1) }
    , handlers_ { std::move(other.handlers_) }
{
}

SignalService::~SignalService() { close(notify_fd_); }

auto SignalService::on_init(ReadinessRegister reg) -> void
{
    reg(notify_fd_, &dispatch_signal);
}

auto SignalService::on_uninit() -> void
{
    pthread_sigmask(SIG_SETMASK, &original_mask_, nullptr);
}

auto dispatch_signal(Service& svc) -> void
{
    auto& self = static_cast<SignalService&>(svc);

    std::array<signalfd_siginfo, 1> buffer {};
    auto const bytes = buffer.size() * sizeof(buffer[0]);

    while (true) {
        auto const read_result = read(self.notify_fd_, buffer.data(), bytes);
        if (read_result < 0) {
            if (errno == EAGAIN) {
                break;
            }
            throw std::system_error { errno, std::system_category() };
        }

        auto const items = read_result / sizeof(buffer[0]);

        for (auto i = 0ul; i < items; ++i) {
            auto signo = buffer[i].ssi_signo;
            auto handler_pos = self.handlers_.find(signo);
            if (handler_pos == self.handlers_.end())
                continue;

            (handler_pos->second)(signo);
        }

        if (items < buffer.size())
            break;
    }
}

namespace detail
{
auto block_signal(std::uint32_t sig, int notify_fd) -> void
{
    sigset_t working_set;
    if (auto const result = pthread_sigmask(SIG_SETMASK, nullptr, &working_set);
        result < 0)
        throw std::system_error { errno, std::system_category() };

    if (auto const result = sigaddset(&working_set, sig); result < 0)
        throw std::system_error { errno, std::system_category() };

    if (auto const result = pthread_sigmask(SIG_SETMASK, &working_set, nullptr);
        result < 0)
        throw std::system_error { errno, std::system_category() };

    if (auto const result = signalfd(notify_fd, &working_set, SFD_NONBLOCK);
        result < 0)
        throw std::system_error { errno, std::system_category() };
}
} // namespace detail

} // namespace sc
