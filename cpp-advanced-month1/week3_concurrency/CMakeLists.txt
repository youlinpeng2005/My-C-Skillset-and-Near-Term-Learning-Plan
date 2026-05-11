cmake_minimum_required(VERSION 3.16)
project(Week3_Concurrency)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX)
    add_compile_options(-Wall -Wextra)
    set(THREAD_LIB pthread)
else()
    add_compile_options(/W4)
    set(THREAD_LIB "")
endif()

# day15 和 day17 跨平台（无 epoll 依赖）
add_executable(day15_producer_consumer src/day15_producer_consumer.cpp)
add_executable(day17_thread_pool       src/day17_thread_pool.cpp)
add_executable(day19_atomic_lockfree   src/day19_atomic_lockfree.cpp)

foreach(target IN ITEMS day15_producer_consumer day17_thread_pool day19_atomic_lockfree)
    target_compile_features(${target} PRIVATE cxx_std_17)
    if(UNIX)
        target_link_libraries(${target} ${THREAD_LIB})
    endif()
endforeach()

# day20 依赖 epoll（Linux only）
if(UNIX AND NOT APPLE)
    add_executable(day20_reactor_threadpool src/day20_reactor_threadpool.cpp)
    target_compile_features(day20_reactor_threadpool PRIVATE cxx_std_17)
    target_link_libraries(day20_reactor_threadpool pthread)
endif()
