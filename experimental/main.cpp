#include "./cuda.hpp"
#include "./media_container.hpp"
#include "./nvfbc.hpp"
#include "./nvfbc_capture.hpp"
#include "./nvfbc_capture_source.hpp"
#include "./video_capture.hpp"
#include "./x11_desktop.hpp"
#include "any_video_capture.hpp"
#include "capture_source.hpp"
#include "io/signals.hpp"
#include "nvenc_encoder_sink.hpp"
#include "nvfbc_gpu.hpp"
#include "utils/cmd_line.hpp"
#include <iostream>
#include <signal.h>

auto app(sc::Parameters params) -> void
{
    exios::ContextThread execution_context;
    exios::Signal signal { execution_context, SIGINT };
    sc::X11Desktop desktop;
    sc::MediaContainer container { params.output_file };

    // FIX: If we create the sink and source in the opposite order then NVFBC
    // complains about the CUDA context. Does this mean that initializing NVENC
    // (indirectly via libav) somehow does the CUDA initialization work for us?
    sc::NvencEncoderSink sink {
        execution_context, container, params, desktop.size()
    };
    sc::NvfbcCaptureSource source { execution_context, params, desktop.size() };

    sc::AnyVideoCapture capture {
        execution_context, std::allocator<void> {}, std::move(sink),
        std::move(source), params.frame_time,
    };

    try {
        container.write_header();

        signal.wait([&](auto) { capture.cancel(); });

        capture.run(
            [&](exios::Result<std::error_code> result) {
                signal.cancel();
                std::cerr << "Writing trailer\n";

                if (!result && result.error() != std::errc::operation_canceled)
                    throw std::system_error { result.error() };

                container.write_trailer();
            },
            std::allocator<void> {});

        std::cerr << "Capture running. Ctrl+C to stop...\n";
        static_cast<void>(execution_context.run());
        std::cerr << "Finished\n";
    }
    catch (std::exception const& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
    }
}

auto main(int argc, char const** argv) -> int
{
    sc::block_signals({ SIGINT, SIGCHLD });
    auto params = sc::get_parameters(sc::parse_cmd_line(argc - 1, argv + 1));

    if (!params) {
        switch (params.error().type) {
        case sc::CmdLineError::show_help:
            sc::output_help();
            break;
        case sc::CmdLineError::show_version:
            sc::output_version();
            break;
        default:
            throw params.error();
        }
    }
    else {
        app(std::move(params.value()));
    }
}
