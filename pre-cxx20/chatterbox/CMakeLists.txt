project(pre_cxx20_chatterbox)

add_executable(pre_cxx20_chatterbox main.cpp app.cpp connection.cpp connection_pool.cpp)
target_link_libraries(pre_cxx20_chatterbox PUBLIC beast_fun_times_config Boost::system Boost::thread)
