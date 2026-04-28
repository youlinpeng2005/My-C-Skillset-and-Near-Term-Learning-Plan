// 第一周 Day6：模板编程基础 + 泛型线程安全队列
// 学习目标：理解模板实例化，实现 BlockingQueue<T>（线程池的核心组件）
// 重点：这个队列下周线程池会直接用，今天必须彻底写清楚

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
#include <optional>
#include <chrono>
#include <type_traits>

// ============================================================
// Part 1: 模板基础回顾
// ============================================================

// 函数模板：编译器根据参数类型自动推导
template <typename T>
T max_val(T a, T b) {
    return a > b ? a : b;
}

// 类模板：泛型容器
template <typename T>
class SimpleStack {
public:
    void push(T value) { data_.push_back(std::move(value)); }
    T pop() {
        if (data_.empty()) throw std::runtime_error("Stack is empty");
        T val = std::move(data_.back());
        data_.pop_back();
        return val;
    }
    bool empty() const { return data_.empty(); }
    size_t size() const { return data_.size(); }

private:
    std::vector<T> data_;
};

// 模板特化：对 bool 类型做专门优化（节省空间）
// （了解概念即可，实际用 std::vector<bool> 特化作为例子）

// SFINAE：模板只对满足条件的类型启用
// enable_if：只有 T 是整数类型时，此函数才存在
template <typename T>
typename std::enable_if<std::is_integral<T>::value, bool>::type
is_even(T n) { return n % 2 == 0; }

void demo_template_basics() {
    std::cout << "=== 模板基础 ===" << std::endl;

    std::cout << "max_val(3, 5) = " << max_val(3, 5) << std::endl;
    std::cout << "max_val(3.14, 2.71) = " << max_val(3.14, 2.71) << std::endl;
    std::cout << "max_val('a', 'z') = " << max_val('a', 'z') << std::endl;

    SimpleStack<std::string> stack;
    stack.push("first");
    stack.push("second");
    std::cout << "pop: " << stack.pop() << std::endl;

    std::cout << "is_even(4) = " << is_even(4) << std::endl;
    std::cout << "is_even(7) = " << is_even(7) << std::endl;
    // is_even(3.14); // 编译错误，SFINAE 不允许
}

// ============================================================
// Part 2: BlockingQueue<T> —— 线程安全阻塞队列
// 这是线程池的核心数据结构，必须手写掌握
// 原理：mutex 保护队列，condition_variable 实现阻塞等待
// ============================================================

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(size_t max_size = SIZE_MAX)
        : max_size_(max_size), stopped_(false) {}

    // 禁止拷贝
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    // 入队：如果队列满，阻塞等待（线程池 submit 时调用）
    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        // 等待条件：队列未满 且 未停止
        not_full_.wait(lock, [this] {
            return queue_.size() < max_size_ || stopped_;
        });
        if (stopped_) return false;
        queue_.push(std::move(item));
        not_empty_.notify_one(); // 通知消费者有新任务
        return true;
    }

    // 出队：如果队列空，阻塞等待（工作线程调用）
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        // 使用 while 而非 if，防止虚假唤醒（spurious wakeup）
        not_empty_.wait(lock, [this] {
            return !queue_.empty() || stopped_;
        });
        if (queue_.empty()) return false; // stopped 且队列空
        item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one(); // 通知生产者可以继续入队
        return true;
    }

    // 非阻塞尝试出队（立即返回）
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 带超时的出队
    bool pop_timeout(T& item, int timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        bool ok = not_empty_.wait_for(
            lock,
            std::chrono::milliseconds(timeout_ms),
            [this] { return !queue_.empty() || stopped_; }
        );
        if (!ok || queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return true;
    }

    // 停止队列：唤醒所有阻塞的线程（线程池关闭时调用）
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    bool is_stopped() const { return stopped_; }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_; // 队列不空时通知消费者
    std::condition_variable not_full_;  // 队列不满时通知生产者
    std::queue<T> queue_;
    size_t max_size_;
    bool stopped_;
};

// ============================================================
// Part 3: 生产者-消费者测试
// ============================================================

void demo_blocking_queue() {
    std::cout << "\n=== BlockingQueue 生产者-消费者测试 ===" << std::endl;

    BlockingQueue<std::string> queue(5); // 最大容量 5

    // 生产者线程
    std::thread producer([&queue] {
        for (int i = 0; i < 8; ++i) {
            std::string task = "任务_" + std::to_string(i);
            if (queue.push(task)) {
                std::cout << "[生产者] 入队: " << task << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        queue.stop();
        std::cout << "[生产者] 已停止" << std::endl;
    });

    // 消费者线程
    std::thread consumer([&queue] {
        std::string task;
        while (queue.pop(task)) {
            std::cout << "[消费者] 处理: " << task << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "[消费者] 队列已停止，退出" << std::endl;
    });

    producer.join();
    consumer.join();
}

// ============================================================
// Part 4: 模板的编译期推导（了解，为后续线程池 submit 做准备）
// ============================================================

// 变参模板：接受任意数量、任意类型的参数
template <typename F, typename... Args>
auto invoke_and_print(F&& f, Args&&... args) -> decltype(f(args...)) {
    std::cout << "[调用函数]" << std::endl;
    return std::forward<F>(f)(std::forward<Args>(args)...);
}

void demo_variadic_template() {
    std::cout << "\n=== 变参模板（线程池 submit 的基础）===" << std::endl;

    auto result = invoke_and_print([](int a, int b) { return a + b; }, 3, 4);
    std::cout << "结果: " << result << std::endl;

    invoke_and_print([]() { std::cout << "无参函数调用" << std::endl; });
}

// ============================================================
// 自测问题（Day6 结束前回答）
// Q1: 为什么 condition_variable 必须配合 mutex 使用？
// Q2: wait 的第二个参数（predicate）为什么用 while 等价语义而非 if？
//     （提示：虚假唤醒 spurious wakeup）
// Q3: BlockingQueue::stop() 为什么要调用 notify_all 而非 notify_one？
// Q4: mutex_ 为什么声明为 mutable？
// ============================================================

Q1：condition_variable 本身不保存条件状态，它只负责“通知”，真正的条件判断需要靠共享变量完成，而共享变量必须用 mutex 保护，否则会出现竞争条件；同时 wait() 会先解锁 mutex 挂起线程，被唤醒后再重新加锁继续执行。

Q2：因为 wait() 可能发生虚假唤醒（spurious wakeup），线程即使没有真正满足条件也可能被唤醒，所以不能用 if 只判断一次，而要用 while 反复检查条件，确保条件真的成立后再继续执行。

Q3：stop() 表示整个队列停止工作，可能有多个生产者和消费者都阻塞在 wait() 上，如果只用 notify_one()，只会唤醒一个线程，其他线程可能永久阻塞；notify_all() 能让所有等待线程都被唤醒并检测停止状态后安全退出。

Q4：mutable 允许成员变量在 const 成员函数中仍然可以被修改。比如 size() const、empty() const 这类逻辑上不修改对象状态的函数，也需要加锁保护共享数据，因此 mutex_ 要声明为 mutable，否则 const 函数里无法对它加锁。

int main() {
    demo_template_basics();
    demo_blocking_queue();
    demo_variadic_template();

    std::cout << "\n=== Day6 完成！明天复盘第一周，提交代码 ===" << std::endl;
    return 0;
}
