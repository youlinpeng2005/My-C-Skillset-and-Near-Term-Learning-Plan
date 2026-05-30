/**
 * day6_conn_pool.h — 线程安全 MySQL 连接池
 *
 * 设计要点：
 *   - ConnectionPool 启动时预创建 pool_size 条连接
 *   - acquire() 阻塞获取空闲连接（condition_variable）
 *   - ConnectionGuard（RAII）出作用域后自动调 release() 归还
 *   - 禁止拷贝 pool，连接生命周期由 pool 管理
 */

#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include "connection.h"

// ── 前向声明 ─────────────────────────────────────────────────────
class ConnectionPool;

// ── ConnectionGuard：RAII 连接持有者 ────────────────────────────
//
// 使用示例：
//   {
//       auto guard = pool.acquire();
//       guard->execute("SELECT 1");
//   }  // 离开作用域 → 析构 → pool.release() 自动调用
//
class ConnectionGuard {
public:
    ConnectionGuard(Connection* conn, ConnectionPool& pool)
        : conn_(conn), pool_(pool) {}

    ~ConnectionGuard();

    Connection* get()             { return conn_; }
    Connection* operator->()      { return conn_; }
    Connection& operator*()       { return *conn_; }

    // 不可拷贝（同一时刻一条连接只能由一个 guard 持有）
    ConnectionGuard(const ConnectionGuard&)            = delete;
    ConnectionGuard& operator=(const ConnectionGuard&) = delete;

    // 可移动（允许从函数返回）
    ConnectionGuard(ConnectionGuard&& other) noexcept
        : conn_(other.conn_), pool_(other.pool_)
    {
        other.conn_ = nullptr;   // 转移所有权后置空，防止 double-release
    }

private:
    Connection*     conn_;   // 裸指针，生命周期由 pool 管理
    ConnectionPool& pool_;
};

// ── ConnectionPool：核心类 ───────────────────────────────────────
class ConnectionPool {
public:
    struct Config {
        std::string  host;
        std::string  user;
        std::string  password;
        std::string  db;
        unsigned int port      = 3306;
        int          pool_size = 5;     // 预创建连接数
    };

    // 构造时预创建所有连接，失败直接抛异常
    explicit ConnectionPool(const Config& cfg);

    // 析构时 pool_ 里的 unique_ptr 会自动 delete → mysql_close
    ~ConnectionPool() = default;

    // 禁止拷贝和移动（pool 是单例资源）
    ConnectionPool(const ConnectionPool&)            = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&)                 = delete;
    ConnectionPool& operator=(ConnectionPool&&)      = delete;

    // 获取一条空闲连接（阻塞，直到有连接可用）
    // 返回 ConnectionGuard，出作用域自动归还
    ConnectionGuard acquire();

    // 归还连接（由 ConnectionGuard 析构调用，不建议直接调用）
    void release(Connection* conn);

    // 当前空闲连接数（调试用）
    int available() const;

private:
    Config                                  cfg_;
    mutable std::mutex                      mutex_;
    std::condition_variable                 cond_;
    std::deque<std::unique_ptr<Connection>> pool_;   // 空闲连接队列
};
