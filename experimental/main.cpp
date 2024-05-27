#include "./audio_encoder_sink.hpp"
#include "./media_container.hpp"
#include "./nvfbc_capture_source.hpp"
#include "./pipewire_capture_source.hpp"
#include "./x11_desktop.hpp"
#include "capture.hpp"
#include "io/signals.hpp"
#include "logging.hpp"
#include "nvenc_encoder_sink.hpp"
#include "utils/cmd_line.hpp"
#include <iostream>
#include <signal.h>

#define AUDIO_ENABLED 1
#define VIDEO_ENABLED 1

auto app(sc::Parameters params) -> void
{
    auto cuda_ctx = sc::make_cuda_context();
    exios::ContextThread execution_context;
    exios::Signal signal { execution_context, SIGINT };
    sc::X11Desktop desktop;
    sc::MediaContainer container { params.output_file };

#if defined(VIDEO_ENABLED)
    sc::NvfbcCaptureSource video_source { execution_context,
                                          params,
                                          desktop.size() };
    sc::NvencEncoderSink video_sink {
        execution_context, cuda_ctx.get(), container, params, desktop.size()
    };
    sc::Capture video_capture { std::move(video_source),
                                std::move(video_sink) };
#endif

#if defined(AUDIO_ENABLED)
    sc::AudioEncoderSink audio_sink { execution_context, container, params };
    sc::PipewireCaptureSource audio_source { execution_context,
                                             params,
                                             audio_sink.frame_size() };

    sc::Capture audio_capture { std::move(audio_source),
                                std::move(audio_sink) };
#endif

    try {
        sc::log(sc::LogLevel::info, "Preparing output container");
        container.write_header();

        signal.wait([&](auto) {
            sc::log(sc::LogLevel::info, "Signal received. Stopping.");
#if defined(AUDIO_ENABLED)
            audio_capture.cancel();
#endif
#if defined(VIDEO_ENABLED)
            video_capture.cancel();
#endif
        });

#if defined(AUDIO_ENABLED)
        audio_capture.run([&](exios::Result<std::error_code> result) {
#if defined(VIDEO_ENABLED)
            video_capture.cancel();
#endif
            signal.cancel();

            if (!result && result.error() != std::errc::operation_canceled)
                throw std::system_error { result.error() };
        });
#endif

#if defined(VIDEO_ENABLED)
        video_capture.run([&](exios::Result<std::error_code> result) {
#if defined(AUDIO_ENABLED)
            audio_capture.cancel();
#endif
            signal.cancel();

            if (!result && result.error() != std::errc::operation_canceled)
                throw std::system_error { result.error() };
        });
#endif

        sc::log(sc::LogLevel::info, "Capture running.");
        static_cast<void>(execution_context.run());
        sc::log(sc::LogLevel::info, "Finalizing output container");
        container.write_trailer();
        sc::log(sc::LogLevel::info, "Finished");
    }
    catch (std::exception const& e) {
        sc::log(sc::LogLevel::error, "%s", e.what());
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
