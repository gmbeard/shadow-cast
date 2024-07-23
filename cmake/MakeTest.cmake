function(make_test)
    set(single_value_args NAME)
    set(multi_value_args SOURCES ENV_VARS ENABLE_IF LABELS LINK_LIBRARIES)
    cmake_parse_arguments(
        MAKE_TEST
        "${options}"
        "${single_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(NOT MAKE_TEST_LABELS)
        set(MAKE_TEST_LABELS "default-tests")
    endif()

    add_executable(${MAKE_TEST_NAME} ${MAKE_TEST_SOURCES})
    target_link_libraries(${MAKE_TEST_NAME} PRIVATE testing embedded_glsl shadow-cast-obj ${MAKE_TEST_LINK_LIBRARIES})
    add_test(NAME ${MAKE_TEST_NAME} COMMAND ${MAKE_TEST_NAME})
    set_tests_properties(
        ${MAKE_TEST_NAME}
        PROPERTIES
            ENVIRONMENT "${MAKE_TEST_ENV_VARS}"
            LABELS "${MAKE_TEST_LABELS}"
    )

    # If the test doesn't specify any ENABLE_IF flags then
    # it is always included...
    list(LENGTH MAKE_TEST_ENABLE_IF enable_if_length)
    if(enable_if_length EQUAL 0)
        return()
    endif()

    # Loop through the ENABLE_IF flags. If any are found in
    # the test properties then we return early and the test
    # is included...
    while(enable_if_length GREATER 0)
        list(POP_FRONT MAKE_TEST_ENABLE_IF enable_if_val)
        if(${enable_if_val} IN_LIST SHADOW_CAST_ENABLE_TEST_CATEGORIES)
            return()
        endif()
        list(LENGTH MAKE_TEST_ENABLE_IF enable_if_length)
    endwhile()

    # At the point, there were ENABLE_IF flags specified but
    # non were found in the test properties, so disable it...
    set_tests_properties(
        ${MAKE_TEST_NAME}
        PROPERTIES
            DISABLED ON
    )
endfunction()
