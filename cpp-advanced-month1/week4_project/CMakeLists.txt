cmake_minimum_required(VERSION 3.16)
project(Week4_ChatServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    add_compile_options(-Wall -Wextra -O2)

    # 聊天服务器
    add_executable(chat_server
        src/server/chat_server.cpp
    )
    target_include_directories(chat_server PRIVATE src)
    target_link_libraries(chat_server pthread)
    target_compile_features(chat_server PRIVATE cxx_std_17)

    # 压测客户端
    add_executable(stress_client
        src/server/stress_client.cpp
    )
    target_link_libraries(stress_client pthread)
    target_compile_features(stress_client PRIVATE cxx_std_17)

else()
    message(WARNING "Week4 需要 Linux（epoll）环境")
endif()
