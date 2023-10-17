find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
if(CMAKE_CXX_CPPCHECK AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND CMAKE_CXX_CPPCHECK
        "--enable=warning"
        "--error-exitcode=1"
    )
endif()
