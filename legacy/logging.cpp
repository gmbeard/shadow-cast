#include "./logging.hpp"
#include <algorithm>
#include <iostream>

namespace
{

auto level_to_string(sc::log::Level level) -> char const*
{
    using sc::log::Level;

    switch (level) {
    case Level::info:
        return "INFO";
    case Level::warning:
        return "WARN";
    case Level::error:
        return "ERROR";
    default:
        return "DEBUG";
    }
}

auto format_level(sc::log::Level level) -> std::string
{
    auto constexpr kMaxLevelStringSize = 5;
    std::string output(kMaxLevelStringSize + 3, ' ');
    output.front() = '[';
    *std::next(output.end(), -2) = ']';

    std::string_view level_as_string = level_to_string(level);
    auto out_pos = std::next(output.begin());

    std::copy_n(begin(level_as_string), kMaxLevelStringSize, out_pos);
    return output;
}

} // namespace

namespace sc::log
{

auto write(Level level, std::string const& msg) -> void
{
    write(level, std::string_view { msg.data(), msg.size() });
}

auto write(Level level, std::string_view msg) -> void
{
    auto const level_string = format_level(level);
    std::copy(begin(level_string),
              end(level_string),
              std::ostreambuf_iterator<char>(std::cerr));
    std::copy(begin(msg), end(msg), std::ostreambuf_iterator<char>(std::cerr));
    std::cerr.write("\n", 1);
}

} // namespace sc::log
