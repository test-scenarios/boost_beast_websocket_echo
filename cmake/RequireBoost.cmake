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

function(BoostSource boost_VERSION urlout hashout)
    if (boost_VERSION STREQUAL "1.73.0")
        set(${urlout} "https://dl.bintray.com/boostorg/release/1.73.0/source/boost_1_73_0.tar.bz2" PARENT_SCOPE)
        set(${hashout} "SHA256=4eb3b8d442b426dc35346235c8733b5ae35ba431690e38c6a8263dce9fcbb402" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "unknown hash for version ${boost_VERSION}")
    endif()
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
    BoostSource("${boost_VERSION}" boost_URL boost_HASH)
    FetchContent_Declare(boost
            URL "${boost_URL}"
            URL_HASH "${boost_HASH}")
    FetchContent_GetProperties(boost)
    if (NOT boost_POPULATED)
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
        list(APPEND b2_args "cxxflags=${flags}")
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
                RESULT_VARIABLE boost_BUILD_ERROR)
        if (NOT boost_BUILD_ERROR)
            set(boost_BUILT "${b2_args}" CACHE INTERNAL "components built for boost")
        else()
            message(FATAL_ERROR "[dependencies] boost build failed")
        endif()
    endif()

    set(BOOST_ROOT "${boost_BINARY_DIR}/install")
    set(BOOST_ROOT "${BOOST_ROOT}" PARENT_SCOPE)
    message(STATUS "[dependencies] BOOST_ROOT = ${BOOST_ROOT}")

endfunction()
