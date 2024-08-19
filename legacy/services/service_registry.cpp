#include "./service_registry.hpp"

namespace sc
{
namespace detail
{
auto service_id_impl() noexcept -> std::size_t
{
    static std::size_t id = 1;
    return id++;
}
} // namespace detail
  //
auto ServiceRegistry::init_all(ReadinessRegister reg) -> void
{
    for (auto& [id, svc] : map_) {
        svc->init(reg);
    }
}

auto ServiceRegistry::uninit_all() -> void
{
    for (auto& [id, svc] : map_) {
        svc->uninit();
    }
}

auto ServiceRegistry::begin() noexcept -> decltype(map_.begin())
{
    return map_.begin();
}

auto ServiceRegistry::end() noexcept -> decltype(map_.end())
{
    return map_.end();
}

auto ServiceRegistry::begin() const noexcept -> decltype(map_.begin())
{
    return map_.begin();
}

auto ServiceRegistry::end() const noexcept -> decltype(map_.end())
{
    return map_.end();
}

auto ServiceRegistry::lock() noexcept -> bool { return lock_.test_and_set(); }

auto ServiceRegistry::unlock() noexcept -> void { return lock_.clear(); }

ServiceRegistryLock::ServiceRegistryLock(ServiceRegistry& reg) noexcept
    : reg_ { reg }
    , obtained_lock_ { !reg_.lock() }
{
}

ServiceRegistryLock::~ServiceRegistryLock()
{
    if (obtained_lock_) {
        reg_.unlock();
    }
}

ServiceRegistryLock::operator bool() const noexcept { return obtained_lock_; }
} // namespace sc
