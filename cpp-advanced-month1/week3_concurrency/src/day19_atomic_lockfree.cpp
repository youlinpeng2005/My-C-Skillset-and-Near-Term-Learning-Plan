// 第三周 Day19（5月7日）：无锁思想入门
// 学习目标：了解 std::atomic、CAS 原语、内存顺序
// 注意：这部分了解为主，不要深陷！重点在应用层并发

#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>

// ============================================================
// Part 1: std::atomic 基础
// ============================================================

void demo_atomic_counter() {
    std::cout << "=== atomic 计数器 vs 加锁计数器 ===" << std::endl;

    const int N = 1000000;

    // 有锁版本
    int locked_counter = 0;
    std::mutex mtx;
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&] {
                for (int i = 0; i < N / 4; ++i) {
                    std::lock_guard<std::mutex> lock(mtx);
                    ++locked_counter;
                }
            });
        }
        for (auto& t : threads) t.join();
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "有锁版本: " << locked_counter << "，耗时 " << ms << "ms" << std::endl;
    }

    // 无锁版本（atomic）
    std::atomic<int> atomic_counter(0);
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&] {
                for (int i = 0; i < N / 4; ++i) {
                    atomic_counter.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& t : threads) t.join();
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "atomic版本: " << atomic_counter.load() << "，耗时 " << ms << "ms" << std::endl;
    }
}

// ============================================================
// Part 2: CAS（Compare-And-Swap）原语
// CAS 是无锁编程的基础：
//   if (*ptr == expected) { *ptr = desired; return true; }
//   else { expected = *ptr; return false; }
//   这是一个原子操作！
// ============================================================

void demo_cas() {
    std::cout << "\n=== CAS 原语 ===" << std::endl;

    std::atomic<int> value(10);

    // compare_exchange_strong：强 CAS，失败时不会重试
    int expected = 10;
    int desired = 20;
    bool success = value.compare_exchange_strong(expected, desired);
    std::cout << "CAS(10->20): " << (success ? "成功" : "失败")
              << "，当前值=" << value.load() << std::endl;

    // 再次尝试：expected 已经变成实际值（10->20 后，expected=20）
    // 这次 expected=20，value=20，CAS 成功
    success = value.compare_exchange_strong(expected, 30);
    std::cout << "CAS(20->30): " << (success ? "成功" : "失败")
              << "，当前值=" << value.load() << std::endl;

    // 失败的 CAS：expected 不匹配
    expected = 999; // 故意错误
    success = value.compare_exchange_strong(expected, 40);
    std::cout << "CAS(999->40): " << (success ? "成功" : "失败")
              << "，expected 被更新为=" << expected
              << "，当前值=" << value.load() << std::endl;
}

// ============================================================
// Part 3: 内存顺序（了解概念，不要深入）
// ============================================================
//
// memory_order_relaxed:  只保证原子性，不保证顺序（最快）
// memory_order_acquire:  读操作屏障（之后的读写不会重排到此前）
// memory_order_release:  写操作屏障（之前的读写不会重排到此后）
// memory_order_seq_cst:  顺序一致性（最安全，默认值，最慢）
//
// 实际建议：
//   - 计数器：memory_order_relaxed
//   - 生产者-消费者标志：release（写）+ acquire（读）
//   - 不确定时：memory_order_seq_cst（安全但有性能代价）

void demo_memory_order() {
    std::cout << "\n=== 内存顺序 ===" << std::endl;

    std::atomic<bool> ready(false);
    std::atomic<int> data(0);

    // 生产者：先写数据，再设置 ready
    std::thread producer([&] {
        data.store(42, std::memory_order_relaxed);     // 写数据
        ready.store(true, std::memory_order_release);  // release：保证 data 在 ready 之前可见
    });

    // 消费者：先检查 ready，再读数据
    std::thread consumer([&] {
        while (!ready.load(std::memory_order_acquire)) { // acquire：保证读到 ready=true 后能看到 data
            std::this_thread::yield();
        }
        std::cout << "消费者读到 data = " << data.load(std::memory_order_relaxed) << "（期望 42）" << std::endl;
    });

    producer.join();
    consumer.join();
}

// ============================================================
// Part 4: 用 atomic 实现自旋锁（了解原理）
// ============================================================

class SpinLock {
public:
    void lock() {
        // CAS 循环：直到成功将 false 换为 true
        bool expected = false;
        while (!flag_.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
            expected = false; // 重置 expected，继续尝试
            // CPU 提示：我在自旋等待（降低功耗，避免流水线冲突）
            // __builtin_ia32_pause(); // x86 专用，可选
        }
    }

    void unlock() {
        flag_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> flag_{false};
};

void demo_spinlock() {
    std::cout << "\n=== 自旋锁（了解原理）===" << std::endl;

    SpinLock spinlock;
    int counter = 0;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 10000; ++i) {
                spinlock.lock();
                ++counter;
                spinlock.unlock();
            }
        });
    }
    for (auto& t : threads) t.join();
    std::cout << "自旋锁计数: " << counter << "（期望 40000）" << std::endl;

    std::cout << "\n【注意】自旋锁适合：锁持有时间极短、线程数 <= CPU核心数的场景" << std::endl;
    std::cout << "【注意】自旋锁不适合：锁持有时间长的场景（CPU 空转浪费）" << std::endl;
}

// ============================================================
// 自测问题（Day19 结束前回答）
// Q1: std::atomic<int> 的 ++ 操作是线程安全的，但 a = a + 1 是线程安全的吗？
// Q2: CAS 中的 ABA 问题是什么？如何解决？
// Q3: 自旋锁和 mutex 的主要区别和适用场景？
// Q4: memory_order_seq_cst 和 memory_order_acquire/release 的性能差异在哪？
// ============================================================

int main() {
    demo_atomic_counter();
    demo_cas();
    demo_memory_order();
    demo_spinlock();

    std::cout << "\n=== Day19 完成！了解无锁思想，但项目中优先用 mutex ===" << std::endl;
    return 0;
}
