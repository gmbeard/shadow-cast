configure_file(
    config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.hpp
)

add_subdirectory(glsl)

add_library(shadowcastobjects)
target_sources(
    shadowcastobjects
    PUBLIC
        audio_encoder_sink.cpp
        arena.cpp
        capture.cpp
        "$<$<BOOL:SHADOW_CAST_ENABLE_HISTOGRAMS>:${CMAKE_CURRENT_SOURCE_DIR}/cpu_usage.cpp>"
        cuda.cpp
        desktop.cpp
        drm_cuda_capture_source.cpp
        gpu.cpp
        logging.cpp
        media_container.cpp
        nvenc_encoder_sink.cpp
        nvfbc.cpp
        nvfbc_capture_source.cpp
        nvidia_gpu.cpp
        packet_queue.cpp
        pipewire_capture_source.cpp
        session.cpp
        sticky_cancel_event.cpp
        sticky_cancel_timer.cpp
        x11_desktop.cpp
        x11_platform.cpp
        wayland_desktop.cpp

        av/buffer.cpp
        av/buffer_pool.cpp
        av/codec.cpp
        av/format.cpp
        av/frame.cpp
        av/media_chunk.cpp
        av/packet.cpp
        av/sample_format.cpp

        color_converter.cpp

        display/display.cpp

        drm/messaging.cpp
        drm/planes.cpp

        error.cpp

        gl/buffer.cpp
        gl/core.cpp
        gl/error.cpp
        gl/framebuffer.cpp
        gl/program.cpp
        gl/shader.cpp
        gl/texture.cpp
        gl/vertex_array_object.cpp

        io/accept_handler.cpp
        io/process.cpp
        io/signals.cpp
        io/unix_socket.cpp

        metrics/metrics.cpp
        metrics/profiling.cpp

        platform/egl.cpp
        platform/opengl.cpp
        platform/wayland.cpp

        utils/base64.cpp
        utils/cmd_line.cpp
        utils/contracts.cpp
        utils/elapsed.cpp
        utils/frame_time.cpp
        utils/result.cpp

        nvidia.cpp
)

target_include_directories(shadowcastobjects
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(
    shadowcastobjects
    PUBLIC
        Exios::exios
        Pipewire::pipewire
        FFMpeg::avcodec
        FFMpeg::avdevice
        FFMpeg::avfilter
        FFMpeg::avformat
        FFMpeg::avutil
        FFMpeg::swresample
        FFMpeg::swscale
        pthread
        X11::X11
        wayland-client
        wayland-egl
)

add_executable(shadowcast)
target_sources(
    shadowcast
    PRIVATE
        main.cpp
)

target_link_libraries(
    shadowcast
    PRIVATE
        embedded_glsl
        shadowcastobjects
)

set(
    SHADOW_CAST_KMS_BINARY_NAME
    "shadow-cast-kms"
    PARENT_SCOPE
)

add_executable(shadow-cast-kms)

target_sources(shadow-cast-kms
    PRIVATE
        kms.cpp
)

set_target_properties(
    shadow-cast-kms
    PROPERTIES
        OUTPUT_NAME "${SHADOW_CAST_KMS_BINARY_NAME}"
)

target_link_libraries(shadow-cast-kms
    PRIVATE
        embedded_glsl
        shadowcastobjects
        DRM::drm
)
