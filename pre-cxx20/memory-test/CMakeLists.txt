project(memory-test)

link_libraries(Boost::boost Boost::system Threads::Threads)
add_executable(memory-test-server server.cpp)
add_executable(memory-test-client client.cpp)
