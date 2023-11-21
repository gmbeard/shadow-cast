#include "utils/contracts.hpp"
#include <cstdio>
#include <exception>

namespace sc
{
auto contract_check_failed(char const* msg, std::source_location loc) -> void
{
    std::printf("Contract check failed: %s - %s:%u\n",
                msg,
                loc.file_name(),
                loc.line());
    std::terminate();
}
} // namespace sc
