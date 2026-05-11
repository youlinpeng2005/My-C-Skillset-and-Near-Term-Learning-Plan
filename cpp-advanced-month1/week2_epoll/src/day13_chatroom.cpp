// 第二周 Day13：单线程多客户端聊天室（epoll 综合实践）
// 这是多线程改造前的基线版本，第四周项目的起点
// 功能：多客户端连接，任意客户端发消息，广播给所有其他客户端
// 编译：g++ -std=c++17 day13_chatroom.cpp -o chatroom
// 测试：telnet 127.0.0.1 8891

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
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

static const int PORT = 8891;
static const int MAX_EVENTS = 1024;
static const int BUF_SIZE = 4096;

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
// ChatRoom：聊天室核心逻辑
// ============================================================

class ChatRoom {
public:
    ChatRoom() {
        epfd_ = epoll_create1(0);
        listenfd_ = create_listen_socket(PORT);
        addToEpoll(listenfd_, EPOLLIN);
        std::cout << "[聊天室] 启动，端口 " << PORT << std::endl;
        std::cout << "[聊天室] 用 'telnet 127.0.0.1 " << PORT << "' 加入" << std::endl;
    }

    ~ChatRoom() {
        close(epfd_);
        close(listenfd_);
    }

    void run() {
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
                    handleNewConnection();
                } else {
                    handleMessage(fd);
                }
            }
        }
    }

private:
    void handleNewConnection() {
        struct sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(listenfd_, (struct sockaddr*)&client_addr, &addrlen);
        if (connfd < 0) return;

        set_nonblocking(connfd);
        addToEpoll(connfd, EPOLLIN);
        clients_.insert(connfd);

        std::string ip = inet_ntoa(client_addr.sin_addr);
        nicknames_[connfd] = "用户" + std::to_string(connfd);
        std::cout << "[连接] " << nicknames_[connfd] << " 加入 (fd=" << connfd << ", ip=" << ip << ")" << std::endl;

        // 欢迎消息
        std::string welcome = "欢迎加入聊天室！你是 " + nicknames_[connfd] + "\r\n";
        send(connfd, welcome.c_str(), welcome.size(), 0);

        // 通知其他人
        broadcast(connfd, nicknames_[connfd] + " 加入了聊天室\r\n");
    }

    void handleMessage(int fd) {
        char buf[BUF_SIZE];
        ssize_t n = recv(fd, buf, BUF_SIZE - 1, 0);

        if (n <= 0) {
            // 客户端断开
            std::cout << "[断开] " << nicknames_[fd] << " (fd=" << fd << ")" << std::endl;
            broadcast(fd, nicknames_[fd] + " 离开了聊天室\r\n");
            removeClient(fd);
            return;
        }

        buf[n] = '\0';
        std::string msg(buf);
        // 去掉末尾的 \r\n
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n')) {
            msg.pop_back();
        }

        if (msg.empty()) return;

        // 支持修改昵称命令：/nick 新名字
        if (msg.substr(0, 6) == "/nick ") {
            std::string new_name = msg.substr(6);
            std::string old_name = nicknames_[fd];
            nicknames_[fd] = new_name;
            std::cout << "[改名] " << old_name << " -> " << new_name << std::endl;
            broadcast(-1, old_name + " 改名为 " + new_name + "\r\n");
            return;
        }

        // 普通消息：广播给所有人
        std::string formatted = "[" + nicknames_[fd] + "]: " + msg + "\r\n";
        std::cout << formatted;
        broadcast(fd, formatted);
    }

    // 广播消息给所有客户端（除了 exclude_fd）
    // exclude_fd = -1 表示广播给所有人
    void broadcast(int exclude_fd, const std::string& msg) {
        for (int fd : clients_) {
            if (fd != exclude_fd) {
                send(fd, msg.c_str(), msg.size(), 0);
            }
        }
    }

    void addToEpoll(int fd, uint32_t events) {
        struct epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    }

    void removeClient(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        clients_.erase(fd);
        nicknames_.erase(fd);
        close(fd);
    }

    int epfd_;
    int listenfd_;
    std::unordered_set<int> clients_;
    std::unordered_map<int, std::string> nicknames_;
};

int main() {
    ChatRoom room;
    room.run();
    return 0;
}

// ============================================================
// 测试方法：
//   终端1: ./chatroom
//   终端2: telnet 127.0.0.1 8891  （用户1）
//   终端3: telnet 127.0.0.1 8891  （用户2）
//   在任意终端输入消息，另一个终端应该看到
//   输入 /nick 张三 可以改变昵称
//
// 这就是多线程改造前的基线版本！
// 第四周任务：用 epoll + 线程池改造这个服务器
//
// 已知限制（第四周改造点）：
//   - 单线程，recv 和广播都在主线程，高并发时会成为瓶颈
//   - 没有协议（粘包未处理）
//   - 没有日志
//   - 没有心跳
// ============================================================
