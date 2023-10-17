function(make_test)
    set(options FULL SIMPLE)
    set(single_value_args NAME ENABLE_MATCH)
    set(multi_value_args SOURCES ENV_VARS)
    cmake_parse_arguments(
        MAKE_TEST
        "${options}"
        "${single_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    message(STATUS "Enable match: ${${MAKE_TEST_ENABLE_MATCH}}")
    if((MAKE_TEST_SIMPLE) AND (NOT ${MAKE_TEST_ENABLE_MATCH} OR ${MAKE_TEST_ENABLE_MATCH} STREQUAL "SIMPLE"))
        set(TEST_ENABLED ON)
    elseif(MAKE_TEST_FULL AND ${MAKE_TEST_ENABLE_MATCH} STREQUAL "FULL")
        set(TEST_ENABLED ON)
    endif()

    if (NOT TEST_ENABLED)
        return()
    endif()
    add_executable(${MAKE_TEST_NAME} ${MAKE_TEST_SOURCES})
    target_link_libraries(${MAKE_TEST_NAME} PRIVATE testing shadow-cast-obj)
    add_test(NAME ${MAKE_TEST_NAME} COMMAND ${MAKE_TEST_NAME})
    set_tests_properties(
        ${MAKE_TEST_NAME}
        PROPERTIES
            ENVIRONMENT "${MAKE_TEST_ENV_VARS}"
    )
endfunction()
