// 第三周 Day20（5月8日）：Reactor + 线程池结合
// 这是 One Reactor + Thread Pool 经典模型
// 主线程：epoll_wait + accept（IO事件分发）
// 工作线程：处理业务逻辑（recv + 业务 + send）
//
// 架构图：
//   主线程
//   └── EventLoop::loop()
//        └── epoll_wait()
//             ├── 新连接 -> accept -> 创建 Channel -> 加入 epoll
//             └── 可读事件 -> submit 到线程池
//                              └── 工作线程: recv -> 业务 -> send

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include <vector>
#include <atomic>

// ============================================================
// ThreadPool（复用 Day17 的实现）
// ============================================================

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stopped_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_.wait(lock, [this] { return !tasks_.empty() || stopped_; });
                        if (stopped_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        { std::unique_lock<std::mutex> lock(mutex_); stopped_ = true; }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }

    void submit(std::function<void()> task) {
        { std::unique_lock<std::mutex> lock(mutex_); tasks_.push(std::move(task)); }
        cv_.notify_one();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    bool stopped_;
};

// ============================================================
// 工具函数
// ============================================================

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int create_listen_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(fd, 128);
    set_nonblocking(fd);
    return fd;
}

// ============================================================
// MultiThreadChatServer：Reactor + 线程池版聊天服务器
// ============================================================

class MultiThreadChatServer {
public:
    MultiThreadChatServer(int port, int num_workers)
        : pool_(num_workers) {

        epfd_ = epoll_create1(0);
        listenfd_ = create_listen_socket(port);
        epoll_add(listenfd_, EPOLLIN);

        std::cout << "[服务器] 启动，端口=" << port
                  << "，工作线程=" << num_workers << std::endl;
    }

    ~MultiThreadChatServer() {
        close(epfd_);
        close(listenfd_);
    }

    void run() {
        const int MAX_EVENTS = 1024;
        struct epoll_event events[MAX_EVENTS];

        while (true) {
            int nready = epoll_wait(epfd_, events, MAX_EVENTS, -1);
            if (nready < 0) {
                if (errno == EINTR) continue;
                break;
            }

            for (int i = 0; i < nready; ++i) {
                int fd = events[i].data.fd;

                if (fd == listenfd_) {
                    // 新连接：在主线程 accept（快速操作）
                    handleAccept();
                } else if (events[i].events & EPOLLIN) {
                    // IO 事件：提交到线程池处理
                    pool_.submit([this, fd] {
                        handleMessage(fd);
                    });
                }
            }
        }
    }

private:
    void handleAccept() {
        struct sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(listenfd_, (struct sockaddr*)&client_addr, &addrlen);
        if (connfd < 0) return;

        set_nonblocking(connfd);
        epoll_add(connfd, EPOLLIN);

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.insert(connfd);
        }

        std::cout << "[连接] fd=" << connfd
                  << " 来自 " << inet_ntoa(client_addr.sin_addr) << std::endl;

        std::string welcome = "欢迎！你是客户端 " + std::to_string(connfd) + "\r\n";
        send(connfd, welcome.c_str(), welcome.size(), 0);
    }

    void handleMessage(int fd) {
        char buf[4096];
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);

        if (n <= 0) {
            std::cout << "[断开] fd=" << fd << std::endl;
            removeClient(fd);
            return;
        }

        buf[n] = '\0';
        std::string msg(buf);
        std::string formatted = "[fd=" + std::to_string(fd) + "]: " + msg;
        std::cout << formatted;

        // 广播（需要加锁保护 clients_）
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (int client : clients_) {
            if (client != fd) {
                send(client, formatted.c_str(), formatted.size(), 0);
            }
        }
    }

    void removeClient(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.erase(fd);
        }
        close(fd);
    }

    void epoll_add(int fd, uint32_t events) {
        struct epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    }

    int epfd_;
    int listenfd_;
    ThreadPool pool_;

    std::mutex clients_mutex_; // 保护 clients_ 的并发访问
    std::unordered_set<int> clients_;
};

int main() {
    // 4 个工作线程
    MultiThreadChatServer server(8892, 4);
    server.run();
    return 0;
}

// ============================================================
// 思考题（Day20 结束前回答）
// Q1: handleMessage 中加锁广播时，如果某个 client 的 send 阻塞了怎么办？
// Q2: 多个工作线程同时执行 handleMessage，这里是线程安全的吗？
// Q3: 如果 epoll 也要多线程化（多 Reactor），架构怎么变？
//     （提示：muduo 的 one loop per thread 模型）
// Q4: 现在的广播逻辑在工作线程执行，但 epoll_add/del 在主线程，有线程安全问题吗？
// ============================================================
