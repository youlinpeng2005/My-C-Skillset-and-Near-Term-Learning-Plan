/**
 * day6_conn_pool.cpp — ConnectionPool 实现
 */

#include "day6_conn_pool.h"

#include <iostream>
#include <stdexcept>

// ── ConnectionGuard 析构 ─────────────────────────────────────────
//
// 必须在 .cpp 里定义（因为此时 ConnectionPool 已完整声明）。
// conn_ == nullptr 说明 guard 被 move 走了，无需归还。
//
ConnectionGuard::~ConnectionGuard() {
    if (conn_) {
        pool_.release(conn_);
    }
}

// ── ConnectionPool 构造 ──────────────────────────────────────────
//
// 预创建 pool_size 条连接。
// 任意一条连接失败都会抛异常，调用方可捕获后决定是否重试。
//
ConnectionPool::ConnectionPool(const Config& cfg) : cfg_(cfg) {
    for (int i = 0; i < cfg.pool_size; ++i) {
        pool_.push_back(std::make_unique<Connection>(
            cfg.host.c_str(),
            cfg.user.c_str(),
            cfg.password.c_str(),
            cfg.db.c_str(),
            cfg.port
        ));
    }
    std::cout << "[ConnectionPool] initialized with "
              << cfg.pool_size << " connections\n";
}

// ── acquire() ───────────────────────────────────────────────────
//
// 核心逻辑：
//   1. 加锁
//   2. condition_variable::wait(lock, predicate)
//      - predicate 为假（pool 空）→ 自动解锁并挂起线程（不占 CPU）
//      - pool 非空或 notify 唤醒后，重新加锁并检查 predicate
//   3. 从队头取出 unique_ptr，release() 交出裸指针所有权给 guard
//   4. 解锁（unique_lock 离开作用域）
//
ConnectionGuard ConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);

    // 没有空闲连接就阻塞，直到 release() 调用 notify_one
    cond_.wait(lock, [this] { return !pool_.empty(); });

    // 取队头连接，将 unique_ptr 所有权转给裸指针
    auto conn_uptr = std::move(pool_.front());
    pool_.pop_front();

    // 把裸指针所有权交给 ConnectionGuard（guard 析构时负责归还）
    return ConnectionGuard(conn_uptr.release(), *this);
}

// ── release() ───────────────────────────────────────────────────
//
// 将连接包回 unique_ptr 归还 pool_，然后 notify_one 唤醒一个等待线程。
// 注意：lock_guard 保证修改 pool_ 是原子的。
//
void ConnectionPool::release(Connection* conn) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(std::unique_ptr<Connection>(conn));
    }
    // 在锁外 notify，减少持锁时间（被唤醒线程会重新竞争锁）
    cond_.notify_one();
}

// ── available() ─────────────────────────────────────────────────
int ConnectionPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(pool_.size());
}
