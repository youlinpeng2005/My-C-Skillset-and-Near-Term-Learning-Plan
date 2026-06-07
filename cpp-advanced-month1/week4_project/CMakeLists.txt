cmake_minimum_required(VERSION 3.16)
project(Week4_ChatServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    add_compile_options(-Wall -Wextra -O2)

    # ── 找 MariaDB/MySQL 客户端库 ──────────────────────────────────
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MARIADB REQUIRED libmariadb)

    # ── 连接池静态库（复用 month2/week1 的实现）─────────────────────
    set(CONN_POOL_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-advanced-month2/week1_mysql/src)

    add_library(conn_pool STATIC
        ${CONN_POOL_DIR}/day6_conn_pool.cpp
    )
    target_include_directories(conn_pool PUBLIC
        ${CONN_POOL_DIR}
        ${MARIADB_INCLUDE_DIRS}
    )
    target_link_libraries(conn_pool ${MARIADB_LIBRARIES})

    # ── 聊天服务器 ────────────────────────────────────────────────
    add_executable(chat_server
        src/server/chat_server.cpp
    )
    target_include_directories(chat_server PRIVATE src ${CONN_POOL_DIR})
    target_link_libraries(chat_server conn_pool pthread ${MARIADB_LIBRARIES})
    target_compile_features(chat_server PRIVATE cxx_std_17)

    # ── 压测客户端 ────────────────────────────────────────────────
    add_executable(stress_client
        src/server/stress_client.cpp
    )
    target_link_libraries(stress_client pthread)
    target_compile_features(stress_client PRIVATE cxx_std_17)

else()
    message(WARNING "Week4 需要 Linux（epoll）环境")
endif()
