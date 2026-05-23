// 第一周 Day2：RAII思想 + unique_ptr 实战
// 学习目标：理解 RAII 的本质，用 unique_ptr 封装资源，保证异常安全
// 重点：析构函数是 RAII 的核心，资源获取即初始化

#include <iostream>
#include <memory>
#include <stdexcept>
#include <fcntl.h>

// ============================================================
// Part 1: RAII 本质理解
// 核心思想：在构造函数中获取资源，在析构函数中释放资源
// 这样无论是正常退出还是异常退出，资源都能被释放
// ============================================================

// 反例：裸指针管理，异常时泄漏
void bad_resource_management() {
    int* data = new int[1000];
    // ... 如果这里抛出异常，delete 永远不会执行
    // throw std::runtime_error("something went wrong");
    delete[] data; // 依赖程序员记住释放
}

// RAII 封装文件描述符（Linux fd）
// 这正是你的聊天服务器需要的：将 int fd 包装成 RAII 类
class FileDescriptor {
public:
    explicit FileDescriptor(int fd) : fd_(fd) {
        if (fd_ < 0) throw std::runtime_error("Invalid fd");
        std::cout << "FileDescriptor 构造，fd = " << fd_ << std::endl;
    }

    ~FileDescriptor() {
        if (fd_ >= 0) {
            // close(fd_);  // 真实代码中调用 close
            std::cout << "FileDescriptor 析构，fd = " << fd_ << " 已关闭" << std::endl;
            fd_ = -1;
        }
    }

    // 禁止拷贝（fd 不能共享所有权）
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    // 允许移动
    FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    int get() const { return fd_; }

    // 释放所有权（不关闭 fd，只是放弃管理）
    int release() {
        int tmp = fd_;
        fd_ = -1;
        return tmp;
    }

private:
    int fd_;
};

void demo_fd_raii() {
    std::cout << "=== RAII 封装文件描述符 ===" << std::endl;
    {
        FileDescriptor fd(5); // 模拟 accept() 返回的 fd
        std::cout << "使用 fd: " << fd.get() << std::endl;
        // 离开作用域自动关闭，即使抛出异常也会关闭
    }
    std::cout << "fd 已自动关闭" << std::endl;
}

// ============================================================
// Part 2: RAII + 锁（这是你写线程池时要用的模式）
// ============================================================

#include <mutex>

class MutexLockGuard {
public:
    explicit MutexLockGuard(std::mutex& mutex) : mutex_(mutex) {
        mutex_.lock();
    }
    ~MutexLockGuard() {
        mutex_.unlock();
    }
    // 禁止拷贝
    MutexLockGuard(const MutexLockGuard&) = delete;
    MutexLockGuard& operator=(const MutexLockGuard&) = delete;

private:
    std::mutex& mutex_;
};

std::mutex g_mutex;

void thread_safe_operation() {
    MutexLockGuard lock(g_mutex); // 加锁
    std::cout << "在锁保护下执行操作" << std::endl;
    // 离开作用域自动解锁，等价于 std::lock_guard<std::mutex>
}

// ============================================================
// Part 3: unique_ptr 实战 —— 工厂模式中的内存安全
// ============================================================

class Connection {
public:
    explicit Connection(int fd) : fd_(fd) {
        std::cout << "Connection 建立，fd = " << fd_ << std::endl;
    }
    ~Connection() {
        std::cout << "Connection 断开，fd = " << fd_ << std::endl;
    }
    void send(const std::string& msg) {
        std::cout << "[fd=" << fd_ << "] 发送: " << msg << std::endl;
    }
    int getFd() const { return fd_; }

private:
    int fd_;
};

// 工厂函数：返回 unique_ptr，明确转移所有权
std::unique_ptr<Connection> createConnection(int fd) {
    return std::make_unique<Connection>(fd);
}

void demo_unique_ptr_factory() {
    std::cout << "\n=== unique_ptr 工厂模式 ===" << std::endl;
    auto conn = createConnection(42);
    conn->send("hello");
    // conn 离开作用域，Connection 自动析构，无需手动 delete
}

// ============================================================
// Part 4: 将 unique_ptr 用于聊天服务器连接管理
// 在你的实际项目中，连接列表应该这样管理
// ============================================================

#include <unordered_map>

class ChatServer {
public:
    void onNewConnection(int fd) {
        // 用 unique_ptr 管理每个 Connection，存入 map
        connections_[fd] = std::make_unique<Connection>(fd);
        std::cout << "新连接加入，当前连接数: " << connections_.size() << std::endl;
    }

    void onDisconnection(int fd) {
        connections_.erase(fd); // erase 时 unique_ptr 析构，Connection 自动释放
        std::cout << "连接断开，当前连接数: " << connections_.size() << std::endl;
    }

    void broadcast(const std::string& msg) {
        for (auto& [fd, conn] : connections_) {
            conn->send(msg);
        }
    }

private:
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
};

void demo_chat_server() {
    std::cout << "\n=== 聊天服务器连接管理（RAII版） ===" << std::endl;
    ChatServer server;
    server.onNewConnection(1);
    server.onNewConnection(2);
    server.onNewConnection(3);
    server.broadcast("欢迎加入聊天室！");
    server.onDisconnection(2);
    server.broadcast("用户2已离线");
    // server 析构时，所有 Connection 自动释放
}

// ============================================================
// 自测问题（Day2 结束前回答）
// Q1: 为什么 FileDescriptor 要禁止拷贝？如果需要共享 fd 怎么办？
// Q2: std::lock_guard 和 std::unique_lock 有什么区别？
// Q3: 什么情况下 RAII 对象的析构函数不会被调用？（提示：exit()、abort()、signal）
// ============================================================

//A1:
//	因为FileDescriptor同时只能有一个指针访问该资源，如果允许拷贝，则会导致多个指针访问该资源，从而导致资源安全问题（double close未提到），共享考虑封装shared_ptr<FileDescriptor>
//A2:lock_guard构建时加锁，析构时加锁，不能手动加锁，而unique_lock可随程序员手动加锁解锁，自由度高
//A3:1. 基类未设置虚析构，而调用基类析构函数导致资源安全问题，2.程序未正常退出如任务管理器手动关闭程序

//参考答案
//A1:
// 禁止拷贝的核心原因是防止【双重关闭（double close）】：
// 若允许拷贝，两个 FileDescriptor 对象持有同一个 fd，析构时各自 close() 一次，
// 第二次 close 会操作一个已被内核重新分配给其他文件的 fd，造成严重 bug。
// 若需要共享 fd，应使用 shared_ptr<FileDescriptor>，
// 由引用计数管理生命周期，只有最后一个持有者析构时才真正 close。

//A2:
// lock_guard：构造时加锁，析构时解锁，不支持手动控制，开销极小，优先使用。
// unique_lock：同样构造加锁/析构解锁，但额外支持：
//   1. 延迟加锁（std::defer_lock）
//   2. 手动 lock() / unlock()
//   3. 配合条件变量使用（condition_variable::wait() 只接受 unique_lock）
//   4. 所有权可移动转移
// 代价：内部多一个 bool 标记是否持有锁，略有额外开销。
// 实践原则：简单临界区用 lock_guard，需要条件变量或分段加锁用 unique_lock。

//A3:
// 以下情况 RAII 对象的析构函数不会被调用：
//   1. 调用 std::exit() 或 std::abort()：直接终止进程，栈上局部对象不析构
//   2. 收到 SIGKILL 信号（kill -9）：内核强制终止，析构函数无法执行
//   3. std::terminate() 被调用（如异常传播到 noexcept 函数外）
//   4. 基类未声明虚析构函数，通过基类指针 delete 派生类对象时，
//      派生类析构函数不会被调用（调用了错误的析构函数）
int main() {
    demo_fd_raii();
    thread_safe_operation();
    demo_unique_ptr_factory();
    demo_chat_server();

    std::cout << "\n=== Day2 完成！明天任务：move语义 + 右值引用 ===" << std::endl;
    return 0;
}
