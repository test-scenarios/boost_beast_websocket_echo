project(pre_cxx20_fmex_client)

file(GLOB_RECURSE src_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS "*.cpp" "*.hpp")
add_executable(pre_cxx20_fmex_client ${src_files})
target_link_libraries(pre_cxx20_fmex_client
    PUBLIC
        Boost::system
        fmt::fmt
        nlohmann_json::nlohmann_json
        OpenSSL::SSL OpenSSL::Crypto
        Threads::Threads)
