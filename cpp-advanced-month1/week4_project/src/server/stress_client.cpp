// 压测客户端（5月15日使用）
// 功能：多线程并发建立连接，持续发送消息，观察服务器性能
// 编译：g++ -std=c++17 stress_client.cpp -o stress_client -lpthread
// 用法：./stress_client [并发连接数] [每连接发送次数]

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstdint>

static const char* SERVER_IP = "127.0.0.1";
static const int   SERVER_PORT = 8893;

// 发送消息（使用自定义协议）
bool send_message(int fd, uint16_t type, const std::string& payload) {
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    uint16_t t   = htons(type);

    // 消息头
    char header[6];
    memcpy(header, &len, 4);
    memcpy(header + 4, &t, 2);

    if (send(fd, header, 6, 0) != 6) return false;
    if (!payload.empty()) {
        if (send(fd, payload.c_str(), payload.size(), 0) != (ssize_t)payload.size())
            return false;
    }
    return true;
}

int connect_to_server() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

std::atomic<int> success_count(0);
std::atomic<int> fail_count(0);
std::atomic<long long> total_messages(0);

void client_task(int client_id, int num_messages) {
    int fd = connect_to_server();
    if (fd < 0) {
        ++fail_count;
        return;
    }
    ++success_count;

    for (int i = 0; i < num_messages; ++i) {
        std::string msg = "client" + std::to_string(client_id)
                        + " msg" + std::to_string(i);
        if (!send_message(fd, 3 /*CHAT*/, msg)) break;
        ++total_messages;
        // 发送间隔（避免网络缓冲区溢出）
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(fd);
}

int main(int argc, char* argv[]) {
    int num_clients  = (argc > 1) ? atoi(argv[1]) : 10;
    int num_messages = (argc > 2) ? atoi(argv[2]) : 20;

    std::cout << "压测开始：" << num_clients << " 个并发连接，每连接 "
              << num_messages << " 条消息" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_clients; ++i) {
        threads.emplace_back(client_task, i, num_messages);
    }
    for (auto& t : threads) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "压测结束：" << std::endl;
    std::cout << "  成功连接: " << success_count << "/" << num_clients << std::endl;
    std::cout << "  失败连接: " << fail_count << std::endl;
    std::cout << "  总消息数: " << total_messages << std::endl;
    std::cout << "  总耗时:   " << ms << "ms" << std::endl;
    std::cout << "  QPS:      " << (total_messages * 1000 / (ms + 1)) << " msg/s" << std::endl;

    return 0;
}
