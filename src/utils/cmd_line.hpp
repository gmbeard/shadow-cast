#ifndef SHADOW_CAST_UTILS_CMD_LINE_HPP_INCLUDED
#define SHADOW_CAST_UTILS_CMD_LINE_HPP_INCLUDED

#include "error.hpp"
#include "utils/frame_time.hpp"
#include "utils/result.hpp"
#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace sc
{

namespace cmdline
{
enum ValueFlags : std::uint32_t
{
    VALUE_NONE = 0,
    VALUE_REQUIRED = 1,
    VALUE_OPTIONAL = 2,

    VALUE_NUMERIC = 4,
};
}

enum class CmdLineOption
{
    audio_encoder,
    frame_rate,
    help,
    video_encoder,
    version,
    sample_rate,
};

struct Parameters
{
    std::string video_encoder;
    std::string audio_encoder;
    FrameTime frame_time;
    std::int32_t sample_rate;
    std::string output_file;
    bool strict_frame_time { true };
};

struct NoValidation
{
};
[[maybe_unused]] NoValidation constexpr no_validation {};

using AcceptableValues = std::array<std::string_view, 16>;
using ValidRange = std::tuple<std::int32_t, std::int32_t>;
using Validation = std::variant<AcceptableValues, ValidRange, NoValidation>;

enum class CmdLineOptionArgument
{
    none,
    optional,
    required,
};

struct CmdLineOptionSpec
{
    char short_name;
    std::string_view long_name;
    CmdLineOption option;
    std::uint32_t flags;
    Validation validation {};
    std::string_view description;
};

struct CmdLineOptionValue
{
    CmdLineOption option;
    std::uint32_t flags;
    std::string value;

    auto is_numeric_value() const noexcept -> bool;
    auto numeric_value() const -> std::int32_t;
    auto string_value() const noexcept -> std::string const&;
};

struct StringValue
{
};
struct NumberValue
{
};

[[maybe_unused]] StringValue constexpr string_value {};
[[maybe_unused]] NumberValue constexpr number_value {};

template <typename T>
concept OptionDataType =
    std::is_same_v<T, StringValue> || std::is_same_v<T, NumberValue>;

struct CmdLine
{
    friend auto parse_cmd_line(int argc, char const** argv) -> CmdLine;

private:
    std::vector<CmdLineOptionValue> options_;
    std::vector<std::string> args_;

    auto get_option_value_dispatch(CmdLineOption, StringValue) const
        -> std::string_view;
    auto get_option_value_dispatch(CmdLineOption, NumberValue) const
        -> std::int32_t;

public:
    auto args() const noexcept -> decltype(args_) const&;
    auto has_option(CmdLineOption) const noexcept -> bool;

    auto get_option_value(CmdLineOption opt) const noexcept -> std::string_view;

    template <OptionDataType T = StringValue>
    decltype(auto) get_option_value(CmdLineOption opt,
                                    T dt = string_value) const
        noexcept(std::is_same_v<T, StringValue>)
    {
        return get_option_value_dispatch(opt, dt);
    }

    template <typename Default, OptionDataType T = StringValue>
    auto get_option_value_or_default(CmdLineOption opt,
                                     Default const& dflt,
                                     T dt = string_value) const
    requires((std::is_same_v<T, StringValue> &&
              std::is_convertible_v<Default, std::string_view>) ||
             (std::is_same_v<T, NumberValue> &&
              std::is_convertible_v<Default, std::int32_t>))
    {
        if (!has_option(opt)) {
            if constexpr (std::is_same_v<T, StringValue>) {
                return std::string_view { dflt };
            }
            else {
                return dflt;
            }
        }

        return get_option_value_dispatch(opt, dt);
    }
};

[[nodiscard]] auto parse_cmd_line(int argc, char const** argv) -> CmdLine;
[[nodiscard]] auto get_parameters(CmdLine const& cmdline) noexcept
    -> Result<Parameters, CmdLineError>;

auto output_help() -> void;
auto output_version() -> void;
} // namespace sc
#endif // SHADOW_CAST_UTILS_CMD_LINE_HPP_INCLUDED
