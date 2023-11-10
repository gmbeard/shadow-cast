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
    av/sample_format.cpp

    display/display.cpp

	handlers/stream_finalizer.cpp
    handlers/audio_chunk_writer.cpp
    handlers/video_frame_writer.cpp

    services/audio_service.cpp
    services/context.cpp
    services/readiness.cpp
    services/service.cpp
    services/service_registry.cpp
    services/signal_service.cpp
    services/video_service.cpp

    utils/cmd_line.cpp
    utils/base64.cpp
    utils/elapsed.cpp
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
    FFMpeg::avresample
    FFMpeg::avutil
    FFMpeg::swresample
    FFMpeg::swscale
    pthread
    X11::X11
)

add_executable(shadow-cast
   main.cpp
) 

target_link_libraries(shadow-cast
    PRIVATE
    shadow-cast-obj
)
