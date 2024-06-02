#include "./audio_encoder_sink.hpp"
#include "./media_container.hpp"
#include "./nvfbc_capture_source.hpp"
#include "./pipewire_capture_source.hpp"
#include "./x11_desktop.hpp"
#include "capture.hpp"
#include "io/signals.hpp"
#include "logging.hpp"
#include "nvenc_encoder_sink.hpp"
#include "session.hpp"
#include "utils/cmd_line.hpp"
#include <signal.h>

#define AUDIO_ENABLED 1
#define VIDEO_ENABLED 1

auto app(sc::Parameters params) -> void
{
    auto cuda_ctx = sc::make_cuda_context();
    exios::ContextThread execution_context;
    exios::Signal signal { execution_context, SIGINT };
    exios::Event cancellation_event { execution_context };
    sc::X11Desktop desktop;

    signal.wait([&](exios::SignalResult) {
        sc::log(sc::LogLevel::info, "Signal received");
        cancellation_event.trigger([&](auto) {
            sc::log(sc::LogLevel::info, "Session cancel requested");
        });
    });

    sc::run_session(execution_context,
                    cancellation_event,
                    desktop,
                    params,
                    cuda_ctx.get(),
                    [&](exios::Result<std::error_code>) {
                        sc::log(sc::LogLevel::info, "Session completed");
                    });

    try {
        static_cast<void>(execution_context.run());
        sc::log(sc::LogLevel::info, "Finalizing output container");
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
