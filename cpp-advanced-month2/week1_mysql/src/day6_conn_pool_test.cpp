/**
 * day6_conn_pool_test.cpp — ConnectionPool 测试
 *
 * 测试用例：
 *   Test 1 — 单线程：拿连接 → 执行 SQL → 自动归还
 *   Test 2 — 多线程：100 线程抢 10 个连接，全部成功不死锁
 *   Test 3 — 池满阻塞：手动持有全部连接后启动新线程，验证 acquire 阻塞 + 归还后唤醒
 */

#include <mariadb/mysql.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "day6_conn_pool.h"

// ── 数据库连接配置 ───────────────────────────────────────────────
static ConnectionPool::Config make_config(int pool_size = 10) {
    ConnectionPool::Config cfg;
    cfg.host      = "127.0.0.1";
    cfg.user      = "root";
    cfg.password  = "pyl200511";
    cfg.db        = "chat_db";
    cfg.port      = 3306;
    cfg.pool_size = pool_size;
    return cfg;
}

// ── Test 1：单线程 RAII 验证 ─────────────────────────────────────
void test_single_thread() {
    std::cout << "\n=== Test 1: Single Thread ===\n";

    ConnectionPool pool(make_config(3));
    std::cout << "available before acquire: " << pool.available() << "\n";

    {
        auto guard = pool.acquire();
        std::cout << "available during acquire: " << pool.available() << "\n";

        guard->execute("DO 1");   // DO 1 无结果集，验证连接可用
        std::cout << "[Test1] DO 1 OK\n";
    }   // guard 析构 → release()

    std::cout << "available after release: " << pool.available() << "\n";

    // 验证：归还后应该和拿之前一样
    if (pool.available() == 3) {
        std::cout << "[Test1] PASS\n";
    } else {
        std::cout << "[Test1] FAIL — available=" << pool.available() << "\n";
    }
}

// ── Test 2：多线程并发抢连接 ─────────────────────────────────────
void test_concurrent() {
    std::cout << "\n=== Test 2: Concurrent (100 threads, 10 connections) ===\n";

    ConnectionPool pool(make_config(10));

    constexpr int THREADS = 100;
    std::atomic<int> success{0};
    std::atomic<int> fail{0};

    auto task = [&]() {
        try {
            auto guard = pool.acquire();
            guard->execute("DO 1");   // DO 1 无结果集，不会触发 out-of-sync
            ++success;
            // 模拟 1ms 业务处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } catch (const std::exception& e) {
            ++fail;
            std::cerr << "[Test2] exception: " << e.what() << "\n";
        }
    };

    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(THREADS);
    for (int i = 0; i < THREADS; ++i)
        threads.emplace_back(task);

    for (auto& t : threads)
        t.join();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    std::cout << "success=" << success << " fail=" << fail
              << " elapsed=" << elapsed << "ms\n";
    std::cout << "available after all threads: " << pool.available() << "\n";

    if (success == THREADS && fail == 0 && pool.available() == 10) {
        std::cout << "[Test2] PASS\n";
    } else {
        std::cout << "[Test2] FAIL\n";
    }
}

// ── Test 3：池满时 acquire 阻塞，归还后唤醒 ──────────────────────
void test_blocking_acquire() {
    std::cout << "\n=== Test 3: Blocking acquire + notify on release ===\n";

    constexpr int POOL_SIZE = 3;
    ConnectionPool pool(make_config(POOL_SIZE));

    // 手动拿走全部 3 个连接，保持不归还
    std::vector<ConnectionGuard> held;
    held.reserve(POOL_SIZE);
    for (int i = 0; i < POOL_SIZE; ++i)
        held.push_back(pool.acquire());

    std::cout << "held all " << POOL_SIZE << " connections, available="
              << pool.available() << "\n";

    // 启动一个线程尝试 acquire（此时应该阻塞）
    std::atomic<bool> got_conn{false};
    std::thread waiter([&]() {
        auto guard = pool.acquire();   // 阻塞在这里
        got_conn   = true;
        std::cout << "[Test3] waiter got connection\n";
        guard->execute("DO 1");
    });

    // 等 50ms，验证 waiter 线程确实在阻塞
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (got_conn.load()) {
        std::cout << "[Test3] FAIL — should have blocked\n";
    } else {
        std::cout << "[Test3] waiter is blocked as expected\n";
    }

    // 归还一条连接，waiter 应该立刻被唤醒
    held.pop_back();   // ConnectionGuard 析构 → release() → notify_one()

    waiter.join();

    if (got_conn.load()) {
        std::cout << "[Test3] PASS\n";
    } else {
        std::cout << "[Test3] FAIL — waiter never woke up\n";
    }
}

// ── main ─────────────────────────────────────────────────────────
int main() {
    mysql_library_init(0, nullptr, nullptr);

    try {
        test_single_thread();
        test_concurrent();
        test_blocking_acquire();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        mysql_library_end();
        return 1;
    }

    std::cout << "\n=== All tests done ===\n";
    mysql_library_end();
    return 0;
}
