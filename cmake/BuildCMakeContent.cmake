if(NOT BUILD_CMAKE_CONTENT_CMAKE)
    set(BUILD_CMAKE_CONTENT_CMAKE 1)
else()
    return()
endif()

function(BuildCMakeContent bcc_NAME bcc_PACKAGE)
    cmake_parse_arguments(bcc
            "" # options
            "" #<one_value_keywords>
            "CMAKE_ARGS" # multi_value keywords
            ${ARGN}) #<args>...)
    if (bcc_NAME STREQUAL "")
        message(FATAL_ERROR "BuildDependency: requires name")
    endif()
    if (bcc_PACKAGE STREQUAL "")
        message(FATAL_ERROR "BuildDependency: requires package")
    endif()

    string(TOUPPER $bcc_NAME bcc_NAME_UPPER)
    set("FETCHCONTENT_UPDATES_DISCONNECTED_${bcc_NAME_UPPER}" ON CACHE BOOL "Whether to check for updates" FORCE)


    message(STATUS "[dependencies] fetching properties for ${bcc_NAME}")
    FetchContent_GetProperties("${bcc_NAME}")
    if (NOT "${${bcc_NAME}_POPULATED}")
        FetchContent_Populate(${bcc_NAME})
        FetchContent_GetProperties("${bcc_NAME}")
        message(STATUS "[dependencies] Populating ${bcc_NAME} in ${bcc_NAME}_SOURCE_DIR")
    endif()

    #
    # configure step
    #

    set(bcc_configure_options
            "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
            "-DCMAKE_INSTALL_PREFIX=${deps_prefix}"
            "-DCMAKE_PREFIX_PATH=${deps_prefix}"
            ${bcc_CMAKE_ARGS}
            "-H${${bcc_NAME}_SOURCE_DIR}"
            "-B${${bcc_NAME}_BINARY_DIR}")
    if (NOT "${${bcc_NAME}_CONFIGURED}" STREQUAL "${bcc_configure_options}")
        message(STATUS "[dependencies] ${bcc_NAME} previously configured with ${${bcc_NAME}_CONFIGURED}")
        message(STATUS "[dependencies] Configuring ${bcc_NAME} with args ${bcc_configure_options}")
        set(${bcc_NAME}_BUILT "" CACHE INTERNAL "")
        set(${bcc_NAME}_INSTALLED "" CACHE INTERNAL "")
        execute_process(COMMAND "${CMAKE_COMMAND}" ${bcc_configure_options})
        if (NOT bcc_RESULT)
            set("${bcc_NAME}_CONFIGURED" "${bcc_configure_options}" CACHE INTERNAL "${bcc_NAME} has been configured")
        else()
            message(FATAL_ERROR "[dependency] failed to configure ${bcc_NAME}")
        endif()
    endif()

    #
    # build step
    #

    set(bcc_build_options "--build" "${${bcc_NAME}_BINARY_DIR}" "--parallel" "${processors}")
    if (NOT "${${bcc_NAME}_BUILT}" STREQUAL "${bcc_build_options}")
        message(STATUS "[dependencies] ${bcc_NAME} previously built ${${bcc_NAME}_BUILT}")
        message(STATUS "[dependencies] Building ${bcc_NAME}")
        set(${bcc_NAME}_INSTALLED "" CACHE INTERNAL "")
        execute_process(COMMAND "${CMAKE_COMMAND}" ${bcc_build_options}
                RESULT_VARIABLE bcc_RESULT)
        if (bcc_RESULT)
            message(FATAL_ERROR "build failed")
        else()
            set("${bcc_NAME}_BUILT" "${bcc_build_options}" CACHE INTERNAL "${bcc_NAME} has been built")
        endif()
    endif()

    #
    # install step
    #

    set(bcc_install_options "--install" "${${bcc_NAME}_BINARY_DIR}")
    if (NOT "${${bcc_NAME}_INSTALLED}" STREQUAL "${bcc_install_options}")
        message(STATUS "[dependencies] ${bcc_NAME} previously installed ${${bcc_NAME}_INSTALLED}")
        message(STATUS "[dependencies] Installing ${bcc_NAME}")
        execute_process(COMMAND "${CMAKE_COMMAND}"  ${bcc_install_options} RESULT_VARIABLE bcc_result)
        if (bcc_RESULT)
            message(FATAL_ERROR "build failed")
        else()
            set("${bcc_NAME}_INSTALLED" "${bcc_install_options}" CACHE INTERNAL "${bcc_NAME} has been installed")
        endif()
    endif()

    message(STATUS "[dependencies] ${bcc_PACKAGE}_ROOT=${deps_prefix}")
endfunction()
