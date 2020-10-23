project(mime_reader)

add_executable(mime_reader main.cpp mime_reader.cpp mime_reader.hpp config.hpp)
target_link_libraries(mime_reader Boost::boost Boost::system Threads::Threads spdlog::spdlog)
