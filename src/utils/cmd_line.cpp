#include "utils/cmd_line.hpp"
#include <span>

namespace sc
{
auto parse_cmd_line(int argc, char** argv) -> std::vector<std::string>
{
    std::span<char*> args { argv, static_cast<std::size_t>(argc) };
    std::vector<std::string> ret;
    if (!args.size())
        return ret;

    std::copy(next(args.begin()), args.end(), std::back_inserter(ret));
    return ret;
}
} // namespace sc
