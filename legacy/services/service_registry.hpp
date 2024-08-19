#ifndef SHADOW_CAST_SERVICE_REGISTRY_HPP_INCLUDED
#define SHADOW_CAST_SERVICE_REGISTRY_HPP_INCLUDED

#include "./service.hpp"
#include <atomic>
#include <cassert>
#include <memory>
#include <unordered_map>

namespace sc
{

namespace detail
{
auto service_id_impl() noexcept -> std::size_t;

template <typename T>
struct ServiceIdProxy
{
    static decltype(service_id_impl()) value;
};

template <typename T>
decltype(service_id_impl()) ServiceIdProxy<T>::value = service_id_impl();

} // namespace detail

template <typename T>
auto service_id() noexcept -> decltype(detail::service_id_impl())
requires(std::is_convertible_v<T*, Service*>)
{
    static auto id = detail::ServiceIdProxy<T>::value;
    return id;
}

using ServicePtr = std::unique_ptr<Service>;
using ServiceId = decltype(detail::service_id_impl());

template <typename T>
concept ServiceLike =
    std::is_convertible_v<std::remove_reference_t<T>*, Service const*>;

template <typename F, typename T>
concept ServiceFactory =
    std::is_constructible_v<std::unique_ptr<T>, decltype(std::declval<F>()())>;

struct ServiceRegistry;

struct ServiceRegistryLock
{
    explicit ServiceRegistryLock(ServiceRegistry&) noexcept;
    ~ServiceRegistryLock();
    operator bool() const noexcept;

private:
    ServiceRegistry& reg_;
    bool obtained_lock_;
};

struct ServiceRegistry
{
    friend struct ServiceRegistryLock;

    template <ServiceLike T, ServiceFactory<T> F>
    auto add_from_factory(F&& f)
    {
        ServiceRegistryLock lock { *this };
        if (!lock) [[unlikely]] {
            assert(false && "Couldn't obtain service registry lock");
        }
        auto id = service_id<T>();
        map_.emplace(id, f());
    }

    template <ServiceLike T>
    auto add(T&& svc) -> void
    {
        ServiceRegistryLock lock { *this };
        if (!lock) [[unlikely]] {
            assert(false && "Couldn't obtain service registry lock");
        }
        auto id = service_id<T>();
        map_.emplace(id, std::make_unique<T>(std::move(svc)));
    }

    // clang-format off
    template <ServiceLike T>
    auto use() -> T&
    requires (std::is_default_constructible_v<T>)
    {
        if (auto svc_pos = map_.find(service_id<T>()); svc_pos != map_.end())
            return static_cast<T&>(*svc_pos->second.get());

        add<T>(T {});
        return use<T>();
    }
    // clang-format on

    // clang-format off
    template <ServiceLike T>
    auto use_if() -> T*
    {
        if (auto svc_pos = map_.find(service_id<T>()); svc_pos != map_.end())
            return static_cast<T*>(svc_pos->second.get());

        return nullptr;
    }
    // clang-format on

    auto init_all(ReadinessRegister reg) -> void;
    auto uninit_all() -> void;

private:
    std::unordered_map<ServiceId, ServicePtr> map_;
    std::atomic_flag lock_ { false };

private:
    auto lock() noexcept -> bool;
    auto unlock() noexcept -> void;

public:
    auto begin() noexcept -> decltype(map_.begin());
    auto end() noexcept -> decltype(map_.end());
    auto begin() const noexcept -> decltype(map_.begin());
    auto end() const noexcept -> decltype(map_.end());
};

} // namespace sc

#endif // SHADOW_CAST_SERVICE_REGISTRY_HPP_INCLUDED
