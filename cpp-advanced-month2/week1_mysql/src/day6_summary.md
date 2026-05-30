# Day6 — MySQL 连接池实现总结

## 一、为什么需要连接池

每次 `mysql_real_connect()` 需要：
1. TCP 三次握手（~1ms LAN）
2. MySQL 认证握手（client → server → client，再 ~1~5ms）

高并发场景下每个请求建一条新连接，连接建立时间会超过实际查询时间。
连接池把这 N 条连接提前建好，请求来时直接复用，用完归还。

---

## 二、核心设计

### 数据结构

```
ConnectionPool
  pool_: deque<unique_ptr<Connection>>  ← 空闲连接队列
  mutex_                                 ← 保护 pool_
  cond_                                  ← 阻塞等待空闲连接

ConnectionGuard（RAII）
  conn_: Connection*                     ← 裸指针（pool 拥有生命周期）
  pool_: ConnectionPool&                 ← 析构时回调 pool.release()
```

### acquire() 流程

```
acquire()
  │
  ├─ lock(mutex_)
  ├─ cond_.wait(lock, [pool 非空])    ← pool 空时挂起，不占 CPU
  ├─ 取 pool_.front()，pop_front()
  ├─ unique_ptr::release() 交出裸指针
  └─ 返回 ConnectionGuard
```

### release() 流程

```
release(conn*)
  │
  ├─ lock_guard lock(mutex_)
  ├─ pool_.push_back(unique_ptr(conn))  ← 连接归还队列
  └─ cond_.notify_one()                  ← 唤醒一个等待线程
```

### RAII Guard 析构

```cpp
ConnectionGuard::~ConnectionGuard() {
    if (conn_) pool_.release(conn_);  // 自动归还，无法忘记
}
```

---

## 三、线程安全保证

| 操作 | 保护机制 |
|------|---------|
| 读写 `pool_` | `std::mutex` 加锁 |
| 等待空闲连接 | `condition_variable::wait()` 原子解锁+挂起 |
| 归还后唤醒 | `notify_one()` 在锁外调用（减少持锁时间） |
| 连接对象本身 | 每条连接同时只被一个线程持有，无需二次保护 |

---

## 四、连接池大小公式

### 公式来源

来自 HikariCP（Java 最流行连接池）的官方 Wiki：
**"About Pool Sizing"**（原文引用 PostgreSQL 团队结论）

### 公式

```
pool_size = (core_count × 2) + effective_spindle_count
```

| 参数 | 含义 |
|------|------|
| `core_count` | CPU 物理核心数（非超线程数） |
| `× 2` | 每核同时跑 1 个 CPU 任务 + 1 个 IO 等待任务 |
| `effective_spindle_count` | 磁盘并行寻道数：机械盘=盘片数，SSD/NVMe=1 |

### 示例计算

| 机器配置 | pool_size |
|---------|-----------|
| 4 核 + SSD | (4×2)+1 = **9**，取整 10 |
| 8 核 + SSD | (8×2)+1 = **17**，取整 16~20 |
| 4 核 + 机械盘（1片）| (4×2)+1 = **9** |

### 为什么不是越大越好

1. **MySQL Server 资源上限**：每条连接约占 256KB~1MB 内存，连接数超过 `max_connections`（默认 151）会报错
2. **线程调度开销**：MySQL 内部每条连接对应一个 Server 线程，过多线程导致 OS 频繁上下文切换，CPU 时间片浪费在调度上
3. **锁竞争加剧**：InnoDB 内部有 row lock / buffer pool mutex，并发连接越多竞争越激烈，反而吞吐下降
4. **经验结论**：并发连接数超过 `core_count × 2` 后，TPS 通常不再提升，反而因上述开销下降

### 实际建议

- 开发/测试：`pool_size = 5`
- 生产（4核 SSD）：`pool_size = 10`
- 压测确定最优值：逐步加大 pool_size，用 TPS 曲线找拐点

---

## 五、可选优化：idle 连接超时回收

当前实现没有超时回收，长期空闲的连接可能被 MySQL Server 强制断开
（`wait_timeout` 默认 8h，`interactive_timeout` 默认 8h）。

应对方案：
1. **定期 ping**：后台线程每 30s 对所有空闲连接调 `mysql_ping()`，断了重建
2. **acquire 时 ping**：取出连接后先 `ping()`，失败则重建（简单但加了一次 RTT）
3. **带时间戳的连接**：记录最后使用时间，超过阈值时 release 后不归还而是 delete 并新建

本次实现采用方案 2 的思路（`Connection::ping()` 已预留接口），Week3 接入 chat_server 时再完善。

---

## 六、面试常问

**Q: 连接池的 acquire() 为什么用 condition_variable 而不是 spin lock？**

A: spin lock 会让等待线程持续占用 CPU（忙等），在连接池满的场景下可能有大量线程在转，
浪费 CPU。`condition_variable::wait()` 会把线程挂起，OS 调度器不再分配时间片给它，
等 `notify_one()` 后才重新进入就绪队列，CPU 利用率更高。

**Q: notify_one 为什么放在锁外？**

A: 如果在锁内 notify，被唤醒线程立刻尝试加锁但持锁者还没解锁，浪费一次唤醒。
锁外 notify 让持锁者先解锁，被唤醒线程直接加锁成功，减少一次上下文切换。

**Q: ConnectionGuard 为什么禁止拷贝但允许移动？**

A: 一条连接同时只能被一个持有者使用（独占资源）。拷贝会造成两个 guard 析构时
double-release 同一条连接。移动语义通过把原 guard 的 `conn_` 置 nullptr 保证所有权唯一转移。
