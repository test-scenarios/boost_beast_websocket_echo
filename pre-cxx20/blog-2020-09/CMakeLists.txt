project(blog_2020_09)
if (FUN_TIMES_BOOST_VERSION VERSION_GREATER "1.71.0")

file(GLOB_RECURSE src_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS "*.cpp" "*.hpp")
add_executable(blog_2020_09 ${src_files})
target_link_libraries(blog_2020_09
        PUBLIC
        Boost::system
        fmt::fmt
        Boost::json
        OpenSSL::SSL OpenSSL::Crypto
        Threads::Threads)
endif()