# 第一周总结：Modern C++ 核心特性

> 时间：4月22日 ～ 4月28日 | 主题：C++11/14/17 现代特性精要

---

## 一、本周学习路线图

```
Day1  智能指针         →  Day2  RAII封装资源
Day3  move语义         →  Day4  lambda / function / bind
Day5  STL高级 + LRU    →  Day6  模板编程 + BlockingQueue<T>
Day7  复盘自测
```

---

## 二、各天核心知识点

### Day1 — 智能指针原理
| 类型 | 特点 | 典型场景 |
|------|------|----------|
| `unique_ptr` | 独占所有权，零开销 | 函数局部资源、工厂返回值 |
| `shared_ptr` | 引用计数共享所有权 | 多对象共享同一资源 |
| `weak_ptr` | 弱引用，不影响计数 | 打破循环引用（如观察者模式） |

**手写 `MySharedPtr<T>`**：

```cpp
template <typename T>
class MySharedPtr {
public:
    explicit MySharedPtr(T* ptr = nullptr)
        : ptr_(ptr), ref_count_(ptr ? new int(1) : nullptr) {}

    // 拷贝构造：共享引用计数并 +1
    MySharedPtr(const MySharedPtr& other)
        : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        if (ref_count_) ++(*ref_count_);
    }

    // 移动构造：接管资源，原指针置空
    MySharedPtr(MySharedPtr&& other) noexcept
        : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        other.ptr_ = nullptr;
        other.ref_count_ = nullptr;
    }

    ~MySharedPtr() { release(); }

    MySharedPtr& operator=(const MySharedPtr& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;
            if (ref_count_) ++(*ref_count_);
        }
        return *this;
    }

    T& operator*()  const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get()        const { return ptr_; }
    int use_count() const { return ref_count_ ? *ref_count_ : 0; }
    explicit operator bool() const { return ptr_ != nullptr; }

private:
    void release() {
        if (ref_count_ && --(*ref_count_) == 0) {
            delete ptr_;
            delete ref_count_;
        }
        ptr_ = nullptr;
        ref_count_ = nullptr;
    }

    T*   ptr_;
    int* ref_count_; // 真实实现用 atomic<int>，此处简化
};
```

> **关键点**：真实 `shared_ptr` 的 `ref_count_` 存在独立的 **control block** 里，使用 `atomic<int>` 保证引用计数的线程安全；`weak_ptr` 共享同一 control block 但只持有弱引用计数，`lock()` 时原子检查强引用计数是否 > 0。

---

### Day2 — RAII 思想 + `unique_ptr` 实战
- **RAII 核心**：资源获取即初始化，析构函数负责释放，保证异常安全。
- 用 `unique_ptr` 封装文件描述符、数据库连接等系统资源。
- **禁止裸 `new/delete`**：使用 `make_unique` / `make_shared`。

```cpp
// 典型用法
auto fd = std::make_unique<FileDescriptor>(open(...));
// 离开作用域自动 close，即使中途抛异常
```

---

### Day3 — move 语义 + 右值引用
- `std::move` 本身**不移动**，只做类型转换（`T&` → `T&&`）。
- move 后原对象处于"**有效但未指定**"状态，不应再使用其值。
- **加 `noexcept`**：`vector` 扩容时才会选择移动而非拷贝，性能关键。

**拷贝 vs 移动对比（手写 `BigData`）**：

```cpp
class BigData {
public:
    explicit BigData(size_t size) : size_(size), data_(new int[size]) {}

    // 拷贝构造：深拷贝，重新分配内存（慢）
    BigData(const BigData& other) : size_(other.size_), data_(new int[other.size_]) {
        std::copy(other.data_, other.data_ + other.size_, data_);
    }

    // 移动构造：直接"偷走"指针，不分配内存（快）
    BigData(BigData&& other) noexcept
        : size_(other.size_), data_(other.data_) {
        other.data_ = nullptr;  // 原对象放弃所有权
        other.size_ = 0;
    }

    ~BigData() { delete[] data_; }

private:
    size_t size_;
    int*   data_;
};
```

---

### Day4 — Lambda + `std::function` + `bind`
- Lambda 捕获列表：`[=]` 值捕获、`[&]` 引用捕获、混合捕获、`mutable`。
- `std::function<R(Args...)>`：类型擦除，统一持有各种可调用对象。
- 实现了 `EventHandler` 回调注册器，为后续 Reactor 模式铺路。

```cpp
// 核心模式：回调注册
std::function<void(int)> handler;
handler = [this](int fd) { onReadable(fd); };
```

---

### Day5 — STL 高级用法 + 手写 LRU 缓存

**迭代器失效规则**（必考）：
- `vector`：中间插入/删除 → 后续迭代器全部失效；扩容 → 全部失效
- `list`：插入/删除不影响其他迭代器
- `unordered_map`：插入可能 rehash → 全部迭代器失效

**手写 `LRUCache<K, V>`**（O(1) get/put）：

```
数据结构：list<pair<K,V>>（维护访问顺序）+ unordered_map<K, list::iterator>（O(1)定位）
get：找到节点 → splice 移到链表头 → 返回值
put：已存在 → 更新值 + splice 到头；不存在且满 → 删除链表尾+map条目 → 插入头部
```

```cpp
template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(int capacity) : capacity_(capacity) {}

    V get(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) throw std::out_of_range("key not found");
        // 访问了，移到链表头（最近使用）
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    void put(const K& key, const V& value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }
        if ((int)list_.size() >= capacity_) {
            // 淘汰链表尾（最久未使用）
            map_.erase(list_.back().first);
            list_.pop_back();
        }
        list_.emplace_front(key, value);
        map_[key] = list_.begin();
    }

    bool contains(const K& key) const { return map_.count(key) > 0; }

private:
    int capacity_;
    std::list<std::pair<K, V>> list_;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> map_;
};
```

> **为什么用 `list` 而非 `vector`**：LRU 需要频繁"中间删除 + 头部插入"，`list` 节点操作 O(1)，`vector` 中间删除 O(n)。`list::splice` 只改指针，不拷贝数据，复杂度 O(1)。

---

### Day6 — 模板编程基础 + 泛型 `BlockingQueue<T>`
- 函数模板、类模板、模板特化（`std::is_integral_v<T>` 等 type traits）。
- 实现线程安全的 `BlockingQueue<T>`：
  - `push`：加锁入队，`notify_one`
  - `pop`：`while (!pred) cv.wait(lock)` 防虚假唤醒
  - `stop`：`notify_all` 唤醒所有等待线程
- **关键**：下周线程池的任务队列直接复用此组件。

---

### Day7 — 复盘自测

| 高频面试题 | 核心答案 |
|-----------|---------|
| `shared_ptr` 线程安全？ | 引用计数操作安全（atomic）；对象访问/shared_ptr本身的赋值不安全 |
| move 后原对象状态？ | 有效但未指定（析构安全，值不可依赖） |
| 移动构造为何加 `noexcept`？ | vector 扩容优先选 noexcept 的移动构造 |
| 条件变量为何用 `while` 而非 `if`？ | 防止虚假唤醒（spurious wakeup） |
| `BlockingQueue::stop()` 为何 `notify_all`？ | 可能有多线程等待，需广播通知全部 |

---

## 三、本周产出文件

| 文件 | 内容 |
|------|------|
| `day1_smart_ptr.cpp` | `MySharedPtr` 手写实现 + weak_ptr 循环引用演示 |
| `day2_raii.cpp` | RAII封装FileDescriptor / 数据库连接 |
| `day3_move_semantics.cpp` | Buffer类移动构造/赋值实现 |
| `day4_lambda_function.cpp` | EventHandler 回调注册器 |
| `day5_stl_lru.cpp` | LRU Cache（双向链表 + unordered_map） |
| `day6_template_queue.cpp` | `BlockingQueue<T>` 泛型线程安全队列 |
| `day7_review.md` | 自测问答 + 下周预告 |

---

## 四、知识图谱：本周技术关联

```
智能指针（unique/shared/weak）
    └─ RAII 思想
         └─ 析构函数保证资源安全
              └─ move 语义减少拷贝开销
                   └─ lambda / function 统一回调
                        └─ 模板泛型 BlockingQueue<T>
                             └─ 下周：线程池任务队列
```

---

## 五、下周预告（第二周：epoll 与 IO 多路复用）

| Day | 主题 |
|-----|------|
| Day8 | 5种IO模型全景（阻塞/非阻塞/多路复用/信号驱动/异步） |
| Day9 | epoll 底层原理（红黑树+就绪链表）+ 最小 echo 服务器 |
| Day10 | ET 模式实战（非阻塞 + 循环读） |
| Day11 | TCP 粘包/拆包 + 自定义协议设计 |
| Day12 | Reactor 模式（EventLoop + Channel + Acceptor） |
| Day13 | 单线程聊天室（多客户端 epoll） |
| Day14 | 复盘 |

**预习建议**：《Linux高性能服务器编程》第9章 IO复用
