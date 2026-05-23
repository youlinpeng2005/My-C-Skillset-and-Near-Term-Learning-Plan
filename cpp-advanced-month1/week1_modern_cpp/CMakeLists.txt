cmake_minimum_required(VERSION 3.16)
project(Week1_ModernCpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译警告开启（养成好习惯）
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 每个 Day 单独编译为可执行文件
foreach(day IN ITEMS
    day1_smart_ptr
    day2_raii
    day3_move_semantics
    day4_lambda_function
    day5_stl_lru
    day6_template_queue)

    add_executable(${day} src/${day}.cpp)
    target_compile_features(${day} PRIVATE cxx_std_17)

    # Linux 下需要链接 pthread
    if(UNIX)
        target_link_libraries(${day} pthread)
    endif()
endforeach()
