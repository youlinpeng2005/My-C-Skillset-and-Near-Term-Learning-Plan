// 第二周 Day11：TCP 粘包/拆包 + 自定义协议设计
// 学习目标：理解粘包原因，设计"长度前缀"协议，实现序列化/反序列化
// 这是聊天服务器的基础协议层，第四周项目会直接用

#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>
#include <arpa/inet.h> // htonl / ntohl（字节序转换）

// ============================================================
// Part 1: 粘包/拆包的原因
// ============================================================
//
// TCP 是字节流协议，不保留消息边界。例如：
//
// 客户端连续发送两条消息：
//   send("hello", 5);
//   send("world", 5);
//
// 服务器 recv 可能收到：
//   情况1（正常）:  "hello" + "world"（两次 recv）
//   情况2（粘包）:  "helloworld"（一次 recv 收到两条）
//   情况3（拆包）:  "hel" + "loworld"（消息被拆开）
//
// 解决方案：
//   方案1: 固定长度消息（浪费空间，不灵活）
//   方案2: 分隔符（如 \n）（分隔符本身出现在内容中需要转义）
//   方案3: 长度前缀（推荐）：消息头包含消息体长度
// ============================================================

// ============================================================
// Part 2: 自定义协议（长度前缀 + 消息类型）
//
// 消息格式：
// +--------+--------+---------+
// | length | type   | payload |
// | 4 bytes| 2 bytes| N bytes |
// +--------+--------+---------+
//
// length：payload 的字节数（网络字节序）
// type：消息类型（LOGIN/CHAT/HEARTBEAT 等）
// payload：消息内容（可以是 JSON 或二进制）
// ============================================================

enum class MsgType : uint16_t {
    HEARTBEAT = 0,
    LOGIN     = 1,
    LOGOUT    = 2,
    CHAT      = 3,
    ERROR     = 4,
};

// 消息头（固定 6 字节）
#pragma pack(push, 1) // 禁止内存对齐，保证结构体大小精确
struct MsgHeader {
    uint32_t length; // payload 长度（网络字节序）
    uint16_t type;   // 消息类型（网络字节序）
};
#pragma pack(pop)

static_assert(sizeof(MsgHeader) == 6, "MsgHeader should be 6 bytes");

// 完整消息
struct Message {
    MsgType type;
    std::string payload;

    // 序列化：Message -> 字节流（发送时调用）
    std::vector<char> serialize() const {
        MsgHeader header;
        header.length = htonl(static_cast<uint32_t>(payload.size()));
        header.type   = htons(static_cast<uint16_t>(type));

        std::vector<char> buf(sizeof(MsgHeader) + payload.size());
        memcpy(buf.data(), &header, sizeof(MsgHeader));
        memcpy(buf.data() + sizeof(MsgHeader), payload.data(), payload.size());
        return buf;
    }

    // 反序列化：字节流 -> Message（接收时调用）
    static bool deserialize(const char* data, size_t len, Message& msg) {
        if (len < sizeof(MsgHeader)) return false;

        MsgHeader header;
        memcpy(&header, data, sizeof(MsgHeader));

        uint32_t payload_len = ntohl(header.length);
        if (len < sizeof(MsgHeader) + payload_len) return false;

        msg.type = static_cast<MsgType>(ntohs(header.type));
        msg.payload.assign(data + sizeof(MsgHeader), payload_len);
        return true;
    }
};

void demo_protocol() {
    std::cout << "=== 自定义协议序列化/反序列化 ===" << std::endl;

    // 构造聊天消息
    Message chat_msg;
    chat_msg.type = MsgType::CHAT;
    chat_msg.payload = R"({"from":"alice","to":"all","content":"hello!"})";

    // 序列化（准备发送）
    auto buf = chat_msg.serialize();
    std::cout << "序列化后字节数: " << buf.size() << std::endl;
    std::cout << "前6字节（消息头）: ";
    for (int i = 0; i < 6; ++i) printf("%02X ", (unsigned char)buf[i]);
    std::cout << std::endl;

    // 反序列化（模拟接收）
    Message recv_msg;
    if (Message::deserialize(buf.data(), buf.size(), recv_msg)) {
        std::cout << "解析成功！" << std::endl;
        std::cout << "类型: " << static_cast<int>(recv_msg.type) << std::endl;
        std::cout << "内容: " << recv_msg.payload << std::endl;
    }
}

// ============================================================
// Part 3: 粘包处理器（接收缓冲区 + 逐步解析）
// 这是服务器接收数据的核心逻辑
// ============================================================

class PacketParser {
public:
    // 追加新收到的数据
    void append(const char* data, size_t len) {
        buffer_.insert(buffer_.end(), data, data + len);
    }

    // 尝试从缓冲区提取一条完整消息
    // 返回 true 表示成功提取，msg 被填充
    bool try_parse(Message& msg) {
        // 1. 检查是否有完整的消息头
        if (buffer_.size() < sizeof(MsgHeader)) return false;

        MsgHeader header;
        memcpy(&header, buffer_.data(), sizeof(MsgHeader));
        uint32_t payload_len = ntohl(header.length);

        // 2. 检查 payload 是否完整到达
        size_t total_len = sizeof(MsgHeader) + payload_len;
        if (buffer_.size() < total_len) return false;

        // 3. 提取完整消息
        bool ok = Message::deserialize(buffer_.data(), total_len, msg);

        // 4. 从缓冲区移除已解析的数据
        buffer_.erase(buffer_.begin(), buffer_.begin() + total_len);

        return ok;
    }

    size_t buffered_size() const { return buffer_.size(); }

private:
    std::vector<char> buffer_;
};

void demo_packet_parser() {
    std::cout << "\n=== 粘包处理器测试 ===" << std::endl;

    // 构造两条消息
    Message msg1{MsgType::CHAT, "第一条消息"};
    Message msg2{MsgType::HEARTBEAT, ""};

    auto buf1 = msg1.serialize();
    auto buf2 = msg2.serialize();

    // 模拟粘包：两条消息合并到一次 recv
    std::vector<char> combined;
    combined.insert(combined.end(), buf1.begin(), buf1.end());
    combined.insert(combined.end(), buf2.begin(), buf2.end());

    PacketParser parser;

    // 模拟分两次接收（拆包）
    size_t half = combined.size() / 2;
    parser.append(combined.data(), half);
    std::cout << "第1次 recv " << half << " 字节，缓冲区: " << parser.buffered_size() << std::endl;

    Message parsed;
    if (parser.try_parse(parsed)) {
        std::cout << "解析到消息1: " << parsed.payload << std::endl;
    } else {
        std::cout << "消息1 不完整，等待更多数据" << std::endl;
    }

    parser.append(combined.data() + half, combined.size() - half);
    std::cout << "第2次 recv " << (combined.size() - half) << " 字节" << std::endl;

    while (parser.try_parse(parsed)) {
        std::cout << "解析到消息: type=" << static_cast<int>(parsed.type)
                  << " payload='" << parsed.payload << "'" << std::endl;
    }
}

// ============================================================
// 自测问题（Day11 结束前回答）
// Q1: 为什么需要字节序转换（htonl/ntohl）？什么是大端/小端？
不同 CPU 保存多字节数据时，字节排列顺序可能不同，大端是高位字节放低地址，小端是低位字节放低地址，而网络协议统一使用大端作为网络字节序。由于很多主机（如 x86）是小端，所以网络通信时需要通过 htonl/htons 在发送前转换成网络字节序，再通过 ntohl/ntohs 在接收后转换回主机字节序，否则不同机器之间会出现整数解析错误的问题。
// Q2: #pragma pack(1) 的作用是什么？不加会有什么问题？
#pragma pack(1) 用于关闭结构体内存对齐，让结构体成员按 1 字节紧凑排列，避免编译器自动插入 padding 字节；如果不加，编译器为了提高 CPU 访问效率通常会按 4 字节或 8 字节对齐，不同平台和编译器的补齐规则还可能不同，这会导致网络协议或文件格式中的结构体大小和字节布局不一致，从而出现协议解析错误的问题。
// Q3: 如果 payload 中包含二进制数据，用 JSON 格式还是二进制格式更合适？为什么？
如果 payload 中包含图片、音频、视频等二进制内容，通常使用二进制格式更合适，因为 JSON 本质是文本协议，保存二进制时往往需要 Base64 编码，会导致数据体积增大、解析效率降低，而二进制协议可以直接传输原始数据，性能和带宽利用率更高；不过 JSON 的优点是可读性强、调试方便，因此实际开发中常见做法是控制信息使用 JSON，而大块数据使用二进制流传输。
// Q4: 如何给协议加版本号，方便后续扩展？
通常会在协议头中增加一个 version 字段，通信双方在解析数据前先读取版本号，再根据不同版本使用对应的解析逻辑，这样即使后续协议新增字段、修改结构，旧客户端依然可以按旧版本处理数据，从而实现向后兼容和协议平滑升级，因此版本号本质上是协议扩展与兼容性设计的重要机制。
// ============================================================

int main() {
    demo_protocol();
    demo_packet_parser();

    std::cout << "\n=== Day11 完成！明天任务：Reactor 模式实现 ===" << std::endl;
    return 0;
}
