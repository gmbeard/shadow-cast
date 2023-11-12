#ifndef SHADOW_CAST_SERVICES_SIGNAL_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_SIGNAL_SERVICE_HPP_INCLUDED

#include "services/service.hpp"
#include "utils/receiver.hpp"
#include <cinttypes>
#include <initializer_list>
#include <signal.h>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace sc
{

namespace detail
{
auto block_signal(std::uint32_t, int) -> void;
}

struct SignalService final : Service
{
    friend auto dispatch_signal(Service&) -> void;

    using SignalHandler = Receiver<void(std::uint32_t)>;

    SignalService();
    ~SignalService();

    SignalService(SignalService&&) noexcept;
    auto operator=(SignalService&&) noexcept = delete;

    SignalService(SignalService const&) = delete;
    auto operator=(SignalService const&) -> SignalService& = delete;

    template <typename T>
    auto add_signal_handler(std::uint32_t sig, T&& handler) -> void
    {
        detail::block_signal(sig, notify_fd_);
        handlers_.emplace(sig, SignalHandler { std::forward<T>(handler) });
    }

protected:
    auto on_init(ReadinessRegister) -> void override;
    auto on_uninit() noexcept -> void override;

private:
    sigset_t original_mask_;
    int notify_fd_;
    std::unordered_map<std::uint32_t, SignalHandler> handlers_;
};

auto dispatch_signal(Service&) -> void;
} // namespace sc

#endif // SHADOW_CAST_SERVICES_SIGNAL_SERVICE_HPP_INCLUDEDq
