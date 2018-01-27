cmake_minimum_required (VERSION 3.5)
project (asio_coro)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -fcoroutines-ts")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nodefaultlibs -lc++ -lc++abi -lm -lc -lgcc_s")

add_executable (async_tcp_echo_server async_tcp_echo_server.cpp)
target_link_libraries(async_tcp_echo_server boost_system pthread)
