if(NOT REQUIRE_BOOST_CMAKE)
    set(REQUIRE_BOOST_CMAKE 1)
else()
    return()
endif()

#
# file level configuation
#

set(boost_PATCHES_DIR boost-patch)

function(ListToString list outstr)
    set(result)
    foreach(item IN LISTS ${list})
        set(result "${result} \"${item}\"")
    endforeach()
    set(${outstr} "${result}" PARENT_SCOPE)
endfunction()

function(BoostDeduceToolSet out)
    set(result)
    if (CMAKE_CXX_COMPILER_ID MATCHES "[Cl]ang")
        set(result "clang")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(result "gcc")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(result "msvc")
    else()
        message(FATAL_ERROR "compiler: ${CMAKE_CXX_COMPILER} : please update the script with a boost toolset")
    endif()
    message(STATUS "[dependencies]: Boost toolset deduced: ${result}")
    set("${out}" "${result}" PARENT_SCOPE)
endfunction()

function(BoostDeduceCXXVersion boost_version out)
    set(result)
    if (CMAKE_CXX_STANDARD LESS_EQUAL 17)
        set(result "${CMAKE_CXX_STANDARD}")
    elseif(CMAKE_CXX_STANDARD EQUAL 20)
        if (boost_version VERSION_LESS 1.74)
            set(result 2a)
        else()
            set(result 20)
        endif()
    else()
        message(FATAL_ERROR "c++ standard ${CMAKE_CXX_STANDARD} not supported yet")
    endif()
    message(STATUS "[dependencies]: Boost cxxstd deduced: ${result}")
    set("${out}" "${result}" PARENT_SCOPE)
endfunction()

function(RequireBoost )
    cmake_parse_arguments(boost
            "" # options
            "VERSION;PREFIX" #<one_value_keywords>
            "COMPONENTS" #<multi_value_keywords>
            ${ARGN}) #<args>...)
    if (NOT boost_VERSION)
        message(FATAL_ERROR "RequireBoost: requires VERSION argument")
    endif()
    if (NOT boost_PREFIX)
        message(FATAL_ERROR "RequireBoost: requires PREFIX argument")
    endif()
    if (NOT boost_COMPONENTS)
        set(boost_COMPONENTS headers)
    endif()

    include(FetchContent)
    set(boost_git_repo "git@github.com:boostorg/boost.git")
    set(boost_git_branch "boost-${boost_VERSION}")
    FetchContent_Declare(boost
            GIT_REPOSITORY "${boost_git_repo}"
            GIT_TAG "${boost_git_branch}")
    FetchContent_GetProperties(boost)
    if (NOT boost_POPULATED)
        message(STATUS "[dependencies] Boost: downloading [${boost_git_repo}] [${boost_git_branch}]")
        FetchContent_Populate(boost)
    endif ()

    #
    # patch step
    #

    file(GLOB patches CONFIGURE_DEPENDS "${boost_PATCHES_DIR}/version-${boost_VERSION}-*.patch")
    if (NOT boost_PATCHES_APPLIED STREQUAL "${patches}")
        message(STATUS "[dependencies] applying boost patches ${patches}")

        set(boost_BOOTSTRAPPED "" CACHE INTERNAL "")
        set(boost_BUILT "" CACHE INTERNAL "")

        foreach(patch IN LISTS patches)
            string(REGEX MATCH "^(.*)/version-(.*)-(.*)\\.patch$" matched "${patch}")
            if (NOT matched)
                message(FATAL_ERROR "[dependencies] incorrect patch file format: ${patch}  ${matched}")
            endif()
            set(component ${CMAKE_MATCH_3})
            set(proc_args "patch" "-p2" "--backup" "-i" "${patch}")
            message(STATUS "[dependencies] patching boost component ${component} with ${proc_args}")
            message(STATUS "[dependencies] patching in directory ${boost_SOURCE_DIR}/boost/${component}")
            execute_process(COMMAND ${proc_args} WORKING_DIRECTORY "${boost_SOURCE_DIR}/boost/${component}" RESULT_VARIABLE res)
            if (res)
                message(WARNING "[dependencies] failed to patch boost component ${component} with ${proc_args} in directory ${boost_SOURCE_DIR}/boost/${component}")
            endif()
        endforeach()
        set(boost_PATCHES_APPLIED "${patches}" CACHE INTERNAL "patches applied to boost")
    endif()

    #
    # bootstrap
    #

    set(bootstrap_COMMAND "./bootstrap.sh")
    if (NOT boost_BOOTSTRAPPED STREQUAL "${bootstrap_COMMAND}")
        message("[boost] bootstrapping at ${boost_SOURCE_DIR} with ${bootstrap_COMMAND}")

        set(boost_BUILT "" CACHE INTERNAL "")

        execute_process(
                COMMAND ${bootstrap_COMMAND}
                WORKING_DIRECTORY ${boost_SOURCE_DIR}
                RESULT_VARIABLE boost_BOOTSTRAP_ERROR)
        if (NOT boost_BOOTSTRAP_ERROR)
            set(boost_BOOTSTRAPPED "${bootstrap_COMMAND}" CACHE INTERNAL "boost has been bootstrapped")
        else()
            message(FATAL_ERROR "cannot bootstrap boost, error code: ${boost_BOOTSTRAP_ERROR}")
        endif()
    endif ()

    #
    # build step
    #

    include(ProcessorCount)
    ProcessorCount(processors)
    set(b2_args
            "variant=release"
            "link=static"
            "threading=multi")
    BoostDeduceToolSet(toolset)
    list(APPEND b2_args "toolset=${toolset}")
    BoostDeduceCXXVersion(${boost_VERSION} cxxstd)
    list(APPEND b2_args "cxxstd=${cxxstd}")
    if (CMAKE_CXX_FLAGS)
        string(REGEX REPLACE "-std=[^ \t\r\n$]*" "" flags "${CMAKE_CXX_FLAGS}")
        string(STRIP "${flags}" flags)
        if (NOT flags STREQUAL "")
            list(APPEND b2_args "cxxflags=${flags}")
        endif()
    endif()
    if (CMAKE_C_FLAGS)
        list(APPEND b2_args "cxxflags=${CMAKE_C_FLAGS}")
    endif()
    if (CMAKE_EXE_LINKER_FLAGS)
        list(APPEND b2_args "linkflags=${CMAKE_EXE_LINKER_FLAGS}")
    endif()
    list(APPEND b2_args
            "--build-dir=${boost_BINARY_DIR}/build"
            "--stage-dir=${boost_BINARY_DIR}/stage"
            "--prefix=${boost_PREFIX}"
            "-j${processors}")
    foreach(comp IN LISTS boost_COMPONENTS)
        list(APPEND b2_args "--with-${comp}")
    endforeach()
    list(APPEND b2_args
            "stage"
            "install")

    if(NOT boost_BUILT STREQUAL "${b2_args}")
        ListToString(b2_args args_str)
        message(STATUS "[dependencies] building boost with ${args_str}")

        execute_process(
                COMMAND "./b2" "--reconfigure" ${b2_args}
                WORKING_DIRECTORY ${boost_SOURCE_DIR}
                OUTPUT_VARIABLE boost_OUT
                ERROR_VARIABLE boost_ERROR
                RESULT_VARIABLE boost_BUILD_ERROR)
        if (NOT boost_BUILD_ERROR)
            set(boost_BUILT "${b2_args}" CACHE INTERNAL "components built for boost")
            message(STATUS "[dependencies] Boost build success: ${boost_OUT}")
        else()
            message(STATUS "[dependencies] Boost build output:\n ${boost_OUT}")
            message(STATUS "[dependencies] Boost build error:\n ${boost_ERROR}")
            message(FATAL_ERROR "[dependencies] boost build failed")
        endif()
    else()
        message(STATUS "[dependencies] Using cached boost: ${b2_args}")
    endif()

    set(BOOST_ROOT "${boost_PREFIX}")
    set(BOOST_ROOT "${BOOST_ROOT}" PARENT_SCOPE)
    message(STATUS "[dependencies] BOOST_ROOT = ${BOOST_ROOT}")

endfunction()
