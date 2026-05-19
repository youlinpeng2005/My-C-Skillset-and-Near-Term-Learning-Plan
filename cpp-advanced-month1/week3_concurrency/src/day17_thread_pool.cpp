// 第三周 Day17（5月5日）：线程池核心实现
// 这是本月最重要的代码，必须自己手写一遍！
// 功能：支持 submit(task)、submit 返回 future、优雅关闭

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <vector>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <chrono>

// ============================================================
// ThreadPool 实现
//
// 架构：
//   主线程 submit(task) -> 任务队列（BlockingQueue）
//                              ^
//   工作线程 (N个) <-------- 从队列取任务 -> 执行
//
// 三要素：
//   1. 工作线程数组：vector<thread>
//   2. 任务队列：queue<function<void()>> + mutex + condition_variable
//   3. 控制信号：stopped_ 标志
// ============================================================

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stopped_(false) {
        if (num_threads == 0) throw std::invalid_argument("num_threads must > 0");

        // 创建工作线程
        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i] {
                worker_loop(i);
            });
        }
        std::cout << "[ThreadPool] 启动 " << num_threads << " 个工作线程" << std::endl;
    }

    // 禁止拷贝和移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 析构：优雅关闭（等所有已提交任务完成后退出）
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all(); // 唤醒所有在等待的工作线程

        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
        std::cout << "[ThreadPool] 已优雅关闭" << std::endl;
    }

    // submit：提交任务，返回 future（可以获取返回值）
    // 使用变参模板 + 完美转发，支持任意可调用对象和参数
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        if (stopped_) throw std::runtime_error("ThreadPool has been stopped");

        using ReturnType = decltype(f(args...));//4.为什么这样写

        // packaged_task 包装任务，绑定参数
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopped_) throw std::runtime_error("ThreadPool has been stopped");
            // 将任务包装为 function<void()> 入队
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one(); // 唤醒一个工作线程

        return result;
    }

    // 提交无返回值任务（简化版）
    void submit_void(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopped_) return;
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    size_t pending_tasks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

private:
    // 工作线程的主循环
    void worker_loop(size_t worker_id) {
        while (true) {
            std::function<void()> task;//2.为什么这样写
            {
                std::unique_lock<std::mutex> lock(mutex_);
                // 等待：有任务 或 已停止
                cv_.wait(lock, [this] {
                    return !tasks_.empty() || stopped_;
                });//3.为什么这样写

                // 停止且队列空：退出
                if (stopped_ && tasks_.empty()) {
                    std::cout << "[工作线程 " << worker_id << "] 退出" << std::endl;
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }
            // 锁已释放，执行任务（不持锁执行，允许其他线程继续提交）
            task();
        }
    }

    mutable std::mutex mutex_; //1.为什么要mutable
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_; 
    bool stopped_;
};

// ============================================================
// 测试：基本任务提交
// ============================================================

void test_basic_submit() {
    std::cout << "\n=== 测试：基本任务提交 ===" << std::endl;
    ThreadPool pool(4);

    std::atomic<int> completed(0);
    for (int i = 0; i < 10; ++i) {
        pool.submit_void([i, &completed] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::cout << "任务 " << i << " 执行完毕，线程ID=" << std::this_thread::get_id() << std::endl;
            ++completed;
        });
    }

    // 等待所有任务完成（简单方式：sleep，生产环境用 latch 或 barrier）
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "完成任务数: " << completed << "/10" << std::endl;
}

// ============================================================
// 测试：submit 返回 future，获取返回值
// ============================================================

void test_future_submit() {
    std::cout << "\n=== 测试：future 获取返回值 ===" << std::endl;
    ThreadPool pool(2);

    auto f1 = pool.submit([](int a, int b) { return a + b; }, 3, 4);
    auto f2 = pool.submit([]() -> std::string { return "hello from threadpool"; });
    auto f3 = pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });

    std::cout << "f1 (3+4) = " << f1.get() << std::endl;
    std::cout << "f2 = " << f2.get() << std::endl;
    std::cout << "f3 = " << f3.get() << std::endl;
}

// ============================================================
// 测试：任务中抛出异常
// ============================================================

void test_exception_handling() {
    std::cout << "\n=== 测试：异常处理 ===" << std::endl;
    ThreadPool pool(2);

    auto f = pool.submit([]() -> int {
        throw std::runtime_error("任务内部异常！");
        return 0;
    });

    try {
        f.get(); // 异常会在 get() 时重新抛出
    } catch (const std::exception& e) {
        std::cout << "捕获到异常: " << e.what() << std::endl;
    }
}

// ============================================================
// 测试：压力测试
// ============================================================

void test_stress() {
    std::cout << "\n=== 压力测试：1000 个任务 ===" << std::endl;
    ThreadPool pool(std::thread::hardware_concurrency()); // CPU核心数

    std::atomic<long long> total(0);
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 1000; ++i) {
        futures.push_back(pool.submit([i] { return i * i; }));
    }

    for (auto& f : futures) {
        total += f.get();
    }

    std::cout << "1000 个任务完成，总和 = " << total << std::endl;
}

int main() {
    test_basic_submit();
    test_future_submit();
    test_exception_handling();
    test_stress();

    std::cout << "\n=== Day17 完成！线程池是本月最重要的代码，确保理解每一行！===" << std::endl;
    return 0;
}

// ============================================================
// 自测问题（Day17 结束前回答）
// Q1: 为什么 packaged_task 要用 shared_ptr 包装？
// Q2: 析构时为什么要先设置 stopped_=true，再 notify_all，再 join？
//     如果顺序错了会怎样？
// Q3: 工作线程执行任务时为什么要先释放锁，再执行 task()？
// Q4: 如果一个任务执行时间非常长，会阻塞整个线程池吗？
// Q5: 如何实现"有界线程池"（任务队列满时阻塞 submit）？
// ============================================================
