// 第二周 Day12：Reactor 模式
// 学习目标：实现 EventLoop + Channel + Acceptor 三个核心类
// Reactor 是 libevent/muduo/nginx 的底层架构思想
//
// Reactor 三要素：
//   1. EventLoop：事件循环（封装 epoll_wait）
//   2. Channel：封装 fd 和它关心的事件 + 回调
//   3. Acceptor：专门处理新连接（监听 fd 的 Channel）

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

// ============================================================
// Channel：封装 fd 的事件和回调
// 每个 fd（listenfd / connfd）对应一个 Channel
// ============================================================

class EventLoop; // 前向声明

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd), events_(0), revents_(0) {}

    ~Channel() = default;

    // 设置关心的事件
    void enableReading()  { events_ |= EPOLLIN;  update(); }
    void enableWriting()  { events_ |= EPOLLOUT; update(); }
    void disableAll()     { events_ = 0;          update(); }

    // 设置回调
    void setReadCallback(EventCallback cb)  { read_cb_  = std::move(cb); }
    void setWriteCallback(EventCallback cb) { write_cb_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { close_cb_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { error_cb_ = std::move(cb); }

    // epoll 返回的实际事件（由 EventLoop 设置）
    void setRevents(uint32_t revents) { revents_ = revents; }

    // 根据 revents 分发到对应回调
    void handleEvent() {
        if (revents_ & (EPOLLHUP | EPOLLRDHUP) && !(revents_ & EPOLLIN)) {
            if (close_cb_) close_cb_();
        }
        if (revents_ & EPOLLERR) {
            if (error_cb_) error_cb_();
        }
        if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
            if (read_cb_) read_cb_();
        }
        if (revents_ & EPOLLOUT) {
            if (write_cb_) write_cb_();
        }
    }

    int fd() const { return fd_; }
    uint32_t events() const { return events_; }

private:
    void update(); // 通知 EventLoop 更新 epoll（在 .cpp 中定义，避免循环依赖）

    EventLoop* loop_;
    int fd_;
    uint32_t events_;  // 关心的事件（EPOLLIN | EPOLLOUT）
    uint32_t revents_; // epoll 返回的实际事件

    EventCallback read_cb_;
    EventCallback write_cb_;
    EventCallback close_cb_;
    EventCallback error_cb_;
};

// ============================================================
// EventLoop：事件循环（封装 epoll）
// 核心：loop() 函数不断 epoll_wait，将事件分发给对应的 Channel
// ============================================================

class EventLoop {
public:
    EventLoop() : quit_(false) {
        epfd_ = epoll_create1(0);
        if (epfd_ < 0) {
            perror("epoll_create1");
            abort();
        }
    }

    ~EventLoop() { close(epfd_); }

    // 核心事件循环
    void loop() {
        const int MAX_EVENTS = 1024;
        struct epoll_event events[MAX_EVENTS];

        while (!quit_) {
            int nready = epoll_wait(epfd_, events, MAX_EVENTS, -1);
            if (nready < 0) {
                if (errno == EINTR) continue;
                perror("epoll_wait");
                break;
            }

            for (int i = 0; i < nready; ++i) {
                int fd = events[i].data.fd;
                auto it = channels_.find(fd);
                if (it != channels_.end()) {
                    it->second->setRevents(events[i].events);
                    it->second->handleEvent(); // 分发事件到 Channel 的回调
                }
            }
        }
    }

    void quit() { quit_ = true; }

    // 注册 Channel（由 Channel::update 调用）
    void updateChannel(Channel* channel) {
        int fd = channel->fd();
        struct epoll_event ev{};
        ev.events = channel->events();
        ev.data.fd = fd;

        if (channels_.count(fd)) {
            epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
        } else {
            epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
            channels_[fd] = channel;
        }
    }

    // 移除 Channel
    void removeChannel(Channel* channel) {
        int fd = channel->fd();
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        channels_.erase(fd);
    }

private:
    int epfd_;
    bool quit_;
    std::unordered_map<int, Channel*> channels_; // fd -> Channel 映射
};

// Channel::update 的实现（需要在 EventLoop 定义后）
void Channel::update() {
    loop_->updateChannel(this);
}

// ============================================================
// Acceptor：专门处理新连接
// 封装监听 fd 的 Channel，当有新连接时调用 new_conn_callback_
// ============================================================

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int connfd, struct sockaddr_in)>;

    Acceptor(EventLoop* loop, int port) : loop_(loop) {
        listenfd_ = create_listen_socket(port);
        channel_ = std::make_unique<Channel>(loop_, listenfd_);
        channel_->setReadCallback([this] { handleAccept(); });
        channel_->enableReading();
        std::cout << "[Acceptor] 监听端口 " << port << std::endl;
    }

    ~Acceptor() {
        loop_->removeChannel(channel_.get());
        close(listenfd_);
    }

    void setNewConnectionCallback(NewConnectionCallback cb) {
        new_conn_cb_ = std::move(cb);
    }

private:
    void handleAccept() {
        struct sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(listenfd_, (struct sockaddr*)&client_addr, &addrlen);
        if (connfd < 0) {
            perror("accept");
            return;
        }
        set_nonblocking(connfd);
        std::cout << "[Acceptor] 新连接 fd=" << connfd << std::endl;
        if (new_conn_cb_) new_conn_cb_(connfd, client_addr);
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

    EventLoop* loop_;
    int listenfd_;
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback new_conn_cb_;
};

// ============================================================
// 用 Reactor 框架构建 echo 服务器
// ============================================================

class EchoServer {
public:
    EchoServer(EventLoop* loop, int port) : loop_(loop), acceptor_(loop, port) {
        acceptor_.setNewConnectionCallback(
            [this](int connfd, struct sockaddr_in addr) {
                onNewConnection(connfd, addr);
            }
        );
    }

private:
    void onNewConnection(int connfd, struct sockaddr_in /*addr*/) {
        auto channel = std::make_shared<Channel>(loop_, connfd);

        channel->setReadCallback([connfd, channel, this]() {
            char buf[4096];
            ssize_t n = recv(connfd, buf, sizeof(buf), 0);
            if (n <= 0) {
                std::cout << "[Echo] fd=" << connfd << " 断开" << std::endl;
                loop_->removeChannel(channel.get());
                channels_.erase(connfd);
                close(connfd);
            } else {
                buf[n] = '\0';
                std::cout << "[Echo] fd=" << connfd << " 收到: " << buf;
                send(connfd, buf, n, 0);
            }
        });

        channel->setCloseCallback([connfd, channel, this]() {
            loop_->removeChannel(channel.get());
            channels_.erase(connfd);
            close(connfd);
        });

        channel->enableReading();
        channels_[connfd] = channel; // 保存 channel 延长生命期
    }

    EventLoop* loop_;
    Acceptor acceptor_;
    std::unordered_map<int, std::shared_ptr<Channel>> channels_;
};

int main() {
    EventLoop loop;
    EchoServer server(&loop, 8890);
    std::cout << "[Reactor Echo] 启动，端口 8890" << std::endl;
    loop.loop();
    return 0;
}

// ============================================================
// 架构理解（画图）：
//
//   main()
//    └── EventLoop::loop()          ← 事件循环
//         └── epoll_wait()          ← 等待事件
//              └── Channel::handleEvent()   ← 分发事件
//                   ├── Acceptor 回调 → accept → 创建 Channel
//                   └── 连接 Channel 回调 → recv → 业务处理
//
// 自测问题（Day12 结束前回答）
// Q1: Channel 和 fd 是什么关系？一个 fd 对应几个 Channel？
fd 是内核中的文件描述符，而 Channel 是对 fd 的封装，里面不仅有 fd，还有关注的事件和回调函数。通常一个 fd 对应一个 Channel，否则容易出现事件管理混乱。
// Q2: Reactor 和 Proactor 的核心区别是什么？
Reactor 是“通知你可以读写了，真正 IO 自己做”；Proactor 是“内核已经帮你做完 IO，再通知你结果”。前者是 IO 就绪通知，后者是 IO 完成通知。
// Q3: EventLoop 为什么不直接存 fd，而是存 Channel*？
因为 EventLoop 不只是管理 fd，还要管理事件和回调，而这些都封装在 Channel 里，所以直接管理 Channel 更方便，也更符合面向对象设计。
// Q4: 如果要支持多线程，EventLoop 需要做哪些修改？
需要加入线程安全机制（mutex）、任务队列、跨线程唤醒（eventfd），并保证一个 EventLoop 只由一个线程负责处理 IO。

// ============================================================
