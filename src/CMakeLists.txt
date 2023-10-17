add_library(shadow-cast-obj
    OBJECT
    audio_service.cpp
    codec.cpp
    context.cpp
    elapsed.cpp
    format.cpp
    frame.cpp
    media_chunk.cpp
    readiness.cpp
    result.cpp
    service.cpp
    service_registry.cpp
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
)

add_executable(shadow-cast
   main.cpp
) 

target_link_libraries(shadow-cast
    PRIVATE
    shadow-cast-obj
)
