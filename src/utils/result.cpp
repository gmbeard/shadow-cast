#include "utils/result.hpp"

namespace sc
{
auto result_ok() noexcept -> detail::VoidResult
{
    return detail::VoidResult {};
}
} // namespace sc
