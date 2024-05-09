#include "utils/cmd_line.hpp"
#include "config.hpp"
#include "error.hpp"
#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <span>
#include <stdexcept>
#include <tuple>

using namespace std::literals::string_literals;

namespace
{

constexpr char const kStrictFrameTimeEnvVar[] = "SHADOW_CAST_STRICT_FPS";

template <typename Container, typename... T>
constexpr auto construct(T... vals) noexcept -> Container
requires(sizeof...(T) <= std::tuple_size<Container>::value) &&
        ((std::is_convertible_v<T, std::string_view>) && ...)
{
    return { std::string_view { vals }... };
}

auto get_resolution(std::string_view param, sc::CaptureResolution& res) noexcept
    -> bool
{
    std::uint32_t w { 0 }, h;
    auto pos = std::find(param.begin(), param.end(), 'x');
    if (pos != param.end()) {
        if (auto result = std::from_chars(param.data(), &*pos++, w);
            result.ec != std::errc {})
            return false;
    }
    else {
        pos = param.begin();
    }

    if (auto result = std::from_chars(&*pos, param.data() + param.size(), h);
        result.ec != std::errc {})
        return false;

    if (w == 0) {
        auto tmp = static_cast<float>(h) * (16.0f / 9.0f);
        w = static_cast<decltype(w)>(tmp);
    }

    res = sc::CaptureResolution { w, h };

    return true;
}

auto quality_string_value_to_enum_value(std::string_view val) noexcept
    -> sc::CaptureQuality
{
    if (val == "low")
        return sc::CaptureQuality::low;

    if (val == "medium")
        return sc::CaptureQuality::medium;

    return sc::CaptureQuality::high;
}

template <typename T>
auto safe_multiply(T& source, T mult) noexcept -> bool
{
    if ((std::numeric_limits<T>::max() / mult) < source)
        return false;

    source *= mult;
    return true;
}

auto get_bitrate(std::string_view val, std::size_t& bitrate) noexcept -> bool
{
    static_cast<void>(val);
    static_cast<void>(bitrate);

    auto numbers_end = std::find_if(val.begin(), val.end(), [](auto const& c) {
        return c < '0' || c > '9';
    });

    if (auto result = std::from_chars(
            val.data(), val.data() + (numbers_end - val.begin()), bitrate);
        result.ec != std::errc {})
        return false;

    if (numbers_end != val.end()) {
        char const unit = *numbers_end;
        switch (unit) {
        case 'm':
        case 'M':
            safe_multiply(bitrate, 1'000'000lu);
            break;
        case 'k':
        case 'K':
            safe_multiply(bitrate, 1'000lu);
            break;
        default:
            return false;
        }

        if (std::next(numbers_end) != val.end())
            return false;
    }

    return true;
}

sc::CmdLineOptionSpec const cmd_line_spec[] = {
    /* Audio encoder...
     */
    { .short_name = 'A',
      .long_name = "--audio-encoder",
      .option = sc::CmdLineOption::audio_encoder,
      .flags = sc::cmdline::VALUE_REQUIRED,
      .validation = sc::no_validation,
      .description = "The audio encoder to use. Default 'libopus'" },

    /* Bit rate...
     */
    { .short_name = 'b',
      .long_name = "--bitrate",
      .option = sc::CmdLineOption::bit_rate,
      .flags = sc::cmdline::VALUE_REQUIRED,
      .validation =
          [](std::string_view val) {
              std::size_t bitrate;
              return get_bitrate(val, bitrate);
          },
      .description = "The video bitrate. This implies constant rate mode." },

    /* Frame rate...
     */
    { .short_name = 'f',
      .long_name = "--framerate",
      .option = sc::CmdLineOption::frame_rate,
      .flags = sc::cmdline::VALUE_NUMERIC,
      .validation = sc::no_validation,
      .description =
          "Video frame rate in frames-per-second. Must be between 20 - 70. "
          "Default 60" },

    /* Show help...
     */
    {
        .short_name = 'h',
        .long_name = "--help",
        .option = sc::CmdLineOption::help,
        .flags = 0,
        .validation = sc::no_validation,
        .description = "Show usage",
    },

    /* Show help...
     */
    {
        .short_name = 'q',
        .long_name = "--quality",
        .option = sc::CmdLineOption::quality,
        .flags = sc::cmdline::VALUE_REQUIRED,
        .validation = construct<sc::AcceptableValues>("low", "medium", "high"),
        .description =
            "Capture quality. Accepted values are 'low', 'medium', or 'high'. "
            "Default 'high'. This implies constant quality mode.",
    },

    /* Capture resolution...
     */
    { .short_name = 'r',
      .long_name = "--resolution",
      .option = sc::CmdLineOption::resolution,
      .flags = sc::cmdline::VALUE_REQUIRED,
      .validation =
          [](std::string_view param) {
              sc::CaptureResolution res;
              return get_resolution(param, res);
          },
      .description = "Capture resolution in the format [<WIDTH>x]<HEIGHT>. "
                     "E.g. 1920x1080. If ony HEIGHT is given then WIDTH will "
                     "be calculated based on a 16:9 aspect ratio" },

    /* Sample rate...
     */
    {
        .short_name = 's',
        .long_name = "--sample-rate",
        .option = sc::CmdLineOption::sample_rate,
        .flags = sc::cmdline::VALUE_REQUIRED | sc::cmdline::VALUE_NUMERIC,
        .validation = sc::ValidRange { 8'000, 48'000 },
        .description =
            "Audio sample rate. Must be between 8000 - 48000. Default 48000",
    },

    /* Show version..
     */
    {
        .short_name = 'v',
        .long_name = "--version",
        .option = sc::CmdLineOption::version,
        .flags = 0,
        .validation = sc::no_validation,
        .description = "Print version",
    },

    /* Video encoder...
     */
    {
        .short_name = 'V',
        .long_name = "--video-encoder",
        .option = sc::CmdLineOption::video_encoder,
        .flags = sc::cmdline::VALUE_REQUIRED,
        .validation =
            construct<sc::AcceptableValues>("h264_nvenc", "hevc_nvenc"),
        .description = "Video encoder to use. Valid values are 'h264_nvenc', "
                       "'hevc_nvenc'. Default 'hevc_nvenc'",
    },
};

auto parse_long_option(std::string_view key, auto first, auto /*last*/)
    -> std::pair<sc::CmdLineOptionValue, decltype(first)>
{
    /* TODO:
     */
    throw std::runtime_error { "Unknown option: "s + std::string { key } };
}

template <typename T>
struct Validator
{
    auto operator()(sc::AcceptableValues const& acceptable) const -> void
    {
        auto const pos = std::find(acceptable.begin(), acceptable.end(), val);

        if (pos == acceptable.end())
            throw std::runtime_error { "Invalid option value: "s +
                                       std::string { val } };
    }

    auto operator()(sc::ValidRange const& range) const -> void
    {
        std::int32_t result {};
        std::from_chars_result r;

        if constexpr (std::is_convertible_v<T, char const*>) {
            r = std::from_chars(val, val + std::strlen(val), result);
        }
        else {
            r = std::from_chars(val.data(), val.data() + val.size(), result);
        }

        if (r.ec != std::errc {} || result < std::get<0>(range) ||
            result > std::get<1>(range))
            throw std::runtime_error {
                "Option value not in acceptable range: "s + std::string { val }
            };
    }

    auto operator()(auto (*f)(std::string_view)->bool) const -> void
    {

        bool result;
        if constexpr (std::is_convertible_v<T, char const*>) {
            result = f(std::string_view { val, std::strlen(val) });
        }
        else {
            result = f(val);
        }

        if (!result) {
            throw std::runtime_error { "Option value not valid: "s +
                                       std::string { val } };
        }
    }

    auto operator()(sc::NoValidation) const noexcept -> void
    {
        /* No-op
         */
    }

    T const& val;
};

template <typename T>
Validator(T const&) -> Validator<T>;

template <typename T>
auto check_valid_value(T const& val, sc::CmdLineOptionSpec const& spec)
    -> std::string
{
    if (spec.flags & sc::cmdline::VALUE_NUMERIC) {
        std::int32_t result {};
        std::from_chars_result r;

        if constexpr (std::is_convertible_v<T, char const*>) {
            r = std::from_chars(val, val + std::strlen(val), result);
        }
        else {
            r = std::from_chars(val.data(), val.data() + val.size(), result);
        }

        if (r.ec != std::errc {})
            throw std::runtime_error { "Invalid numeric value: "s +
                                       std::string { val } };
    }

    std::visit(Validator { val }, spec.validation);

    return std::string { val };
}

auto parse_short_option(std::string_view key, auto first, auto last)
    -> std::pair<sc::CmdLineOptionValue, decltype(first)>
{
    auto spec_pos = std::find_if(
        std::begin(cmd_line_spec),
        std::end(cmd_line_spec),
        [&](auto const& spec) { return spec.short_name == key[0]; });

    auto const saved_key = key[0];

    if (spec_pos == std::end(cmd_line_spec))
        throw std::runtime_error { "Unknown option: -"s + saved_key };

    if (!spec_pos->flags)
        return { { spec_pos->option, spec_pos->flags, {} }, first };

    key = key.substr(1);

    if (key.size() && ((spec_pos->flags & sc::cmdline::VALUE_OPTIONAL) ||
                       (spec_pos->flags & sc::cmdline::VALUE_REQUIRED)))
        return { { spec_pos->option,
                   spec_pos->flags,
                   check_valid_value(key, *spec_pos) },
                 first };

    if ((spec_pos->flags & sc::cmdline::VALUE_REQUIRED) && first == last)
        throw std::runtime_error { "Missing argument: -"s + saved_key };

    return { { spec_pos->option,
               spec_pos->flags,
               check_valid_value(*first, *spec_pos) },
             first == last ? first : std::next(first) };
}

auto parse_option(auto first, auto last)
    -> std::pair<sc::CmdLineOptionValue, decltype(first)>
{
    auto it = first;
    auto key = std::string_view { *it++ };
    assert(key.size());

    auto leading_dashes = 0;
    while (key.size() && key[0] == '-') {
        key = key.substr(1);
        leading_dashes++;
    }

    switch (leading_dashes) {
    case 1:
        return parse_short_option(key, it, last);
    default:
        return parse_long_option(key, it, last);
    }
}

struct Wrapped
{
    std::string_view data;
    std::size_t cols;
    std::size_t indent { 0 };
};

template <typename Out>
auto operator<<(Out& out, Wrapped const& wrapped) -> Out&
{
    auto data = wrapped.data;

    bool is_first_line = true;
    while (data.size() > (wrapped.cols + wrapped.indent)) {
        auto const line = data.substr(0, wrapped.cols);
        data = data.substr(wrapped.cols);
        if (!is_first_line)
            out << std::string(wrapped.indent, ' ');
        out << line << '\n';
        is_first_line = false;
    }

    if (!is_first_line)
        out << std::string(wrapped.indent, ' ');
    return out << data;
}

} // namespace

namespace sc
{
auto CmdLine::args() const noexcept -> decltype(args_) const& { return args_; }

auto CmdLine::has_option(CmdLineOption opt) const noexcept -> bool
{
    return options_.end() !=
           std::find_if(options_.begin(),
                        options_.end(),
                        [&](auto const& item) { return item.option == opt; });
}

auto CmdLine::get_option_value_dispatch(CmdLineOption opt, StringValue) const
    -> std::string_view
{
    auto const pos =
        std::find_if(options_.begin(), options_.end(), [&](auto const& item) {
            return item.option == opt;
        });

    if (pos == options_.end())
        throw std::runtime_error { "Option doesn't exist" };

    return pos->value;
}

auto CmdLine::get_option_value_dispatch(CmdLineOption opt, NumberValue) const
    -> std::int32_t
{
    auto const pos =
        std::find_if(options_.begin(), options_.end(), [&](auto const& item) {
            return item.option == opt;
        });

    if (pos == options_.end())
        throw std::runtime_error { "Option doesn't exist" };

    if ((pos->flags & cmdline::VALUE_NUMERIC) == 0)
        throw std::runtime_error { "Option doesn't hold a numeric value" };

    std::int32_t val;
    [[maybe_unused]] auto const r = std::from_chars(
        pos->value.data(), pos->value.data() + pos->value.size(), val);
    assert(r.ec == std::errc {});
    return val;
}

auto CmdLine::get_option_value(CmdLineOption opt) const noexcept
    -> std::string_view
{
    return get_option_value_dispatch(opt, string_value);
}

auto parse_cmd_line(int argc, char const** argv) -> CmdLine
{
    CmdLine cmdline;
    std::span<char const*> args { argv, static_cast<std::size_t>(argc) };
    if (!args.size())
        return cmdline;

    for (auto it = args.begin(); it != args.end();) {
        auto const* c = *it;
        if (!c[0])
            break;

        if (c[0] == '-' && c[1]) {
            CmdLineOptionValue val;
            std::tie(val, it) = parse_option(it, args.end());
            cmdline.options_.push_back(val);
        }
        else {
            cmdline.args_.push_back(*it++);
        }
    }

    return cmdline;
}

auto read_env(Parameters& params) -> void
{
    if (auto const* strict_frame_time = std::getenv(kStrictFrameTimeEnvVar);
        strict_frame_time) {
        params.strict_frame_time = std::strcmp(strict_frame_time, "1") == 0;
    }
}

auto get_parameters(CmdLine const& cmdline) noexcept
    -> Result<Parameters, CmdLineError>
{
    if (cmdline.has_option(CmdLineOption::help))
        return CmdLineError { CmdLineError::show_help };

    if (cmdline.has_option(CmdLineOption::version))
        return CmdLineError { CmdLineError::show_version };

    Parameters params {
        .video_encoder = std::string { cmdline.get_option_value_or_default(
            sc::CmdLineOption::video_encoder, "hevc_nvenc") },
        .audio_encoder = std::string { cmdline.get_option_value_or_default(
            sc::CmdLineOption::audio_encoder, "libopus") },
        .frame_time = from_fps(cmdline.get_option_value_or_default(
            sc::CmdLineOption::frame_rate, 60, sc::number_value)),
        .sample_rate = cmdline.get_option_value_or_default(
            sc::CmdLineOption::sample_rate, 48'000, sc::number_value),
        .output_file = cmdline.args().size() ? cmdline.args()[0] : "",
        .quality = quality_string_value_to_enum_value(
            cmdline.get_option_value_or_default(sc::CmdLineOption::quality,
                                                "high")),
    };

    if (cmdline.has_option(CmdLineOption::resolution)) {
        CaptureResolution res;
        if (!get_resolution(cmdline.get_option_value(CmdLineOption::resolution),
                            res))
            return CmdLineError { CmdLineError::error,
                                  "Invalid parameter: resolution" };
        params.resolution = res;
    }

    if (cmdline.has_option(CmdLineOption::bit_rate)) {
        if (cmdline.has_option(CmdLineOption::quality))
            return CmdLineError {
                CmdLineError::error,
                "You cannot specify both quality and bitrate together"
            };

        std::size_t bitrate;
        if (!get_bitrate(cmdline.get_option_value(CmdLineOption::bit_rate),
                         bitrate))
            return CmdLineError { CmdLineError::error,
                                  "Invalid parameter: bitrate" };

        params.bitrate = bitrate;
    }

    if (!params.output_file.size())
        return CmdLineError { CmdLineError::error,
                              "Missing parameter: output file" };

    read_env(params);
    return params;
}

auto output_help() -> void
{
    std::cerr << "USAGE: " << kShadowCastProg
              << " [ OPTIONS... ] <OUTPUT_FILE>\n";
    std::cerr << "OPTIONS:\n";
    for (auto const& spec : cmd_line_spec) {
        std::cerr << "  -" << spec.short_name;
        if (spec.flags & cmdline::VALUE_REQUIRED)
            std::cerr << " <VALUE>";
        else if (spec.flags & cmdline::VALUE_OPTIONAL)
            std::cerr << " [ <VALUE> ]";

        std::cerr << "\n    " << Wrapped { spec.description, 60, 4 } << "\n\n";
    }
    std::cerr << '\n';
}

auto output_version() -> void { std::cerr << kShadowCastVersion << '\n'; }

} // namespace sc
