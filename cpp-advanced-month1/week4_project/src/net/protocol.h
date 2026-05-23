#pragma once
// 自定义协议（复用 Day11 代码，整合到项目中）

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>

enum class MsgType : uint16_t {
    HEARTBEAT = 0,
    LOGIN     = 1,
    LOGOUT    = 2,
    CHAT      = 3,
    ERROR     = 4,
};

#pragma pack(push, 1)
struct MsgHeader {
    uint32_t length; // payload 长度（网络字节序）
    uint16_t type;   // 消息类型（网络字节序）
};
#pragma pack(pop)

static_assert(sizeof(MsgHeader) == 6, "MsgHeader must be 6 bytes");

struct Message {
    MsgType type;
    std::string payload;

    std::vector<char> serialize() const {
        MsgHeader hdr;
        hdr.length = htonl(static_cast<uint32_t>(payload.size()));
        hdr.type   = htons(static_cast<uint16_t>(type));
        std::vector<char> buf(sizeof(MsgHeader) + payload.size());
        memcpy(buf.data(), &hdr, sizeof(MsgHeader));
        memcpy(buf.data() + sizeof(MsgHeader), payload.data(), payload.size());
        return buf;
    }

    static bool deserialize(const char* data, size_t len, Message& msg) {
        if (len < sizeof(MsgHeader)) return false;
        MsgHeader hdr;
        memcpy(&hdr, data, sizeof(MsgHeader));
        uint32_t plen = ntohl(hdr.length);
        if (len < sizeof(MsgHeader) + plen) return false;
        msg.type = static_cast<MsgType>(ntohs(hdr.type));
        msg.payload.assign(data + sizeof(MsgHeader), plen);
        return true;
    }
};

// 粘包处理器
class PacketParser {
public:
    void append(const char* data, size_t len) {
        buffer_.insert(buffer_.end(), data, data + len);
    }

    bool try_parse(Message& msg) {
        if (buffer_.size() < sizeof(MsgHeader)) return false;
        MsgHeader hdr;
        memcpy(&hdr, buffer_.data(), sizeof(MsgHeader));
        uint32_t plen = ntohl(hdr.length);
        size_t total = sizeof(MsgHeader) + plen;
        if (buffer_.size() < total) return false;
        bool ok = Message::deserialize(buffer_.data(), total, msg);
        buffer_.erase(buffer_.begin(), buffer_.begin() + total);
        return ok;
    }

private:
    std::vector<char> buffer_;
};
