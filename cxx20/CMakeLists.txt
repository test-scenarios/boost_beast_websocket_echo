
project(PreCxx20)

add_executable(pre-cxx20 main.cpp app.cpp connection.cpp server.cpp)
target_link_libraries(pre-cxx20 PUBLIC Boost::system)

