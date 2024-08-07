include(MakeTest)
include_directories(${PROJECT_SOURCE_DIR}/experimental)

make_test(NAME capture_frame_tests SOURCES capture_frame_tests.cpp)
make_test(NAME non_pointer_unique_ptr_tests SOURCES non_pointer_unique_ptr_tests.cpp)
make_test(NAME wayland_desktop_tests SOURCES wayland_desktop_tests.cpp ENABLE_IF wayland)
