#include "./cuda.hpp"
#include "./media_container.hpp"
#include "./nvfbc.hpp"
#include "./nvfbc_capture.hpp"
#include "./video_capture.hpp"
#include "./x11_desktop.hpp"
#include "io/signals.hpp"
#include "nvfbc_gpu.hpp"
#include <iostream>
#include <signal.h>

auto main() -> int
{
    sc::block_signals({ SIGINT, SIGCHLD });
    sc::FrameTime frame_time {
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::seconds(1))
            .count() /
        60
    };
    exios::ContextThread execution_context;
    exios::Signal signal { execution_context, SIGINT };
    sc::NvFbcGpu gpu { frame_time };
    sc::X11Desktop desktop;
    sc::MediaContainer container { "/tmp/test.mp4" };

    sc::VideoCapture capture {
        execution_context,
        std::move(desktop),
        std::move(gpu),
        "hevc_nvenc",
        sc::FrameTime { std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::seconds(1))
                            .count() /
                        60 }
    };

    container.add_stream(capture.codec());

    try {
        container.write_header();

        signal.wait([&](auto) { capture.cancel(); });

        capture.run(container, [&](exios::Result<std::error_code> result) {
            signal.cancel();
            std::cerr << "Writing trailer\n";

            if (!result && result.error() != std::errc::operation_canceled)
                throw std::system_error { result.error() };

            container.write_trailer();
        });

        static_cast<void>(execution_context.run());
        std::cerr << "Finished\n";
    }
    catch (std::exception const& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
    }

    return 0;
}
