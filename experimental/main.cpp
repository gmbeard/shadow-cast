#include "./audio_encoder_sink.hpp"
#include "./cuda.hpp"
#include "./media_container.hpp"
#include "./nvfbc.hpp"
#include "./nvfbc_capture.hpp"
#include "./nvfbc_capture_source.hpp"
#include "./pipewire_capture_source.hpp"
#include "./video_capture.hpp"
#include "./x11_desktop.hpp"
#include "any_video_capture.hpp"
#include "audio_capture.hpp"
#include "capture_source.hpp"
#include "io/signals.hpp"
#include "nvenc_encoder_sink.hpp"
#include "nvfbc_gpu.hpp"
#include "utils/cmd_line.hpp"
#include <iostream>
#include <signal.h>
#include <thread>

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

    sc::AnyVideoCapture video_capture {
        execution_context, std::allocator<void> {}, std::move(sink),
        std::move(source), params.frame_time,
    };

    sc::AudioEncoderSink audio_sink { execution_context, container, params };
    sc::PipewireCaptureSource audio_source { execution_context,
                                             params,
                                             audio_sink.frame_size() };

    sc::AudioCapture audio_capture { std::move(audio_sink),
                                     std::move(audio_source) };

    try {
        container.write_header();

        signal.wait([&](auto) {
            audio_capture.cancel();
            video_capture.cancel();
        });

        audio_capture.run([&](exios::Result<std::error_code> result) {
            video_capture.cancel();
            signal.cancel();

            if (!result && result.error() != std::errc::operation_canceled)
                throw std::system_error { result.error() };
        });

        video_capture.run(
            [&](exios::Result<std::error_code> result) {
                audio_capture.cancel();
                signal.cancel();

                if (!result && result.error() != std::errc::operation_canceled)
                    throw std::system_error { result.error() };
            },
            std::allocator<void> {});

        std::cerr << "Capture running. Ctrl+C to stop...\n";
        static_cast<void>(execution_context.run());
        container.write_trailer();
        std::cerr << "Finished\n";
    }
    catch (std::exception const& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
    }
}

struct PipewireInit
{
    PipewireInit(int& argc, char** argv) noexcept { pw_init(&argc, &argv); }
    ~PipewireInit() { pw_deinit(); }
};

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
        PipewireInit pw { argc, const_cast<char**>(argv) };
        app(std::move(params.value()));
    }
}
