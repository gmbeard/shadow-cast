#include "./shadow_cast.hpp"
#include "av/codec.hpp"
#include "av/sample_format.hpp"
#include "config.hpp"
#include "display/display.hpp"
#include "platform/capture.hpp"
#include "platform/display.hpp"
#include "platform/gpu.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/cmd_line.hpp"
#include "utils/frame_time.hpp"
#include "utils/overloads.hpp"

#include <X11/Xlib.h>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <iostream>
#include <libavformat/avformat.h>
#include <memory>
#include <signal.h>
#include <thread>
#include <type_traits>
#include <vector>

#if FF_API_BUFFER_SIZE_T
using BufferSize = int;
#else
using BufferSize = std::size_t;
#endif

template <typename F>
auto add_signal_handler(sc::Context& ctx, std::uint32_t sig, F&& handler)
{
    ctx.services().use_if<sc::SignalService>()->add_signal_handler(
        sig, std::forward<F>(handler));
}

template <typename F>
auto set_audio_chunk_handler(sc::Context& ctx, F&& handler)
{
    ctx.services().use_if<sc::AudioService>()->set_chunk_listener(
        std::forward<F>(handler));
}

template <typename F>
auto set_stream_end_handler(sc::Context& ctx, F&& handler)
{
    ctx.services().use_if<sc::AudioService>()->set_stream_end_listener(
        std::forward<F>(handler));
}

template <typename F>
auto set_video_frame_handler(sc::Context& ctx, F&& handler)
{
    ctx.services().use_if<sc::VideoService>()->set_capture_frame_handler(
        std::forward<F>(handler));
}

template <typename F>
auto set_drm_video_frame_handler(sc::Context& ctx, F&& handler)
{
    ctx.services().use_if<sc::DRMVideoService>()->set_capture_frame_handler(
        std::forward<F>(handler));
}

struct PipewireInit
{
    PipewireInit(int& argc, char** argv) noexcept { pw_init(&argc, &argv); }
    ~PipewireInit() { pw_deinit(); }
};

struct DestroyCaptureSessionGuard
{
    ~DestroyCaptureSessionGuard()
    {
        sc::destroy_nvfbc_capture_session(nvfbc_handle, nvfbc);
    }

    NVFBC_SESSION_HANDLE nvfbc_handle;
    sc::NvFBC nvfbc;
};

auto run_loop(sc::Context& main,
              sc::Context& media,
              sc::Context& audio SC_METRICS_PARAM_DEFINE(sc::Context&, metrics),
              sc::BorrowedPtr<AVCodecContext> video_codec,
              sc::BorrowedPtr<AVStream> video_stream,
              sc::BorrowedPtr<AVCodecContext> audio_codec,
              sc::BorrowedPtr<AVStream> audio_stream) -> void
{
    std::mutex exception_mutex;
    std::exception_ptr ex;
    sc::Encoder encoder { media };

    auto const stop = [&] {
#ifdef SHADOW_CAST_ENABLE_METRICS
        metrics.request_stop();
#endif
        main.request_stop();
        audio.request_stop();
    };

    main.services().add<sc::SignalService>(sc::SignalService {});
    add_signal_handler(main, SIGINT, [&](std::uint32_t) { stop(); });

#ifdef SHADOW_CAST_ENABLE_METRICS
    auto metrics_thread = std::thread([&] {
        SC_SCOPE_GUARD([&] { stop(); });
        try {
            metrics.run();
        }
        catch (...) {
            std::lock_guard lock { exception_mutex };
            if (!ex)
                ex = std::current_exception();
        }
    });
#endif

    auto media_thread = std::thread([&] {
        SC_SCOPE_GUARD([&] { stop(); });
        try {
            media.run();
        }
        catch (...) {
            std::lock_guard lock { exception_mutex };
            if (!ex)
                ex = std::current_exception();
        }
    });

    auto audio_thread = std::thread([&] {
        SC_SCOPE_GUARD([&] { stop(); });
        try {
            audio.run();
        }
        catch (...) {
            std::lock_guard lock { exception_mutex };
            if (!ex)
                ex = std::current_exception();
        }
    });

    {
        SC_SCOPE_GUARD([&] { stop(); });
        try {
            main.run();
        }
        catch (...) {
            std::lock_guard lock { exception_mutex };
            if (!ex)
                ex = std::current_exception();
        }
    }

    std::cerr << "Finalizing output. Please wait...\n";

#ifdef SHADOW_CAST_ENABLE_METRICS
    metrics_thread.join();
#endif
    audio_thread.join();

    try {
        encoder.flush(video_codec.get(), video_stream.get());
        encoder.flush(audio_codec.get(), audio_stream.get());
    }
    catch (...) {
        std::lock_guard lock { exception_mutex };
        if (!ex)
            ex = std::current_exception();
    }

    media.request_stop();
    media_thread.join();

    if (ex)
        std::rethrow_exception(ex);
}

struct MediaEncodingPipeline
{
    MediaEncodingPipeline(sc::CodecContextPtr&& codec_context,
                          AVStream* stream) noexcept
        : codec_context_ { std::move(codec_context) }
        , stream_ { stream }
    {
    }

    auto codec_context() const noexcept -> sc::BorrowedPtr<AVCodecContext>
    {
        return sc::BorrowedPtr<AVCodecContext> { codec_context_.get() };
    }

    auto stream() const noexcept -> sc::BorrowedPtr<AVStream>
    {
        return stream_;
    }

private:
    sc::CodecContextPtr codec_context_;
    sc::BorrowedPtr<AVStream> stream_;
};

auto create_audio_encoder_pipeline(
    sc::Parameters const& params,
    sc::BorrowedPtr<AVFormatContext> format_context) -> MediaEncodingPipeline
{
    sc::BorrowedPtr<AVCodec const> encoder { avcodec_find_encoder_by_name(
        params.audio_encoder.c_str()) };
    if (!encoder) {
        throw sc::CodecError { "Failed to find required audio codec" };
    }

    if (!sc::is_sample_rate_supported(params.sample_rate, encoder))
        throw std::runtime_error { "Sample rate not supported by codec: " +
                                   std::to_string(params.sample_rate) };

    auto const supported_formats = find_supported_formats(encoder);
    if (!supported_formats.size())
        throw std::runtime_error { "No supported sample formats found" };

    sc::CodecContextPtr audio_encoder_context { avcodec_alloc_context3(
        encoder.get()) };
    if (!audio_encoder_context)
        throw sc::CodecError { "Failed to allocate audio codec context" };

#if LIBAVCODEC_VERSION_MAJOR < 60
    audio_encoder_context->channels = 2;
    audio_encoder_context->channel_layout = av_get_default_channel_layout(2);
#else
    av_channel_layout_default(&audio_encoder_context->ch_layout, 2);
#endif
    audio_encoder_context->sample_rate = params.sample_rate;
    audio_encoder_context->sample_fmt =
        sc::convert_to_libav_format(supported_formats.front());
    audio_encoder_context->bit_rate = 128'000;
    audio_encoder_context->time_base = AVRational { 1, params.sample_rate };
    audio_encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (auto const ret =
            avcodec_open2(audio_encoder_context.get(), encoder.get(), nullptr);
        ret < 0) {
        throw sc::CodecError { "Failed to open audio codec: " +
                               sc::av_error_to_string(ret) };
    }

    sc::BorrowedPtr<AVStream> stream { avformat_new_stream(
        format_context.get(), audio_encoder_context->codec) };

    if (!stream)
        throw sc::CodecError { "Failed to allocate audio stream" };

    stream->index = 0;

    if (auto const ret = avcodec_parameters_from_context(
            stream->codecpar, audio_encoder_context.get());
        ret < 0) {
        throw sc::CodecError {
            "Failed to copy codec parameters from context: " +
            sc::av_error_to_string(ret)
        };
    }

    return MediaEncodingPipeline { std::move(audio_encoder_context),
                                   stream.get() };
}

auto create_video_processor(sc::Context& ctx,
                            sc::DisplayEnvironment const& display,
                            sc::GPU const& gpu,
                            sc::Capture const& capture,
                            MediaEncodingPipeline const& video_encoder_pipeline,
                            sc::Encoder& media_writer SC_METRICS_PARAM_DEFINE(
                                sc::Context&, metrics_ctx)) -> void
{
    auto const x11_nvidia_nvfbc = [&](sc::X11Environment const&,
                                      sc::Nvidia const& nv_gpu,
                                      sc::NvFBCCapture const& nvfbc_capture) {
        ctx.services().add_from_factory<sc::VideoService>([&] {
            return std::make_unique<sc::VideoService>(
                nvfbc_capture.nvfbc_instance,
                nv_gpu.cuda_ctx.get(),
                nvfbc_capture.nvfbc_session.get() SC_METRICS_PARAM_USE(
                    metrics_ctx.services().use_if<sc::MetricsService>()));
        });
    };

    auto const wayland_nvidia_egl = [&](sc::WaylandEnvironment const& env,
                                        sc::Nvidia const& nv_gpu,
                                        sc::EGLCapture const& egl_capture) {
        ctx.services().add_from_factory<sc::DRMVideoService>([&] {
            return std::make_unique<sc::DRMVideoService>(
                nv_gpu.cudalib,
                nv_gpu.cuda_ctx.get(),
                egl_capture.egl,
                *env.platform,
                egl_capture.egl_objects SC_METRICS_PARAM_USE(
                    metrics_ctx.services().use_if<sc::MetricsService>()));
        });
    };

    auto const unsupported = [&](auto const&, auto const&, auto const&) {
        throw std::runtime_error {
            "Unsupported display / GPU / capture combination"
        };
    };

    std::visit(
        sc::Overloads { x11_nvidia_nvfbc, wayland_nvidia_egl, unsupported },
        display,
        gpu,
        capture);

    std::visit(
        sc::Overloads {
            [&](sc::WaylandEnvironment const&) {
                set_drm_video_frame_handler(
                    ctx,
                    sc::DRMVideoFrameWriter {
                        video_encoder_pipeline.codec_context().get(),
                        video_encoder_pipeline.stream().get(),
                        media_writer });
            },
            [&](sc::X11Environment const&) {
                set_video_frame_handler(
                    ctx,
                    sc::VideoFrameWriter {
                        video_encoder_pipeline.codec_context().get(),
                        video_encoder_pipeline.stream().get(),
                        media_writer });
            },
            [&](auto const&) {
                throw std::runtime_error { "Unsupported display environment" };
            } },
        display);
}

auto create_video_encoder_pipeline(
    sc::Parameters const& params,
    sc::DisplayEnvironment const& display,
    sc::GPU const& gpu,
    sc::BorrowedPtr<AVFormatContext> format_context,
    sc::BorrowedPtr<AVBufferPool> buffer_pool) -> MediaEncodingPipeline
{
    auto const x11_nvidia =
        [&](sc::X11Environment const& env,
            sc::Nvidia const& nv_gpu) -> sc::CodecContextPtr {
        auto output_width =
            XWidthOfScreen(DefaultScreenOfDisplay(env.display.get()));
        auto output_height =
            XHeightOfScreen(DefaultScreenOfDisplay(env.display.get()));
        SC_EXPECT(output_width > 0 && output_height > 0);
        sc::VideoOutputSize size {
            .width = static_cast<decltype(std::declval<sc::VideoOutputSize>()
                                              .width)>(output_width),
            .height = static_cast<decltype(std::declval<sc::VideoOutputSize>()
                                               .height)>(output_height)
        };

        if (params.resolution) {
            size.width = params.resolution->width;
            size.height = params.resolution->height;
        }

        return sc::create_video_encoder(params.video_encoder.c_str(),
                                        nv_gpu.cuda_ctx.get(),
                                        buffer_pool.get(),
                                        size,
                                        AV_PIX_FMT_BGR0,
                                        params);
    };

    auto const wayland_nvidia =
        [&](sc::WaylandEnvironment const& env,
            sc::Nvidia const& nv_gpu) -> sc::CodecContextPtr {
        return sc::create_video_encoder(
            params.video_encoder.c_str(),
            nv_gpu.cuda_ctx.get(),
            nullptr,
            { .width = env.platform->output_width,
              .height = env.platform->output_height },
            AV_PIX_FMT_BGR0,
            params);
    };

    auto video_encoder_context =
        std::visit(sc::Overloads { x11_nvidia, wayland_nvidia }, display, gpu);

    sc::BorrowedPtr<AVStream> video_stream { avformat_new_stream(
        format_context.get(), video_encoder_context->codec) };
    if (!video_stream)
        throw sc::CodecError { "Failed to allocate video stream" };

    video_stream->index = 1;

    if (auto const ret = avcodec_parameters_from_context(
            video_stream->codecpar, video_encoder_context.get());
        ret < 0) {
        throw sc::CodecError {
            "Failed to copy video codec parameters from context: " +
            sc::av_error_to_string(ret)
        };
    }

    return MediaEncodingPipeline { std::move(video_encoder_context),
                                   video_stream.get() };
}

auto run(sc::Parameters const& params,
         sc::DisplayEnvironment display,
         sc::GPU gpu,
         sc::Capture capture) -> void
{
    AVFormatContext* fc_tmp;
    if (auto const ret = avformat_alloc_output_context2(
            &fc_tmp, nullptr, nullptr, params.output_file.c_str());
        ret < 0) {
        throw sc::FormatError { "Failed to allocate output context: " +
                                sc::av_error_to_string(ret) };
    }

    sc::FormatContextPtr format_context { fc_tmp };

    auto audio_encoder_pipeline =
        create_audio_encoder_pipeline(params, format_context.get());

    sc::BufferPoolPtr buffer_pool { av_buffer_pool_init(
        1, [](BufferSize size) { return av_buffer_alloc(size); }) };

    if (!buffer_pool)
        throw sc::CodecError { "Failed to allocate video buffer pool" };

    auto video_encoder_pipeline = create_video_encoder_pipeline(
        params, display, gpu, format_context.get(), buffer_pool.get());

    if (auto const ret = avio_open(
            &format_context->pb, params.output_file.c_str(), AVIO_FLAG_WRITE);
        ret < 0) {
        throw sc::IOError { "Failed to open output file: " +
                            sc::av_error_to_string(ret) };
    }

    SC_SCOPE_GUARD([&] { avio_close(format_context->pb); });

    sc::Context ctx { params.frame_time };
    sc::Context audio_ctx { params.frame_time };
    sc::Context media_ctx { params.frame_time };
#ifdef SHADOW_CAST_ENABLE_METRICS
    sc::Context metrics_ctx { params.frame_time };
    metrics_ctx.services().add_from_factory<sc::MetricsService>([&] {
        return std::make_unique<sc::MetricsService>(params.output_file);
    });
#endif

    ctx.services().add<sc::SignalService>(sc::SignalService {});
    add_signal_handler(ctx, SIGINT, [&](std::uint32_t) {
        ctx.request_stop();
        audio_ctx.request_stop();
#ifdef SHADOW_CAST_ENABLE_METRICS
        metrics_ctx.request_stop();
#endif
    });

    audio_ctx.services().add_from_factory<sc::AudioService>([&] {
        return std::make_unique<sc::AudioService>(
            sc::convert_from_libav_format(
                audio_encoder_pipeline.codec_context()->sample_fmt),
            params.sample_rate,
            audio_encoder_pipeline.codec_context()
                ->frame_size SC_METRICS_PARAM_USE(
                    metrics_ctx.services().use_if<sc::MetricsService>()));
    });

    media_ctx.services().add_from_factory<sc::EncoderService>([&] {
        return std::make_unique<sc::EncoderService>(
            format_context.get() SC_METRICS_PARAM_USE(
                metrics_ctx.services().use_if<sc::MetricsService>()));
    });

    sc::Encoder media_writer { media_ctx };

    set_audio_chunk_handler(
        audio_ctx,
        sc::ChunkWriter { audio_encoder_pipeline.codec_context().get(),
                          audio_encoder_pipeline.stream().get(),
                          media_writer });

    create_video_processor(ctx,
                           display,
                           gpu,
                           capture,
                           video_encoder_pipeline,
                           media_writer SC_METRICS_PARAM_USE(metrics_ctx));

    if (auto const ret = avformat_write_header(format_context.get(), nullptr);
        ret < 0) {
        throw sc::IOError { "Failed to write header: " +
                            sc::av_error_to_string(ret) };
    }

    SC_SCOPE_GUARD([&] {
        if (auto const ret = av_write_trailer(format_context.get()); ret < 0)
            std::cerr << "Failed to write trailer: "
                      << sc::av_error_to_string(ret) << '\n';
    });

    run_loop(ctx,
             media_ctx,
             audio_ctx SC_METRICS_PARAM_USE(metrics_ctx),
             video_encoder_pipeline.codec_context().get(),
             video_encoder_pipeline.stream().get(),
             audio_encoder_pipeline.codec_context().get(),
             audio_encoder_pipeline.stream().get());
}

auto main(int argc, char const** argv) -> int
{
    try {
        sc::block_signals({ SIGINT, SIGCHLD });
        auto params =
            sc::get_parameters(sc::parse_cmd_line(argc - 1, argv + 1));

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
            auto p = sc::get_value(std::move(params));
            auto display = sc::select_display(p);
            auto gpu = sc::select_gpu(p);
            auto capture = sc::select_capture(p, display, gpu);

            run(p, std::move(display), std::move(gpu), std::move(capture));
        }
    }
    catch (std::exception const& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
