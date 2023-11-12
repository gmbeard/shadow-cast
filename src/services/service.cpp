#include "services/service.hpp"

namespace sc
{

Service::~Service() {}

auto Service::init(ReadinessRegister register_readiness) -> void
{
    on_init(register_readiness);
}

auto Service::uninit() noexcept -> void { on_uninit(); }

auto Service::on_uninit() noexcept -> void {}

} // namespace sc
