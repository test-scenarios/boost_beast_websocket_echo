
project(pre_cxx20_echo_server)

add_executable(pre_cxx20_echo_server main.cpp app.cpp connection.cpp server.cpp)
target_link_libraries(pre_cxx20_echo_server PUBLIC
        beast_fun_times_config Boost::system Threads::Threads)

