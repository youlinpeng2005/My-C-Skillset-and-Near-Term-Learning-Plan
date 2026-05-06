// 第二周 Day9：epoll 底层原理 + 最小化 echo 服务器
// 学习目标：理解 epoll_create/epoll_ctl/epoll_wait 三板斧，写出可运行的 echo 服务器
// 运行环境：Linux（在 WSL 或 Linux 机器上编译运行）
// 编译命令：g++ -std=c++17 -Wall day9_epoll_echo_server.cpp -o echo_server

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

// ============================================================
// 工具函数
// ============================================================

// 将 fd 设置为非阻塞模式（epoll 必须配合非阻塞使用，尤其是 ET 模式）
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 添加 fd 到 epoll 监视（LT 模式）
bool epoll_add(int epfd, int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

// 从 epoll 移除 fd
bool epoll_del(int epfd, int fd) {
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

// ============================================================
// 创建监听 socket
// ============================================================

int create_listen_socket(int port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        return -1;
    }

    // SO_REUSEADDR：允许重用 TIME_WAIT 状态的端口（开发时必设）
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listenfd);
        return -1;
    }

    if (listen(listenfd, 128) < 0) { // 128 = 全连接队列大小
        perror("listen");
        close(listenfd);
        return -1;
    }

    set_nonblocking(listenfd);
    return listenfd;
}

// ============================================================
// epoll echo 服务器（LT 模式，单线程）
//
// 流程：
//   1. epoll_create 创建 epoll 实例
//   2. 将 listenfd 加入 epoll 监视 EPOLLIN 事件
//   3. 进入事件循环：epoll_wait 等待事件
//      - listenfd 可读：accept 新连接，加入 epoll
//      - connfd 可读：recv 数据，echo 回去
//      - connfd 断开：close，从 epoll 删除
// ============================================================

static const int PORT = 8888;
static const int MAX_EVENTS = 1024;
static const int BUF_SIZE = 4096;

int main() {
    int listenfd = create_listen_socket(PORT);
    if (listenfd < 0) return 1;
    std::cout << "[服务器] 监听端口 " << PORT << std::endl;

    // Step1: 创建 epoll 实例（内核创建红黑树 + 就绪链表）
    int epfd = epoll_create1(0); // 参数 0 等同于 epoll_create(size)
    if (epfd < 0) {
        perror("epoll_create1");
        return 1;
    }

    // Step2: 将监听 fd 加入 epoll（EPOLLIN = 可读事件）
    epoll_add(epfd, listenfd, EPOLLIN);

    struct epoll_event events[MAX_EVENTS];
    char buf[BUF_SIZE];

    std::cout << "[服务器] 进入事件循环（用 telnet 127.0.0.1 8888 测试）" << std::endl;

    while (true) {
        // Step3: 等待事件（-1 = 永久等待）
        // 内核扫描就绪链表，将就绪的 epoll_event 拷贝到 events 数组
        int nready = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nready < 0) {
            if (errno == EINTR) continue; // 被信号中断，继续
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nready; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == listenfd) {
                // ---- 新连接到来 ----
                struct sockaddr_in client_addr{};
                socklen_t addrlen = sizeof(client_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &addrlen);
                if (connfd < 0) {
                    perror("accept");
                    continue;
                }
                set_nonblocking(connfd);
                epoll_add(epfd, connfd, EPOLLIN);

                std::cout << "[连接] 新客户端 fd=" << connfd
                          << " 来自 " << inet_ntoa(client_addr.sin_addr)
                          << ":" << ntohs(client_addr.sin_port) << std::endl;

            } else if (ev & EPOLLIN) {
                // ---- 客户端有数据可读 ----
                ssize_t n = recv(fd, buf, BUF_SIZE - 1, 0);
                if (n <= 0) {
                    if (n == 0 || errno != EAGAIN) {
                        // n == 0：对端关闭连接
                        // errno != EAGAIN：真正的错误
                        std::cout << "[断开] fd=" << fd << std::endl;
                        epoll_del(epfd, fd);
                        close(fd);
                    }
                    continue;
                }
                buf[n] = '\0';
                std::cout << "[消息] fd=" << fd << " 说: " << buf;

                // Echo 回去
                send(fd, buf, n, 0);

            } else if (ev & (EPOLLHUP | EPOLLERR)) {
                // ---- 连接异常 ----
                std::cout << "[异常] fd=" << fd << std::endl;
                epoll_del(epfd, fd);
                close(fd);
            }
        }
    }

    close(epfd);
    close(listenfd);
    return 0;
}

// ============================================================
// 测试方法：
//   终端1: ./echo_server
//   终端2: telnet 127.0.0.1 8888
//   输入任意文字，服务器会 echo 回来
//
// 自测问题（Day9 结束前回答）
// Q1: epoll_create 的参数 size 有什么意义？现在还有用吗？
epoll_create 的 size 在早期用于提示内核 fd 数量，现在已被忽略，内核采用动态结构管理，实际开发中通常使用 epoll_create1
// Q2: 为什么监听 fd 要设置 SO_REUSEADDR？不设置会有什么问题？
SO_REUSEADDR 允许在端口处于 TIME_WAIT 状态时重新绑定，否则服务重启时可能出现 Address already in use 错误,服务重启失败。
// Q3: epoll_wait 返回 0 意味着什么？
epoll_wait 返回 0 表示在指定 timeout 时间内没有任何事件发生，即超时返回
// Q4: LT 模式下，如果 recv 只读了部分数据，下次 epoll_wait 还会通知吗？
LT 模式下，如果 recv 只读了部分数据，下次 epoll_wait 还会通知，因为 LT 模式会持续通知，直到数据被读完。
// ============================================================
