#ifndef SHADOW_CAST_UTILS_CMD_LINE_HPP_INCLUDED
#define SHADOW_CAST_UTILS_CMD_LINE_HPP_INCLUDED

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace sc
{
auto parse_cmd_line(int argc, char** argv) -> std::vector<std::string>;
} // namespace sc
#endif // SHADOW_CAST_UTILS_CMD_LINE_HPP_INCLUDED
