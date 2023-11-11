#include "./shadow_cast.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <iostream>
#include <signal.h>
#include <thread>
#include <vector>

auto block_signals(std::initializer_list<decltype(SIGINT)> sigs) -> void
{
    sigset_t blocked_signals;
    sigemptyset(&blocked_signals);
    for (auto sig : sigs) {
        if (auto const result = sigaddset(&blocked_signals, sig); result < 0)
            throw std::system_error { errno, std::system_category() };
    }
    if (auto const result =
            pthread_sigmask(SIG_SETMASK, &blocked_signals, nullptr);
        result < 0)
        throw std::system_error { errno, std::system_category() };
}

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
        nvfbc_instance.get(), nvfbc, params.frame_rate);

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
    sc::BorrowedPtr<AVCodec> encoder { avcodec_find_encoder_by_name(
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

    audio_encoder_context->channels = 2;
    audio_encoder_context->channel_layout = av_get_default_channel_layout(2);
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
        1, [](int size) { return av_buffer_alloc(size); }) };
    if (!buffer_pool)
        throw sc::CodecError { "Failed to allocate video buffer pool" };

    auto video_encoder_context =
        sc::create_video_encoder(params.video_encoder.c_str(),
                                 cuda_ctx.get(),
                                 buffer_pool.get(),
                                 display.get(),
                                 params.frame_rate);

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

    sc::Context ctx { static_cast<std::uint32_t>(params.frame_rate) };
    sc::Context audio_ctx { static_cast<std::uint32_t>(params.frame_rate) };

    ctx.services().add<sc::SignalService>(sc::SignalService {});
    add_signal_handler(ctx, SIGINT, [&](std::uint32_t) {
        ctx.request_stop();
        audio_ctx.request_stop();
    });

    audio_ctx.services().add_from_factory<sc::AudioService>([&] {
        return std::make_unique<sc::AudioService>(
            supported_formats.front(),
            params.sample_rate,
            audio_encoder_context->frame_size);
    });

    ctx.services().add_from_factory<sc::VideoService>([&] {
        return std::make_unique<sc::VideoService>(
            nvcudalib, nvfbc, cuda_ctx.get(), nvfbc_instance.get());
    });

    set_audio_chunk_handler(audio_ctx,
                            sc::ChunkWriter { format_context.get(),
                                              audio_encoder_context.get(),
                                              stream.get() });
    set_video_frame_handler(ctx,
                            sc::VideoFrameWriter { format_context.get(),
                                                   video_encoder_context.get(),
                                                   video_stream.get() });

    set_stream_end_handler(audio_ctx,
                           sc::StreamFinalizer { format_context.get(),
                                                 audio_encoder_context.get(),
                                                 video_encoder_context.get(),
                                                 stream.get(),
                                                 video_stream.get() });

    auto audio_thread = std::thread([&] { audio_ctx.run(); });
    ctx.run();
    audio_thread.join();

    std::cerr << "Finalizing output...\n";
    if (auto const ret = av_write_trailer(format_context.get()); ret < 0)
        throw std::runtime_error { "Failed to write trailer: " +
                                   sc::av_error_to_string(ret) };
}

auto main(int argc, char const** argv) -> int
{
    try {
        block_signals({ SIGINT });
        auto const params =
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
            run(sc::get_value(std::move(params)));
        }
    }
    catch (std::exception const& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
