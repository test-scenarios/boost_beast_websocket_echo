
project(Cxx20)

add_executable(cxx20 main.cpp app.cpp connection.cpp server.cpp)
target_link_libraries(cxx20 PUBLIC Boost::system beast_fun_times::util)

