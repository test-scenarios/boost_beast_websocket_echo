cmake_minimum_required(VERSION 3.16)

if (NOT DEFINED FUN_TIMES_BOOST_VERSION)
    set(FUN_TIMES_BOOST_VERSION "1.73.0" CACHE STRING "Boost Version")
endif ()

include(FetchContent)
include(cmake/RequireBoost.cmake)
include(cmake/BuildCMakeContent.cmake)

project(beast_fun_times)

option(ENABLE_TESTING "" ON)

if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif ()

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_BINARY_DIR}/dependencies/install)
if (NOT DEFINED BOOST_ROOT)
    #    RequireBoost(PREFIX ${CMAKE_CURRENT_BINARY_DIR}/dependencies/install VERSION 1.73.0 COMPONENTS system)
    RequireBoost(PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dependencies/install/boost-${FUN_TIMES_BOOST_VERSION}" VERSION "${FUN_TIMES_BOOST_VERSION}"
            COMPONENTS
            system
            thread)
else()
    message(STATUS "[dependencies] BOOST_ROOT=${BOOST_ROOT}")
endif ()

if (ENABLE_TESTING)
    FetchContent_Declare(catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v2.12.1)
    BuildCmakeContent(catch2 Catch2 CMAKE_ARGS
            "-DCATCH_BUILD_TESTING=OFF"
            "-DCATCH_INSTALL_DOCS=OFF"
            "-DCATCH_INSTALL_HELPERS=OFF")

endif ()

add_compile_definitions(
        "BOOST_ASIO_NO_DEPRECATED=1"
        "BOOST_ASIO_DISABLE_CONCEPTS=1")

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads)
find_package(Catch2)

add_subdirectory(lib)

add_subdirectory(pre-cxx20)
if (CMAKE_CXX_STANDARD GREATER_EQUAL 20)
    add_subdirectory(cxx20)
endif ()

