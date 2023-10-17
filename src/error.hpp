#ifndef SHADOW_CAST_ERROR_HPP_INCLUDED
#define SHADOW_CAST_ERROR_HPP_INCLUDED

#include "av/fwd.hpp"
#include <stdexcept>
#include <string>

namespace sc
{
struct ModuleError final : std::runtime_error
{
    ModuleError(std::string const&);
};

struct CodecError final : std::runtime_error
{
    CodecError(std::string const& msg);
};

struct FormatError final : std::runtime_error
{
    FormatError(std::string const& msg);
};

struct IOError final : std::runtime_error
{
    IOError(std::string const& msg);
};

auto av_error_to_string(int err) -> std::string;

} // namespace sc

#endif // SHADOW_CAST_ERROR_HPP_INCLUDED
