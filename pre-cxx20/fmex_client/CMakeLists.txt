project(pre_cxx20_fmex_client)

add_executable(pre_cxx20_fmex_client main.cpp)
target_link_libraries(pre_cxx20_fmex_client
    PUBLIC
        Boost::system
        fmt::fmt
        nlohmann_json::nlohmann_json
        OpenSSL::SSL OpenSSL::Crypto
        Threads::Threads)
