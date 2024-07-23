#include "./shadow_cast.hpp"
#include "utils/frame_time.hpp"

#include <X11/Xlib.h>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <iostream>
#include <libavutil/pixfmt.h>
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

auto run_wayland(sc::Parameters const& params, sc::wayland::DisplayPtr display)
    -> void
{
    sc::load_gl_extensions();

    auto nvcudalib = sc::load_cuda();
    auto egl = sc::egl();

    auto wayland = sc::initialize_wayland(std::move(display));
    auto wayland_egl = sc::initialize_wayland_egl(egl, *wayland);

    if (auto const init_result = nvcudalib.cuInit(0);
        init_result != CUDA_SUCCESS)
        throw sc::NvCudaError { nvcudalib, init_result };
    auto cuda_ctx = sc::create_cuda_context(nvcudalib);

    AVFormatContext* fc_tmp;
    if (auto const ret = avformat_alloc_output_context2(
            &fc_tmp, nullptr, nullptr, params.output_file.c_str());
        ret < 0) {
        throw sc::FormatError { "Failed to allocate output context: " +
                                sc::av_error_to_string(ret) };
    }

    sc::FormatContextPtr format_context { fc_tmp };
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

    /* cuMemcpy2D seems to fail if
     * we use the buffer pool. Hmm...
     */
    // sc::BufferPoolPtr buffer_pool { av_buffer_pool_init(
    //     1, [](int size) { return av_buffer_alloc(size); }) };
    // if (!buffer_pool)
    //     throw sc::CodecError { "Failed to allocate video buffer pool" };

    auto video_encoder_context = sc::create_video_encoder(
        params.video_encoder.c_str(),
        cuda_ctx.get(),
        nullptr, /*buffer_pool.get(),*/
        { .width = wayland->output_width, .height = wayland->output_height },
        params.frame_time,
        AV_PIX_FMT_RGB0);

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

    if (auto const ret = avio_open(
            &format_context->pb, params.output_file.c_str(), AVIO_FLAG_WRITE);
        ret < 0) {
        throw sc::IOError { "Failed to open output file: " +
                            sc::av_error_to_string(ret) };
    }

    struct AVIOContextCloseGuard
    {
        ~AVIOContextCloseGuard() { avio_close(ctx); }
        AVIOContext* ctx;
    } avio_context_close_guard { format_context->pb };

    if (auto const ret = avformat_write_header(format_context.get(), nullptr);
        ret < 0) {
        throw sc::IOError { "Failed to write header: " +
                            sc::av_error_to_string(ret) };
    }

#ifdef SHADOW_CAST_ENABLE_METRICS
    sc::Context metrics_ctx { params.frame_time };
    metrics_ctx.services().add_from_factory<sc::MetricsService>([&] {
        return std::make_unique<sc::MetricsService>(params.output_file);
    });
#endif
    sc::Context ctx { params.frame_time };
    sc::Context audio_ctx { params.frame_time };
    sc::Context media_ctx { params.frame_time };

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
            supported_formats.front(),
            params.sample_rate,
            audio_encoder_context->frame_size
                ? audio_encoder_context->frame_size
                : 2048 SC_METRICS_PARAM_USE(
                      metrics_ctx.services().use_if<sc::MetricsService>()));
    });

    ctx.services().add_from_factory<sc::DRMVideoService>([&] {
        return std::make_unique<sc::DRMVideoService>(
            nvcudalib,
            cuda_ctx.get(),
            egl,
            *wayland,
            wayland_egl SC_METRICS_PARAM_USE(
                metrics_ctx.services().use_if<sc::MetricsService>()));
    });

    media_ctx.services().add_from_factory<sc::EncoderService>([&] {
        return std::make_unique<sc::EncoderService>(
            format_context.get() SC_METRICS_PARAM_USE(
                metrics_ctx.services().use_if<sc::MetricsService>()));
    });

    sc::Encoder media_writer { media_ctx };

    set_audio_chunk_handler(audio_ctx,
                            sc::ChunkWriter { audio_encoder_context.get(),
                                              stream.get(),
                                              media_writer });
    set_drm_video_frame_handler(
        ctx,
        sc::DRMVideoFrameWriter {
            video_encoder_context.get(), video_stream.get(), media_writer });

    SC_SCOPE_GUARD([&] {
        if (auto const ret = av_write_trailer(format_context.get()); ret < 0)
            std::cerr << "Failed to write trailer: "
                      << sc::av_error_to_string(ret) << '\n';
    });

    run_loop(ctx,
             media_ctx,
             audio_ctx SC_METRICS_PARAM_USE(metrics_ctx),
             video_encoder_context.get(),
             video_stream.get(),
             audio_encoder_context.get(),
             stream.get());
}

auto run(sc::Parameters const& params) -> void
{
    auto const display = sc::get_display();
    /* CUDA and NvFBC...
     */
    auto nvcudalib = sc::load_cuda();
    auto nvfbclib = sc::load_nvfbc();
    if (auto const init_result = nvcudalib.cuInit(0);
        init_result != CUDA_SUCCESS)
        throw sc::NvCudaError { nvcudalib, init_result };
    auto cuda_ctx = sc::create_cuda_context(nvcudalib);
    auto nvfbc = nvfbclib.NvFBCCreateInstance();

    /* TODO: We can probably combine these two calls into
     * one function that returns an RAII object...
     */
    auto nvfbc_instance = sc::create_nvfbc_session(nvfbc);
    sc::create_nvfbc_capture_session(
        nvfbc_instance.get(), nvfbc, params.frame_time);

    DestroyCaptureSessionGuard destroy_capture_session_guard {
        nvfbc_instance.get(), nvfbc
    };

    AVFormatContext* fc_tmp;
    if (auto const ret = avformat_alloc_output_context2(
            &fc_tmp, nullptr, nullptr, params.output_file.c_str());
        ret < 0) {
        throw sc::FormatError { "Failed to allocate output context: " +
                                sc::av_error_to_string(ret) };
    }

    sc::FormatContextPtr format_context { fc_tmp };
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

    sc::BufferPoolPtr buffer_pool { av_buffer_pool_init(
        1, [](BufferSize size) { return av_buffer_alloc(size); }) };

    if (!buffer_pool)
        throw sc::CodecError { "Failed to allocate video buffer pool" };

    auto output_width = XWidthOfScreen(DefaultScreenOfDisplay(display.get()));
    auto output_height = XHeightOfScreen(DefaultScreenOfDisplay(display.get()));
    SC_EXPECT(output_width > 0 && output_height > 0);
    sc::VideoOutputSize size {
        .width =
            static_cast<decltype(std::declval<sc::VideoOutputSize>().width)>(
                output_width),
        .height =
            static_cast<decltype(std::declval<sc::VideoOutputSize>().height)>(
                output_height)
    };

    auto video_encoder_context =
        sc::create_video_encoder(params.video_encoder.c_str(),
                                 cuda_ctx.get(),
                                 buffer_pool.get(),
                                 size,
                                 params.frame_time,
                                 AV_PIX_FMT_BGR0);

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

    if (auto const ret = avio_open(
            &format_context->pb, params.output_file.c_str(), AVIO_FLAG_WRITE);
        ret < 0) {
        throw sc::IOError { "Failed to open output file: " +
                            sc::av_error_to_string(ret) };
    }

    struct AVIOContextCloseGuard
    {
        ~AVIOContextCloseGuard() { avio_close(ctx); }
        AVIOContext* ctx;
    } avio_context_close_guard { format_context->pb };

    if (auto const ret = avformat_write_header(format_context.get(), nullptr);
        ret < 0) {
        throw sc::IOError { "Failed to write header: " +
                            sc::av_error_to_string(ret) };
    }

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
            supported_formats.front(),
            params.sample_rate,
            audio_encoder_context->frame_size
                ? audio_encoder_context->frame_size
                : 2048 SC_METRICS_PARAM_USE(
                      metrics_ctx.services().use_if<sc::MetricsService>()));
    });

    ctx.services().add_from_factory<sc::VideoService>([&] {
        return std::make_unique<sc::VideoService>(
            nvfbc,
            cuda_ctx.get(),
            nvfbc_instance.get() SC_METRICS_PARAM_USE(
                metrics_ctx.services().use_if<sc::MetricsService>()));
    });

    media_ctx.services().add_from_factory<sc::EncoderService>([&] {
        return std::make_unique<sc::EncoderService>(
            format_context.get() SC_METRICS_PARAM_USE(
                metrics_ctx.services().use_if<sc::MetricsService>()));
    });

    sc::Encoder media_writer { media_ctx };

    set_audio_chunk_handler(audio_ctx,
                            sc::ChunkWriter { audio_encoder_context.get(),
                                              stream.get(),
                                              media_writer });
    set_video_frame_handler(ctx,
                            sc::VideoFrameWriter { video_encoder_context.get(),
                                                   video_stream.get(),
                                                   media_writer });

    SC_SCOPE_GUARD([&] {
        if (auto const ret = av_write_trailer(format_context.get()); ret < 0)
            std::cerr << "Failed to write trailer: "
                      << sc::av_error_to_string(ret) << '\n';
    });

    run_loop(ctx,
             media_ctx,
             audio_ctx SC_METRICS_PARAM_USE(metrics_ctx),
             video_encoder_context.get(),
             video_stream.get(),
             audio_encoder_context.get(),
             stream.get());
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
            sc::wayland::DisplayPtr wayland_display { wl_display_connect(
                nullptr) };
            if (wayland_display)
                run_wayland(sc::get_value(std::move(params)),
                            std::move(wayland_display));
            else {
                auto p = sc::get_value(std::move(params));

                if (!p.strict_frame_time)
                    p.frame_time = sc::truncate_to_millisecond(p.frame_time);

                run(std::move(p));
            }
        }
    }
    catch (std::exception const& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
