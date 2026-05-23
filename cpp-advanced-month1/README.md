# C++ 进阶第一个月学习代码库

**时间**：2026年4月19日 ~ 5月18日  
**目标**：从"会写功能"到"能做系统"，掌握并发编程、epoll、现代C++，完成工程级聊天服务器

---

## 目录结构

```
cpp-advanced-month1/
├── week1_modern_cpp/       # 第一周：现代C++强化（4/19~4/25）
│   ├── src/
│   │   ├── day1_smart_ptr.cpp      # 智能指针原理 + 手写 MySharedPtr
│   │   ├── day2_raii.cpp           # RAII思想 + unique_ptr实战
│   │   ├── day3_move_semantics.cpp # move语义 + 右值引用
│   │   ├── day4_lambda_function.cpp# lambda + std::function + 回调注册器
│   │   ├── day5_stl_lru.cpp        # STL高级 + 手写LRU缓存
│   │   ├── day6_template_queue.cpp # 模板编程 + BlockingQueue<T>
│   │   └── day7_review.md          # 第一周复盘 + 自测问题答案
│   └── CMakeLists.txt
│
├── week2_epoll/            # 第二周：epoll与IO多路复用（4/26~5/2）
│   ├── src/
│   │   ├── day8_io_models.md           # 5种IO模型全景笔记
│   │   ├── day9_epoll_echo_server.cpp  # 最小化epoll echo服务器（LT模式）
│   │   ├── day10_et_mode.cpp           # ET模式实战 + 循环读
│   │   ├── day11_protocol.cpp          # TCP粘包 + 自定义协议设计
│   │   ├── day12_reactor.cpp           # Reactor模式（EventLoop+Channel+Acceptor）
│   │   └── day13_chatroom.cpp          # 单线程多客户端聊天室（基线版本）
│   └── CMakeLists.txt
│
├── week3_concurrency/      # 第三周：并发编程（5/3~5/9）
│   ├── src/
│   │   ├── day15_producer_consumer.cpp # 生产者-消费者 + 死锁预防
│   │   ├── day17_thread_pool.cpp       # 手写线程池（本月最重要代码）
│   │   ├── day19_atomic_lockfree.cpp   # atomic + CAS + 内存顺序
│   │   └── day20_reactor_threadpool.cpp# Reactor + 线程池结合
│   └── CMakeLists.txt
│
└── week4_project/          # 第四周：工程级项目（5/10~5/18）
    ├── src/
    │   ├── base/
    │   │   ├── logger.h        # 异步日志模块
    │   │   └── thread_pool.h   # 工程版线程池
    │   ├── net/
    │   │   └── protocol.h      # 自定义协议 + 粘包处理器
    │   └── server/
    │       ├── chat_server.cpp  # 工程级聊天服务器（epoll+线程池+协议+日志+心跳）
    │       └── stress_client.cpp# 压测客户端
    └── CMakeLists.txt
```

---

## 技术栈

| 模块     | 技术                                          |
|--------|-----------------------------------------------|
| 网络层   | epoll（ET/LT）+ 非阻塞IO + Reactor模式         |
| 并发层   | std::thread + mutex + condition_variable + 线程池 |
| 现代C++ | shared_ptr / unique_ptr / move / lambda / 模板  |
| 协议层   | 长度前缀自定义协议 + 粘包处理                    |
| 日志     | 异步日志（BlockingQueue + 日志线程）              |

---

## 编译方法

### Week1（跨平台，Windows/Linux/macOS 均可）

```bash
cd week1_modern_cpp
mkdir build && cd build
cmake .. && cmake --build .
./day1_smart_ptr
./day5_stl_lru
./day6_template_queue
```

### Week2~4（需要 Linux / WSL）

```bash
cd week2_epoll
mkdir build && cd build
cmake .. && make
./day9_echo_server   # 终端1
telnet 127.0.0.1 8888  # 终端2 测试

cd ../../week4_project
mkdir build && cd build
cmake .. && make
./chat_server &
./stress_client 50 100  # 50并发，每连接100条消息
```

---

## 学习进度追踪

| 周   | 计划日期       | 实际执行日期      | 主题              | 状态   |
|-----|------------|--------------|-----------------|------|
| 第1周 | 4/19~4/25  | 4/19~4/25    | 现代C++强化        | ✅ 已完成（day1~day7 全部提交）  |
| 第2周 | 4/26~5/2   | 4/26~5/10    | epoll与IO多路复用   | ✅ 已完成（day8~day13 全部提交）  |
| 第3周 | 5/3~5/9    | 5/11~5/19    | 并发编程+手写线程池  | ✅ 已完成（day15/17/19/20 全部提交，b048050）  |
| 第4周 | 5/10~5/18  | 5/19~5/23    | 工程级项目整合      | ✅ 已完成（chat_server + 压测跑通，补交 commit 于 5/23）  |

> 📌 **完成说明（2026/05/23 收官更新）**：
> - 第2周延期至 5/10 完成，整体落后约一周
> - 第3周（并发编程）5/11~5/19 完成
> - 第4周工程项目 5/19~5/23 完成（代码 + 压测），5/23 当天补提交
> - **教训**：Week4 完成代码后拖了 4 天才 commit，违反"当天写当天交"的规则，month2 必须杜绝

---

## 关键参考资料

- **muduo 源码**：重点看 EventLoop、Channel、ThreadPool
- **《Linux高性能服务器编程》**：游双著，第9章IO复用必读
- **《Effective Modern C++》**：Scott Meyers，条款18-21（智能指针）
- **cppreference.com**：std::shared_ptr / std::thread / std::atomic

---

## 里程碑

- [x] Week1 结束：能写出无内存泄漏的现代C++代码（✅ 2026/04/25 完成）
- [x] Week2 结束：独立实现基于 epoll 的 Reactor 框架（✅ 2026/05/10 完成，延期8天）
- [x] Week3 结束：手写线程池，理解所有同步原语（✅ 2026/05/19 完成）
- [x] Week4 结束：完成可写进简历的工程级聊天服务器（✅ 2026/05/23 完成，代码+压测+QA 全部交付）

---

## 月度收官

- ✅ month1 全部完成，进入月度复盘期（5/23）
- 📅 month2 开工日：**2026/05/25（周一）**，主题：MySQL 工程化 + Redis + chat_server 重构
- 📁 month2 计划入口：[`../cpp-advanced-month2/README.md`](../cpp-advanced-month2/README.md)
