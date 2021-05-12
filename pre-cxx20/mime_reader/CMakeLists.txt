project(mime_reader)

add_executable(mime_reader main.cpp config.hpp)
target_link_libraries(mime_reader beast_fun_times_config Boost::boost Boost::system Threads::Threads spdlog::spdlog)
