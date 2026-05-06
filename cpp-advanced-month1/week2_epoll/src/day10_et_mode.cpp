// 第二周 Day10：非阻塞IO + ET（边沿触发）模式实战
// 学习目标：理解 ET vs LT 的本质区别，掌握 ET 下的正确读法
// 关键：ET 模式下必须循环读直到 EAGAIN，否则会漏读数据！

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
// LT vs ET 模式的核心区别（先读懂这段注释）
//
// LT（水平触发，默认）：
//   - 只要 fd 中有未读数据，每次 epoll_wait 都会通知
//   - 类比：水位超过阈值就报警（持续报警）
//   - 优点：编程简单，不会漏读
//   - 缺点：事件通知次数多，开销稍大
//
// ET（边沿触发）：
//   - 只在 fd 状态变化时通知一次（从无数据 -> 有数据）
//   - 类比：水位变化时触发一次报警（不再重复）
//   - 优点：减少 epoll_wait 调用次数，高并发下性能更好
//   - 缺点：必须一次性读完所有数据，否则丢失通知！
//           因此 ET 必须配合非阻塞 IO 使用
//
// ET 正确读法：
//   while (true) {
//       ssize_t n = recv(fd, buf, BUF_SIZE, 0);
//       if (n < 0) {
//           if (errno == EAGAIN || errno == EWOULDBLOCK) break; // 数据读完
//           // 真正的错误
//           break;
//       } else if (n == 0) {
//           // 对端关闭
//           break;
//       }
//       // 处理 buf 中的 n 字节数据
//   }
// ============================================================

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool epoll_add_et(int epfd, int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events | EPOLLET; // 加上 EPOLLET 标志
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool epoll_del(int epfd, int fd) {
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

int create_listen_socket(int port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenfd, 128);
    set_nonblocking(listenfd);
    return listenfd;
}

// ============================================================
// ET 模式下处理新连接：accept 也需要循环（LT 只需一次）
// 原因：ET 下一次事件通知可能对应多个 accept 等待
// ============================================================
void handle_accept_et(int epfd, int listenfd) {
    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &addrlen);

        if (connfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // 所有连接已 accept 完
            }
            perror("accept");
            break;
        }

        set_nonblocking(connfd);
        epoll_add_et(epfd, connfd, EPOLLIN); // ET 模式加入
        std::cout << "[ET-连接] 新客户端 fd=" << connfd << std::endl;
    }
}

// ============================================================
// ET 模式下读数据：必须循环读到 EAGAIN
// 返回值：true=正常，false=连接关闭或错误
// ============================================================
bool handle_read_et(int fd) {
    const int BUF_SIZE = 4096;
    char buf[BUF_SIZE];
    std::string received;

    while (true) {
        ssize_t n = recv(fd, buf, BUF_SIZE, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 数据读完，正常退出
                break;
            }
            if (errno == EINTR) {
                continue; // 被信号中断，继续读
            }
            perror("recv");
            return false; // 真正的错误
        } else if (n == 0) {
            return false; // 对端关闭
        }
        received.append(buf, n);
    }

    if (!received.empty()) {
        std::cout << "[ET-消息] fd=" << fd << " 收到 " << received.size()
                  << " 字节: " << received.substr(0, 50) << std::endl;
        // Echo 回去（同样应该循环写，这里简化）
        send(fd, received.c_str(), received.size(), 0);
    }
    return true;
}

// ============================================================
// ET 模式的 echo 服务器（主循环）
// ============================================================

int main() {
    const int PORT = 8889;
    int listenfd = create_listen_socket(PORT);
    std::cout << "[ET服务器] 监听端口 " << PORT << std::endl;

    int epfd = epoll_create1(0);
    epoll_add_et(epfd, listenfd, EPOLLIN);

    const int MAX_EVENTS = 1024;
    struct epoll_event events[MAX_EVENTS];

    while (true) {
        int nready = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nready < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < nready; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == listenfd) {
                handle_accept_et(epfd, listenfd); // ET: 循环 accept

            } else if (ev & EPOLLIN) {
                if (!handle_read_et(fd)) {        // ET: 循环 recv
                    std::cout << "[ET-断开] fd=" << fd << std::endl;
                    epoll_del(epfd, fd);
                    close(fd);
                }

            } else if (ev & (EPOLLHUP | EPOLLERR)) {
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
// ET Bug 复现实验（理解为什么 ET 需要循环读）：
//
// 假设客户端发送 "hello\n"（6字节），但 recv 的 buf 只有 3 字节：
//
// LT 模式下：
//   第1次 epoll_wait -> EPOLLIN -> recv 3字节 ("hel")
//   第2次 epoll_wait -> EPOLLIN -> recv 3字节 ("lo\n")  ← 继续通知
//
// ET 模式下：
//   第1次 epoll_wait -> EPOLLIN -> recv 3字节 ("hel")   ← 只通知一次！
//   第2次 epoll_wait -> 没有事件！ ← "lo\n" 永远读不到（BUG）
//
// 正确做法：ET 下 recv 循环读到 EAGAIN，一次性读完所有数据
//
// 自测问题
// Q1: ET 模式下，如果 accept 不循环，在突发大量连接时会怎样？
ET 模式下必须循环 accept，直到返回 EAGAIN，否则未处理的连接不会再次触发通知，导致连接“丢失”。
// Q2: recv 返回 EINTR 是什么情况？为什么需要继续读？
EINTR 表示系统调用被信号中断，并非真正错误，需要重新调用 recv 继续完成操作。
// Q3: 写（send）数据时，ET 模式下也需要循环吗？什么时候需要？
在 ET 模式下，send 可能只发送部分数据，必须循环发送直到 EAGAIN，否则剩余数据不会再次触发写事件。
// Q4: EPOLLET | EPOLLONESHOT 是什么？用于什么场景？
EPOLLONESHOT 用于保证一个 fd 在同一时刻只被一个线程处理，常用于多线程 Reactor 模型中，处理完后需要通过 epoll_ctl 重新激活。
// ============================================================
