// 第四周：工程级聊天服务器（5月10日~5月16日逐步完善）
// 技术栈：epoll + 线程池 + 自定义协议 + 异步日志 + 心跳检测
// 编译：g++ -std=c++17 -Wall -I../base -I../net chat_server.cpp -o chat_server -lpthread

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <chrono>
#include <sstream>

#include "../base/logger.h"
#include "../base/thread_pool.h"
#include "../net/protocol.h"

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

static const int PORT = 8893;
static const int MAX_EVENTS = 1024;
static const int BUF_SIZE = 65536;
static const int HEARTBEAT_TIMEOUT_SEC = 30; // 30秒无心跳断开

// ============================================================
// 连接信息
// ============================================================

struct Connection {
    int fd;
    std::string ip;
    std::string nickname;
    TimePoint last_active;   // 最后活跃时间（心跳检测用）
    PacketParser parser;     // 每个连接有自己的粘包处理器

    explicit Connection(int fd, const std::string& ip)
        : fd(fd), ip(ip),
          nickname("用户" + std::to_string(fd)),
          last_active(Clock::now()) {}
};

// ============================================================
// ChatServer：工程级聊天服务器
// ============================================================

class ChatServer {
public:
    ChatServer(int port, int num_workers)
        : pool_(num_workers), running_(false) {

        epfd_ = epoll_create1(0);
        listenfd_ = create_listen_socket(port);
        epoll_add(listenfd_, EPOLLIN);

        LOG_INFO("服务器启动，端口=" + std::to_string(port)
                 + "，工作线程=" + std::to_string(num_workers));
    }

    ~ChatServer() {
        close(epfd_);
        close(listenfd_);
    }

    void run() {
        running_ = true;
        struct epoll_event events[MAX_EVENTS];

        LOG_INFO("进入事件循环");

        while (running_) {
            // 超时 5 秒检查一次心跳（不用 -1 永久等待）
            int nready = epoll_wait(epfd_, events, MAX_EVENTS, 5000);

            if (nready < 0) {
                if (errno == EINTR) continue;
                LOG_ERROR("epoll_wait 错误: " + std::string(strerror(errno)));
                break;
            }

            if (nready == 0) {
                // 超时：检查心跳
                check_heartbeat();
                continue;
            }

            for (int i = 0; i < nready; ++i) {
                int fd = events[i].data.fd;

                if (fd == listenfd_) {
                    handleAccept();
                } else if (events[i].events & EPOLLIN) {
                    // 提交到线程池处理（注意：epoll 相关操作仍在主线程）
                    pool_.submit_void([this, fd] {
                        handleMessage(fd);
                    });
                } else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    removeConnection(fd);
                }
            }
        }
    }

    void stop() { running_ = false; }

private:
    void handleAccept() {
        struct sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(listenfd_, (struct sockaddr*)&client_addr, &addrlen);
        if (connfd < 0) {
            LOG_WARN("accept 失败: " + std::string(strerror(errno)));
            return;
        }

        set_nonblocking(connfd);
        epoll_add(connfd, EPOLLIN);

        std::string ip = inet_ntoa(client_addr.sin_addr);
        {
            std::lock_guard<std::mutex> lock(conns_mutex_);
            conns_[connfd] = std::make_shared<Connection>(connfd, ip);
        }

        LOG_INFO("新连接 fd=" + std::to_string(connfd) + " ip=" + ip);

        // 发送欢迎消息
        Message welcome;
        welcome.type = MsgType::CHAT;
        welcome.payload = "欢迎加入聊天室！";
        send_message(connfd, welcome);
    }

    void handleMessage(int fd) {
        char buf[BUF_SIZE];
        ssize_t n = recv(fd, buf, BUF_SIZE - 1, 0);

        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
            LOG_INFO("连接关闭 fd=" + std::to_string(fd));
            removeConnection(fd);
            return;
        }

        // 更新活跃时间
        {
            std::lock_guard<std::mutex> lock(conns_mutex_);
            auto it = conns_.find(fd);
            if (it != conns_.end()) {
                it->second->last_active = Clock::now();
                it->second->parser.append(buf, n);
            }
        }

        // 解析消息
        Message msg;
        bool has_msg = false;
        {
            std::lock_guard<std::mutex> lock(conns_mutex_);
            auto it = conns_.find(fd);
            if (it != conns_.end()) {
                has_msg = it->second->parser.try_parse(msg);
            }
        }

        if (!has_msg) return;

        switch (msg.type) {
            case MsgType::HEARTBEAT:
                LOG_DEBUG("收到心跳 fd=" + std::to_string(fd));
                break;

            case MsgType::CHAT: {
                std::string nickname;
                {
                    std::lock_guard<std::mutex> lock(conns_mutex_);
                    auto it = conns_.find(fd);
                    if (it != conns_.end()) nickname = it->second->nickname;
                }
                std::string formatted = "[" + nickname + "]: " + msg.payload;
                LOG_INFO(formatted);
                broadcast(fd, formatted);
                break;
            }

            case MsgType::LOGIN:
                // 设置昵称
                {
                    std::lock_guard<std::mutex> lock(conns_mutex_);
                    auto it = conns_.find(fd);
                    if (it != conns_.end()) {
                        it->second->nickname = msg.payload;
                    }
                }
                LOG_INFO("fd=" + std::to_string(fd) + " 设置昵称: " + msg.payload);
                break;

            default:
                LOG_WARN("未知消息类型 fd=" + std::to_string(fd));
                break;
        }
    }

    void broadcast(int exclude_fd, const std::string& content) {
        Message msg;
        msg.type = MsgType::CHAT;
        msg.payload = content;

        std::lock_guard<std::mutex> lock(conns_mutex_);
        for (auto& [fd, conn] : conns_) {
            if (fd != exclude_fd) {
                send_message(fd, msg);
            }
        }
    }

    void send_message(int fd, const Message& msg) {
        auto buf = msg.serialize();
        // 注意：send 在多线程下对同一 fd 可能有竞态，生产环境应加 per-fd 写锁
        send(fd, buf.data(), buf.size(), MSG_NOSIGNAL);
    }

    // 心跳检测：移除超时的连接
    void check_heartbeat() {
        auto now = Clock::now();
        std::vector<int> timeout_fds;

        {
            std::lock_guard<std::mutex> lock(conns_mutex_);
            for (auto& [fd, conn] : conns_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - conn->last_active).count();
                if (elapsed > HEARTBEAT_TIMEOUT_SEC) {
                    timeout_fds.push_back(fd);
                }
            }
        }

        for (int fd : timeout_fds) {
            LOG_WARN("心跳超时，断开 fd=" + std::to_string(fd));
            removeConnection(fd);
        }
    }

    void removeConnection(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        {
            std::lock_guard<std::mutex> lock(conns_mutex_);
            conns_.erase(fd);
        }
        close(fd);
    }

    void epoll_add(int fd, uint32_t events) {
        struct epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    }

    static int set_nonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    static int create_listen_socket(int port) {
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

    int epfd_;
    int listenfd_;
    ThreadPool pool_;
    std::atomic<bool> running_;

    std::mutex conns_mutex_;
    std::unordered_map<int, std::shared_ptr<Connection>> conns_;
};

// 信号处理
static ChatServer* g_server = nullptr;
void signal_handler(int /*sig*/) {
    if (g_server) g_server->stop();
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); // 忽略 broken pipe

    ChatServer server(PORT, std::thread::hardware_concurrency());
    g_server = &server;

    LOG_INFO("服务器已启动，按 Ctrl+C 停止");
    server.run();
    LOG_INFO("服务器已停止");

    return 0;
}
