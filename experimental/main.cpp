#include "./audio_encoder_sink.hpp"
#include "./egl.hpp"
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
#include "utils/scope_guard.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
#include <X11/Xlib.h>
#include <signal.h>

#define AUDIO_ENABLED 1
#define VIDEO_ENABLED 1

auto app(sc::Parameters params) -> void
{
    auto cuda_ctx = sc::make_cuda_context();
    exios::ContextThread execution_context;
    exios::Signal signal { execution_context, SIGINT };
    exios::Event cancellation_event { execution_context };
    auto gfx = sc::bind_gfx(sc::X11Desktop {});

    gfx.with_egl_context_args([](auto egl_display, auto, auto) {
        auto egl_client_apis =
            sc::egl::lib().eglQueryString(egl_display, EGL_CLIENT_APIS);
        sc::log(sc::LogLevel::debug, "EGL Client APIs: %s", egl_client_apis);

        auto egl_vendor =
            sc::egl::lib().eglQueryString(egl_display, EGL_VENDOR);
        sc::log(sc::LogLevel::debug, "EGL Vendor: %s", egl_vendor);

        // auto egl_extensions =
        //     sc::egl::lib().eglQueryString(egl_display, EGL_EXTENSIONS);
        // sc::log(sc::LogLevel::debug, "EGL Extensions: %s", egl_extensions);

        auto gl_renderer = sc::gl::lib().glGetString(GL_RENDERER);
        auto gl_vendor = sc::gl::lib().glGetString(GL_VENDOR);
        sc::log(sc::LogLevel::info, "GPU: %s (%s)", gl_renderer, gl_vendor);
        // int num_extensions = 0;
        // sc::gl::lib().glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
        // for (auto i = 0; i < num_extensions; ++i) {
        //     auto gl_extension = sc::gl::lib().glGetStringi(GL_EXTENSIONS, i);
        //     sc::log(sc::LogLevel::debug, "GL Extensions %s", gl_extension);
        // }
    });

    signal.wait([&](exios::SignalResult) {
        sc::log(sc::LogLevel::info, "Signal received");
        cancellation_event.trigger([&](auto) {
            sc::log(sc::LogLevel::info, "Session cancel requested");
        });
    });

    sc::run_session(execution_context,
                    cancellation_event,
                    gfx.native_display(),
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
