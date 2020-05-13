cmake_minimum_required(VERSION 3.16)

include(cmake/RequireBoost.cmake)

project(WebsocketDemo)

if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif ()

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_BINARY_DIR}/dependencies/install)
if (NOT DEFINED BOOST_ROOT)
    RequireBoost(PREFIX ${CMAKE_CURRENT_BINARY_DIR}/dependencies/install VERSION 1.73.0 COMPONENTS system)
endif ()

add_compile_definitions(
        "BOOST_ASIO_NO_DEPRECATED=1"
        "BOOST_ASIO_DISABLE_CONCEPTS=1")

find_package(Boost REQUIRED system)
find_package(Threads)
link_libraries(Threads::Threads)


add_subdirectory(pre-cxx20)
if (CMAKE_CXX_STANDARD GREATER_EQUAL 20)
    add_subdirectory(cxx20)
endif ()

