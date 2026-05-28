cmake_minimum_required(VERSION 3.16)
project(day5_mysql_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 查找 MariaDB / MySQL 客户端库 ──────────────────────────────
# 优先用 pkg-config（libmariadb-dev 安装后自动注册）
find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED libmariadb)

# ── 目标 ───────────────────────────────────────────────────────
add_executable(day5_mysql_demo day5_mysql_demo.cpp)

target_include_directories(day5_mysql_demo PRIVATE
    ${MARIADB_INCLUDE_DIRS}
)

target_link_libraries(day5_mysql_demo PRIVATE
    ${MARIADB_LIBRARIES}
)

target_compile_options(day5_mysql_demo PRIVATE
    ${MARIADB_CFLAGS_OTHER}
    -Wall
    -Wextra
)
