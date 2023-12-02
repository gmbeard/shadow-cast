configure_file(
    config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.hpp
)

add_library(shadow-cast-obj
    OBJECT
    av/buffer.cpp
    av/buffer_pool.cpp
    av/codec.cpp
    av/format.cpp
    av/frame.cpp
    av/media_chunk.cpp
    av/packet.cpp
    av/sample_format.cpp

    display/display.cpp

    drm/messaging.cpp
    drm/planes.cpp

    handlers/audio_chunk_writer.cpp
    handlers/drm_video_frame_writer.cpp
	handlers/stream_finalizer.cpp
    handlers/video_frame_writer.cpp

    io/accept_handler.cpp
    io/process.cpp
    io/signals.cpp
    io/unix_socket.cpp

    platform/egl.cpp
    platform/wayland.cpp

    services/audio_service.cpp
    services/context.cpp
    services/drm_video_service.cpp
    services/encoder.cpp
    services/encoder_service.cpp
    services/metrics_service.cpp
    services/readiness.cpp
    services/service.cpp
    services/service_registry.cpp
    services/signal_service.cpp
    services/video_service.cpp

    utils/base64.cpp
    utils/cmd_line.cpp
    utils/contracts.cpp
    utils/elapsed.cpp
    utils/frame_time.cpp
    utils/result.cpp

    error.cpp
    logging.cpp
    nvidia.cpp
)

target_include_directories(shadow-cast-obj
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(shadow-cast-obj
    PUBLIC
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

add_executable(shadow-cast
   main.cpp
) 

target_link_libraries(shadow-cast
    PRIVATE
    shadow-cast-obj
)

set(
    SHADOW_CAST_KMS_BINARY_NAME
    "shadow-cast-kms"
    PARENT_SCOPE
)

add_executable(shadow-cast-kms
    kms.cpp
)

set_target_properties(
    shadow-cast-kms
    PROPERTIES
        OUTPUT_NAME "${SHADOW_CAST_KMS_BINARY_NAME}"
)

target_link_libraries(shadow-cast-kms
    PRIVATE
    shadow-cast-obj
    DRM::drm
)
