#include "testing.hpp"
#include "utils/cmd_line.hpp"

auto should_parse() -> void
{
    char const* argv[] = {
        "-f30", "-A", "libopus", "/tmp/test.mp4", "-h", "-v"
    };

    auto const cmdline = sc::parse_cmd_line(std::size(argv), argv);

    EXPECT(cmdline.args().size() == 1);

    EXPECT(cmdline.has_option(sc::CmdLineOption::frame_rate));
    EXPECT(cmdline.get_option_value(sc::CmdLineOption::frame_rate,
                                    sc::number_value) == 30);

    EXPECT(!cmdline.has_option(sc::CmdLineOption::video_encoder));
    EXPECT(cmdline.get_option_value_or_default(sc::CmdLineOption::video_encoder,
                                               "hevc_nvenc") == "hevc_nvenc");

    EXPECT(cmdline.has_option(sc::CmdLineOption::audio_encoder));
    EXPECT(cmdline.get_option_value(sc::CmdLineOption::audio_encoder) ==
           "libopus");

    EXPECT(cmdline.has_option(sc::CmdLineOption::help));

    EXPECT(cmdline.has_option(sc::CmdLineOption::version));

    EXPECT(cmdline.args()[0] == "/tmp/test.mp4");
}

auto should_fail_number_range() -> void
{
    char const* argv[] = { "-f-1", "/tmp/test.mp4" };

    EXPECT_THROWS(sc::parse_cmd_line(std::size(argv), argv));
}

auto main() -> int
{
    return testing::run({ TEST(should_parse), TEST(should_fail_number_range) });
}
