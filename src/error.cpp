#include "./error.hpp"

namespace sc
{
ModuleError::ModuleError(std::string const& msg)
    : std::runtime_error { msg }
{
}

CodecError::CodecError(std::string const& msg)
    : std::runtime_error { msg }
{
}

FormatError::FormatError(std::string const& msg)
    : std::runtime_error { msg }
{
}

IOError::IOError(std::string const& msg)
    : std::runtime_error { msg }
{
}

auto av_error_to_string(int err) -> std::string
{
    std::string error_string(AV_ERROR_MAX_STRING_SIZE, '\0');
    if (av_strerror(err, error_string.data(), error_string.size()) < 0)
        error_string = "Uknown error";
    return error_string;
}
} // namespace sc
