cmake_minimum_required(VERSION 3.16)
project(Week2_Epoll)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# epoll 只在 Linux 下可用
if(UNIX AND NOT APPLE)
    add_compile_options(-Wall -Wextra)

    add_executable(day9_echo_server  src/day9_epoll_echo_server.cpp)
    add_executable(day10_et_server   src/day10_et_mode.cpp)
    add_executable(day11_protocol    src/day11_protocol.cpp)
    add_executable(day12_reactor     src/day12_reactor.cpp)
    add_executable(day13_chatroom    src/day13_chatroom.cpp)

    foreach(target IN ITEMS
        day9_echo_server day10_et_server day11_protocol
        day12_reactor day13_chatroom)
        target_compile_features(${target} PRIVATE cxx_std_17)
    endforeach()
else()
    message(WARNING "Week2 需要在 Linux（或 WSL）下编译，epoll 不支持 Windows/macOS")
endif()
