project(beast_fun_times_config)

get_filename_component(libs_src_dir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(libs_bin_dir ${CMAKE_CURRENT_BINARY_DIR} DIRECTORY)

file(GLOB_RECURSE cpp_files
        LIST_DIRECTORIES false CONFIGURE_DEPENDS
        "*.cpp")
set(spec_cpp_files ${cpp_files})
list(FILTER spec_cpp_files INCLUDE REGEX "^.*\\.spec\\.cpp$")
list(FILTER spec_cpp_files EXCLUDE REGEX "^.*/main\\.spec\\.cpp$")
list(FILTER cpp_files EXCLUDE REGEX "^.*.spec.cpp$")
file(GLOB_RECURSE hpp_files
        LIST_DIRECTORIES false CONFIGURE_DEPENDS
        "*.hpp")

set(maybe_interface)
if (NOT "${cpp_files}" STREQUAL "")
    add_library(${PROJECT_NAME} ${cpp_files} ${hpp_files})
    set(maybe_interface PUBLIC)
else ()
    set(maybe_interface INTERFACE)
    add_library(${PROJECT_NAME} INTERFACE)
endif ()
message(STATUS "[lib] ${PROJECT_NAME} is ${maybe_interface}")

target_include_directories(${PROJECT_NAME} ${maybe_interface}
        $<BUILD_INTERFACE:${libs_src_dir}>
        $<BUILD_INTERFACE:${libs_bin_dir}>)

target_link_libraries(${PROJECT_NAME} ${maybe_interface} Boost::system Threads::Threads)
target_compile_definitions(${PROJECT_NAME} ${maybe_interface}
        "BOOST_ASIO_NO_DEPRECATED=1"
        "BOOST_ASIO_DISABLE_CONCEPTS=1"
        "BOOST_ASIO_NO_TS_EXECUTORS=1")

if (${ENABLE_TESTING} AND NOT "${spec_cpp_files}" STREQUAL "")
    add_executable("test_${PROJECT_NAME}" main.spec.cpp ${spec_cpp_files})
    target_link_libraries("test_${PROJECT_NAME}" PUBLIC ${PROJECT_NAME} Catch2::Catch2)
endif ()


