include(MakeTest)
include_directories(${PROJECT_SOURCE_DIR}/experimental)

make_test(NAME capture_frame_tests SOURCES capture_frame_tests.cpp SIMPLE)
make_test(NAME non_pointer_unique_ptr_tests SOURCES non_pointer_unique_ptr_tests.cpp SIMPLE)
