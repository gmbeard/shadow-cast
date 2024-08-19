#ifndef SHADOW_CAST_CPU_USAGE_HPP_INCLUDED
#define SHADOW_CAST_CPU_USAGE_HPP_INCLUDED

#include <cstdint>

namespace sc
{
auto get_cpu_usage() noexcept -> std::uint64_t;
}

#endif // SHADOW_CAST_CPU_USAGE_HPP_INCLUDED
