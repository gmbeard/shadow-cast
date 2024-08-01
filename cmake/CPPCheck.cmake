find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
if(CMAKE_CXX_CPPCHECK AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND CMAKE_CXX_CPPCHECK
        "--check-level=exhaustive"
        "--inline-suppr"
        "--enable=warning"
        "--error-exitcode=1"
    )
else()
    set(CMAKE_CXX_CPPCHECK "")
endif()
