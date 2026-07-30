include(GNUInstallDirs)
add_executable(drm_planes)

target_sources(
    drm_planes
    PRIVATE
    drm_planes/dmabuf_exporter.cpp
    drm_planes/dmabuf_handle.cpp
    drm_planes/drm_device.cpp
    drm_planes/drm_plane.cpp
    drm_planes/drm_plane_check_loop.cpp
    drm_planes/drm_plane_framebuffer.cpp
    drm_planes/drm_plane_property.cpp
    drm_planes/drm_planes.cpp
    drm_planes/frame_time_detector.cpp
    drm_planes/main.cpp
)

target_include_directories(
    drm_planes
    PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/drm_planes"
)

set_target_properties(
    drm_planes
    PROPERTIES
        OUTPUT_NAME "shadowcast-drm-exporter"
)

target_link_libraries(
    drm_planes
    PRIVATE
    shadowcastobjects
    DRM::drm
)

add_executable(senders)

target_sources(
    senders
    PRIVATE
    senders/main.cpp
)

target_include_directories(
    senders
    PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/senders"
)

target_link_libraries(
    senders
    PRIVATE
    shadowcastobjects
    DRM::drm
)

install(
    TARGETS drm_planes
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}"
)
