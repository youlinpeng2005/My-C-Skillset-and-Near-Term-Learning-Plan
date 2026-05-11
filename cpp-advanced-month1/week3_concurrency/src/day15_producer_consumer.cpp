// 第三周 Day15（5月3日）：线程基础 + 生产者-消费者模型
// 学习目标：掌握 std::thread / mutex / condition_variable，理解虚假唤醒
// 重点：condition_variable::wait 的 predicate 参数为什么必须用 while 语义

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <chrono>
#include <atomic>

// ============================================================
// Part 1: std::thread 基础
// ============================================================

void demo_thread_basics() {
    std::cout << "=== std::thread 基础 ===" << std::endl;

    // 方式1：传函数
    auto task = [](int id) {
        std::cout << "线程 " << id << " 运行，ID = "
                  << std::this_thread::get_id() << std::endl;
    };

    std::thread t1(task, 1);
    std::thread t2(task, 2);

    // join：等待线程结束（主线程阻塞）
    t1.join();
    t2.join();

    // detach：分离线程（主线程不等待，线程独立运行）
    // 注意：detach 后不能再 join，且主线程退出时 detached 线程也会被终止
    std::thread t3([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // 如果主线程已退出，这里会 crash（慎用 detach）
    });
    t3.detach();

    std::cout << "主线程 ID = " << std::this_thread::get_id() << std::endl;
}

// ============================================================
// Part 2: mutex 基础 —— 保护共享数据
// ============================================================

std::mutex g_mutex;
int g_counter = 0;

void demo_mutex() {
    std::cout << "\n=== mutex 保护共享数据 ===" << std::endl;

    auto increment = [](int count) {
        for (int i = 0; i < count; ++i) {
            std::lock_guard<std::mutex> lock(g_mutex); // RAII 锁
            ++g_counter;
        }
    };

    g_counter = 0;
    std::thread t1(increment, 10000);
    std::thread t2(increment, 10000);
    t1.join();
    t2.join();

    // 有 mutex 保护：结果始终是 20000
    std::cout << "有锁保护，g_counter = " << g_counter << "（期望 20000）" << std::endl;
}

// ============================================================
// Part 3: 生产者-消费者模型
// ============================================================

class ProducerConsumer {
public:
    ProducerConsumer(int max_size) : max_size_(max_size), stopped_(false) {}

    void produce(const std::string& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        // 虚假唤醒保护：用 while（即 wait 的 predicate 参数）
        // 如果用 if：虚假唤醒后会跳过检查，可能在队列满时继续 push
        not_full_.wait(lock, [this] {
            return (int)queue_.size() < max_size_ || stopped_;
        });

        if (stopped_) return;
        queue_.push(item);
        std::cout << "[生产] " << item << "（队列大小=" << queue_.size() << "）" << std::endl;

        not_empty_.notify_one(); // 通知消费者
    }

    bool consume(std::string& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        not_empty_.wait(lock, [this] {
            return !queue_.empty() || stopped_;
        });

        if (queue_.empty()) return false;
        item = queue_.front();
        queue_.pop();
        std::cout << "[消费] " << item << "（队列大小=" << queue_.size() << "）" << std::endl;

        not_full_.notify_one(); // 通知生产者
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        not_full_.notify_all();
        not_empty_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::queue<std::string> queue_;
    int max_size_;
    bool stopped_;
};

void demo_producer_consumer() {
    std::cout << "\n=== 生产者-消费者模型 ===" << std::endl;

    ProducerConsumer pc(3); // 最大容量 3

    std::thread producer([&pc] {
        for (int i = 1; i <= 6; ++i) {
            pc.produce("任务" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        pc.stop();
    });

    std::thread consumer([&pc] {
        std::string item;
        while (pc.consume(item)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }
    });

    producer.join();
    consumer.join();
}

// ============================================================
// Part 4: 死锁演示 + 预防
// ============================================================

void demo_deadlock_prevention() {
    std::cout << "\n=== 死锁预防 ===" << std::endl;

    std::mutex m1, m2;

    // 死锁场景（注释掉，仅演示）：
    // 线程A: lock(m1) -> lock(m2)
    // 线程B: lock(m2) -> lock(m1)
    // 两个线程互相等待，永久阻塞

    // 预防方法1：按固定顺序加锁（所有线程都先锁 m1 再锁 m2）
    auto safe_task = [&] {
        std::lock_guard<std::mutex> lock1(m1); // 总是先锁 m1
        std::lock_guard<std::mutex> lock2(m2); // 再锁 m2
        std::cout << "安全获取两把锁" << std::endl;
    };

    // 预防方法2：std::lock 同时锁多个（无死锁保证）
    auto safe_task2 = [&] {
        std::unique_lock<std::mutex> l1(m1, std::defer_lock);
        std::unique_lock<std::mutex> l2(m2, std::defer_lock);
        std::lock(l1, l2); // 原子地获取两把锁
        std::cout << "std::lock 安全获取两把锁" << std::endl;
    };

    std::thread t1(safe_task);
    std::thread t2(safe_task2);
    t1.join();
    t2.join();
}

// ============================================================
// 自测问题（Day15 结束前回答）
// Q1: std::thread 析构时如果既没有 join 也没有 detach 会怎样？
std::thread 析构时如果既没有 join 也没有 detach 会调用 std::terminate 终止程序。
// Q2: lock_guard 和 unique_lock 的区别是什么？各自用于什么场景？
lock_guard 是最轻量的 RAII 加锁工具，创建时自动加锁，离开作用域自动解锁，不能手动解锁，也不能转移所有权，适合简单的临界区保护；而 unique_lock 更灵活，支持延迟加锁、手动解锁、重复加锁、移动语义等，并且 condition_variable::wait() 必须搭配 unique_lock 使用，因为 wait 过程中需要临时释放锁再重新加锁。简单场景优先用 lock_guard，复杂同步场景使用 unique_lock。
// Q3: 为什么虚假唤醒会发生？操作系统层面的原因是什么？
虚假唤醒（spurious wakeup）指线程即使没有真正收到通知，也可能从 wait() 返回。这是因为底层条件变量通常由操作系统的 futex、pthread_cond 等机制实现，系统为了避免“丢失唤醒”、简化内核实现或处理竞争条件，允许线程被“误唤醒”。此外，多个线程竞争条件变量时，也可能出现某个线程被唤醒但条件已经被其他线程抢先修改的情况。
// Q4: 如何避免死锁？列举至少3种方法
固定加锁顺序
一次性加锁
使用 try_lock
避免嵌套锁
// ============================================================

int main() {
    demo_thread_basics();
    demo_mutex();
    demo_producer_consumer();
    demo_deadlock_prevention();

    std::cout << "\n=== Day15 完成！明天任务：手写线程安全 BlockingQueue ===" << std::endl;
    return 0;
}
