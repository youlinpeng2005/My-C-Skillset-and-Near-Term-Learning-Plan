cmake_minimum_required(VERSION 3.16)
project(week1_mysql LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 查找 MariaDB / MySQL 客户端库 ──────────────────────────────
# 优先用 pkg-config（libmariadb-dev 安装后自动注册）
find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED libmariadb)

# ── 公共编译选项 ────────────────────────────────────────────────
set(COMMON_COMPILE_OPTIONS ${MARIADB_CFLAGS_OTHER} -Wall -Wextra)

# ── Day5：C++ 接入 MySQL（裸客户端版） ─────────────────────────
add_executable(day5_mysql_demo day5_mysql_demo.cpp)

target_include_directories(day5_mysql_demo PRIVATE
    ${MARIADB_INCLUDE_DIRS}
)

target_link_libraries(day5_mysql_demo PRIVATE
    ${MARIADB_LIBRARIES}
)

target_compile_options(day5_mysql_demo PRIVATE
    ${COMMON_COMPILE_OPTIONS}
)

# ── Day6：MySQL 连接池 ──────────────────────────────────────────
# 连接池库（复用于测试 + 未来 chat_server）
add_library(conn_pool STATIC
    day6_conn_pool.cpp
)

target_include_directories(conn_pool PUBLIC
    ${MARIADB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}   # 让 #include "connection.h" 可以找到
)

target_link_libraries(conn_pool PUBLIC
    ${MARIADB_LIBRARIES}
)

target_compile_options(conn_pool PRIVATE
    ${COMMON_COMPILE_OPTIONS}
)

# 连接池测试可执行文件
add_executable(day6_conn_pool_test day6_conn_pool_test.cpp)

target_link_libraries(day6_conn_pool_test PRIVATE
    conn_pool
    pthread   # std::thread 在 Linux 上需要链接 pthread
)

target_compile_options(day6_conn_pool_test PRIVATE
    ${COMMON_COMPILE_OPTIONS}
)
