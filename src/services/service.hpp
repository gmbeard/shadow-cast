#ifndef SHADOW_CAST_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_SERVICE_HPP_INCLUDED

#include "services/readiness.hpp"
#include <cstddef>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace sc
{

struct Service;

struct Service
{
    virtual ~Service();
    auto init(ReadinessRegister) -> void;
    auto uninit() -> void;

protected:
    virtual auto on_init(ReadinessRegister) -> void = 0;
    virtual auto on_uninit() -> void;
};

} // namespace sc

#endif // SHADOW_CAST_SERVICE_HPP_INCLUDED
