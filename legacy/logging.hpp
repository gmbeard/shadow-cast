#ifndef SHADOW_CAST_LOGGING_HPP_INCLUDED
#define SHADOW_CAST_LOGGING_HPP_INCLUDED

#include <string>
#include <string_view>
#include <utility>

namespace sc::log
{

template <typename T>
concept StringLike =
    std::is_convertible_v<std::remove_cvref_t<T>, std::string> ||
    std::is_convertible_v<std::remove_cvref_t<T>, std::string_view>;

enum struct Level
{
    debug,
    info,
    warning,
    error
};

auto write(Level, std::string const&) -> void;
auto write(Level, std::string_view) -> void;

template <StringLike Msg>
auto debug(Msg&& msg) -> void
{
    write(Level::debug, std::forward<Msg>(msg));
}

template <StringLike Msg>
auto info(Msg&& msg) -> void
{
    write(Level::info, std::forward<Msg>(msg));
}

template <StringLike Msg>
auto warn(Msg&& msg) -> void
{
    write(Level::warning, std::forward<Msg>(msg));
}

template <StringLike Msg>
auto error(Msg&& msg) -> void
{
    write(Level::error, std::forward<Msg>(msg));
}
} // namespace sc::log

#endif // SHADOW_CAST_LOGGING_HPP_INCLUDED
