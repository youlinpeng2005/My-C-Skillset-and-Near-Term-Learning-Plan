# Week4 工程级项目：学习流程与每日任务

> **时间**：5月19日（今天）起，目标在2天内完成可写进简历的工程级聊天服务器  
> **当前状态**：框架已建好，代码骨架已存在，需要你逐模块读懂 → 跑通 → 扩展

---

## 现有代码盘点（先搞清楚你有什么）

```
week4_project/
├── src/
│   ├── base/
│   │   ├── logger.h        ✅ 已完成 — 异步日志（BlockingQueue + 日志线程）
│   │   └── thread_pool.h   ✅ 已完成 — 工程版线程池（复用 day17）
│   ├── net/
│   │   └── protocol.h      ✅ 已完成 — 自定义协议 + PacketParser 粘包处理
│   └── server/
│       ├── chat_server.cpp  ✅ 已完成 — epoll + 线程池 + 协议 + 日志 + 心跳
│       └── stress_client.cpp✅ 已完成 — 多线程压测客户端
└── CMakeLists.txt           ✅ 已完成
```

**结论**：代码框架已完整，不需要从零写。你的任务是：**读懂每个模块 → 在 WSL 里跑起来 → 做扩展和压测 → 写总结**。

---

## Day 1（今天 5/19）— 读懂 + 跑通

### 上午（约 2 小时）：逐模块精读代码

按以下顺序读，每个文件都要能回答后面的问题：

#### 第一步：读 `base/logger.h`（20分钟）

**读完要能回答：**
- `AsyncLogger` 为什么不直接在 `log()` 函数里写文件，而是要用队列 + 后台线程？
- `stopped_` 用了 `std::atomic<bool>`，但 `queue_` 的操作还是用了 `mutex_`，为什么？
- 析构函数里为什么先设 `stopped_ = true`，再 `notify_all()`，最后才 `join()`？如果顺序错了会发生什么？

**动手验证**（在文件末尾加一个 `main` 测试，跑完删掉）：
```cpp
// 临时测试，跑完删掉
int main() {
    auto& logger = get_logger();
    LOG_INFO("hello from logger");
    LOG_WARN("this is a warning");
    LOG_ERROR("simulated error");
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 等待日志写完
}
```

---

#### 第二步：读 `base/thread_pool.h`（20分钟）

**对比 day17**：这个版本和你自己写的 day17 有什么区别？把差异列出来（比如 `submit` 返回 `future`、`submit_void` 简化版本）。

**读完要能回答：**
- `submit()` 用了 `std::packaged_task` + `std::future`，什么场景下需要拿到返回值？
- `submit_void()` 没有 future，适合什么场景？`chat_server.cpp` 里用的是哪个？
- 为什么 `stopped_` 检查在 `worker_loop` 里用的是 `stopped_ && tasks_.empty()`，而不是直接 `stopped_`？

---

#### 第三步：读 `net/protocol.h`（20分钟）

**对比 day11**：粘包处理器 `PacketParser` 和你 day11 写的有什么不同？

**读完要能回答：**
- `MsgHeader` 为什么要 `#pragma pack(push, 1)`？不加会怎样？
- `htonl` / `ntohl` 是什么？为什么网络传输必须转换字节序？
- `PacketParser::try_parse()` 调用一次只解析一条消息，如果一次 `recv` 收到了3条消息，怎么处理？（看 `chat_server.cpp` 里是否有循环调用）

---

#### 第四步：读 `server/chat_server.cpp`（60分钟，核心）

这是整个项目的主文件，要仔细读。按以下思路梳理：

**数据流（最重要）**：一条消息从客户端发出到广播给其他人，经过了哪几步？
```
客户端 send()
  → epoll_wait 返回 EPOLLIN
  → pool_.submit_void([this, fd]{ handleMessage(fd); })
  → 线程池工作线程执行 handleMessage()
  → recv() 拿到数据
  → parser.append() 加入缓冲区
  → parser.try_parse() 解析出 Message
  → switch(msg.type) 分发处理
  → broadcast() 广播给其他连接
```

**读完要能回答：**
- `handleMessage()` 被提交到线程池执行，而 `handleAccept()`、`removeConnection()` 在主线程执行，这样设计有什么并发风险？（提示：看 `conns_` 的 mutex 使用）
- `send_message()` 里有一行注释"生产环境应加 per-fd 写锁"，为什么？如果两个线程同时给同一个 fd 发消息会怎样？
- `epoll_wait` 的超时设为 5000ms，为什么不设为 -1（永久等待）？

---

### 下午（约 2 小时）：在 WSL 里跑通

#### 步骤 1：进入 WSL，编译项目

```bash
# 进入 WSL
wsl

# 进到项目目录（根据你的 WSL 挂载路径调整）
cd /mnt/c/Users/20834/Desktop/个人进阶/My-C-Skillset-and-Near-Term-Learning-Plan/cpp-advanced-month1/week4_project

# 创建构建目录，编译
mkdir -p build && cd build
cmake .. && make -j$(nproc)

# 检查产物
ls -la
# 应该看到 chat_server 和 stress_client
```

#### 步骤 2：运行服务器

```bash
# 终端1：启动服务器
./chat_server
# 预期输出：
# [时间] [INFO ] 服务器启动，端口=8893，工作线程=N
# [时间] [INFO ] 进入事件循环
```

#### 步骤 3：用 telnet 手动测试（验证基本功能）

```bash
# 终端2：连接测试
telnet 127.0.0.1 8893
# 连上后服务器会发欢迎消息（虽然是二进制协议，telnet 会显示乱码，但不报错就说明连接成功）

# 更好的测试方式：用 nc（netcat）发自定义消息
# 或者直接用 stress_client
```

#### 步骤 4：压测（核心验证）

```bash
# 终端2（保持服务器运行）：运行压测
./stress_client 50 100   # 50个并发连接，每个发100条消息

# 预期输出：
# 压测开始：50 个并发连接，每连接 100 条消息
# 压测结束：
#   成功连接: 50/50
#   失败连接: 0
#   总消息数: 5000
#   总耗时:   XXXX ms
#   QPS:      XXXX msg/s
```

#### 步骤 5：记录结果

把压测数字记下来，这是你简历上能写的数据：
- 并发连接数
- 总 QPS
- 是否有连接失败

---

### 晚上（1小时）：整理第一天产出

在这个文件（`WEEK4_LEARNING_PLAN.md`）下方的"学习记录"区域，填写：
- 三个模块各自的关键问题你的答案
- 压测数据
- 发现的 bug 或疑问

---

## Day 2（5/20）— 扩展 + 总结

### 上午（2小时）：选1个扩展功能实现

根据时间选做，**优先选第1个**：

#### 扩展 A（推荐）：修复多线程写入竞态问题

当前代码在 `send_message()` 里有注释：
> "生产环境应加 per-fd 写锁"

**任务**：给每个 `Connection` 加一个 `std::mutex write_mutex`，在 `send_message()` 里加锁。

```cpp
// 修改 Connection 结构体
struct Connection {
    int fd;
    std::string ip;
    std::string nickname;
    TimePoint last_active;
    PacketParser parser;
    std::mutex write_mutex;  // 新增：per-fd 写锁
    // ...
};

// 修改 send_message()
void send_message(int fd, const Message& msg) {
    auto buf = msg.serialize();
    std::lock_guard<std::mutex> lock(/* 拿到对应 conn 的 write_mutex */);
    send(fd, buf.data(), buf.size(), MSG_NOSIGNAL);
}
```

**难点**：`send_message()` 现在需要 `Connection` 对象而不只是 `fd`，需要改一下函数签名。

#### 扩展 B：支持房间功能（/join room_name）

解析聊天消息，如果以 `/join ` 开头则切换房间，广播时只发给同房间的人。

#### 扩展 C：消息持久化（写到文件）

在 broadcast 时，用 `logger` 记录消息到文件。（这个最简单，但也最没技术含量）

---

### 下午（2小时）：加大压测 + 性能分析

```bash
# 逐步加大并发，找到服务器瓶颈
./stress_client 100 200   # 100连接
./stress_client 200 100   # 200连接
./stress_client 500 50    # 500连接

# 同时观察服务器的 CPU 使用情况
# 另开一个终端：
watch -n 1 'ps aux | grep chat_server'
# 或者：
top -p $(pgrep chat_server)
```

**分析瓶颈在哪里**：
- 如果 CPU 使用率接近 100%：计算密集，广播 O(N) 成为瓶颈
- 如果连接失败增多：文件描述符限制（`ulimit -n` 查看）
- 如果 QPS 不随线程数增加而线性增加：锁竞争是瓶颈

---

### 晚上（1小时）：完成整个月的总结

在 `week4_project/` 下创建 `WEEK4_SUMMARY.md`，写以下内容：

1. **项目架构图**（用 ASCII 画出各模块关系）
2. **压测数据**（不同并发数下的 QPS）
3. **遇到的 bug 和解决方法**
4. **如果再做一次，会怎么设计**（可以提反思性的问题，如"为什么不用 one-thread-per-connection？"）
5. **可写进简历的描述**（30字以内，突出技术关键词）

简历描述示例：
> 基于 epoll + 线程池实现 C++ 高性能聊天服务器，支持自定义二进制协议、粘包处理、心跳检测，实测 200 并发 QPS > XXXX msg/s

最后提交 git，完成整个一个月的计划。

---

## 最终 git 提交清单

```bash
# Day1 结束后提交
git add cpp-advanced-month1/week4_project/
git commit -m "feat(week4): run chat_server in WSL, stress test baseline XXXX QPS"

# Day2 扩展后提交
git add .
git commit -m "feat(week4): add per-fd write mutex, stress test 200 concurrent XXXX QPS"

# 写完总结后提交
git add cpp-advanced-month1/week4_project/WEEK4_SUMMARY.md
git add cpp-advanced-month1/README.md   # 更新进度追踪表
git commit -m "docs(week4): add week4 summary, complete month1 learning plan"
```

---

## 常见问题排查

| 问题 | 原因 | 解决方法 |
|------|------|---------|
| `cmake ..` 报 "Week4 需要 Linux 环境" | 在 Windows 下编译 | 切换到 WSL 再编译 |
| `epoll_create1: Function not implemented` | WSL1 不支持 epoll | 升级到 WSL2：`wsl --set-version Ubuntu 2` |
| `bind: Address already in use` | 端口 8893 被占用 | `fuser -k 8893/tcp` 或等 30 秒 TIME_WAIT |
| 压测连接失败 | 文件描述符限制 | `ulimit -n 65536` 临时扩大 |
| QPS 很低（< 1000） | sleep_for 10ms 限制了发送速度 | 修改 `stress_client.cpp` 里的 sleep 时间 |

---

## 学习记录区（每天填写）

### Day 1 记录

**logger.h 三问答案：**
- Q1（为什么用队列而不直接写文件）：
- Q2（atomic vs mutex）：
- Q3（析构顺序）：

**压测基线数据：**
- 并发连接数：
- QPS：
- 连接失败：

**发现的问题/疑问：**
- 

---

### Day 2 记录

**选择的扩展功能：**

**扩展过程遇到的问题：**
- 

**最终压测数据（200并发）：**
- QPS：
- 与 Day1 相比提升/下降：

**整个月学习最大的收获：**
- 
