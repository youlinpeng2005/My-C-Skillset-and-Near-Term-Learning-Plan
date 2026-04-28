# 第一周复盘（4月25日）

## 本周完成情况检查

完成后在 [ ] 中打 x

- [ ] Day1: 实现了 MySharedPtr，理解引用计数机制
- [ ] Day2: 用 RAII 封装了 fd，改写了连接管理
- [ ] Day3: 理解了 move 语义，实现了 Buffer 类
- [ ] Day4: 实现了 EventHandler 回调注册器
- [ ] Day5: 手写了 LRU 缓存并通过测试
- [ ] Day6: 实现了 BlockingQueue<T>，生产者消费者测试通过

---

## 自测问题答案（自己先回答，再对照）

### Q1: shared_ptr 线程安全吗？

**引用计数操作**：线程安全（使用 atomic 操作）  
**指向对象的访问**：不安全（需要额外同步）  
**shared_ptr 本身的拷贝/赋值**：不安全（多线程操作同一个 shared_ptr 对象需要加锁）

### Q2: move 后原对象状态是什么？

处于"有效但未指定"状态（valid but unspecified state）：
- 对象仍然有效，析构函数可以安全调用
- 但不应该再使用其值（通常为空/零）
- 例：string 被 move 后变为空字符串，unique_ptr 变为 nullptr

### Q3: 为什么移动构造要加 noexcept？

`std::vector` 扩容时会选择：
- 如果移动构造是 noexcept：使用移动（高效）
- 如果移动构造可能抛异常：使用拷贝（保证异常安全）

所以：**移动构造不加 noexcept，vector 扩容时不会用移动，性能损失！**

### Q4: 条件变量为什么用 while（predicate）而非 if？

**虚假唤醒（spurious wakeup）**：线程可能在没有 notify 的情况下被唤醒  
wait(lock, predicate) 等价于：
```cpp
while (!predicate()) {
    wait(lock);
}
```
如果用 if，虚假唤醒后会跳过检查直接继续，导致逻辑错误。

### Q5: BlockingQueue::stop() 为什么 notify_all？

因为可能有多个线程在等待，notify_one 只唤醒一个，其他线程会永久阻塞。  
stop 是广播事件，必须通知所有等待者。

---

## 代码提交清单

```bash
git init  # 如果还没初始化
git add week1_modern_cpp/
git commit -m "Week1: Modern C++ - smart ptr, RAII, move, lambda, LRU, BlockingQueue"
```

---

## 下周预告（4月26日）

**第二周：epoll 与 IO 多路复用**

- Day8: 5种IO模型全景（阻塞/非阻塞/多路复用/信号驱动/异步IO）
- Day9: epoll 底层原理（红黑树+就绪链表+回调）+ 最小 echo 服务器
- Day10: ET 模式实战（非阻塞 + 循环读）
- Day11: TCP 粘包/拆包 + 自定义协议设计
- Day12: Reactor 模式（EventLoop + Channel + Acceptor）
- Day13: 单线程聊天室（多客户端 epoll）
- Day14: 复盘

**预习建议**：今晚快速浏览《Linux高性能服务器编程》第9章 IO复用
