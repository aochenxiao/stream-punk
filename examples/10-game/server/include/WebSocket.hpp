// 简易 WebSocket 协议实现（RFC 6455）
// 仅实现服务端需要的功能：HTTP 升级握手 + 帧封装/解封装

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>

// ===== SHA1 实现（仅用于 WebSocket 握手）=====
namespace ws_detail {

inline u32 leftRotate(u32 x, u32 n) {
    return (x << n) | (x >> (32 - n));
}

inline std::string sha1(const std::string& input) {
    // 预处理
    std::vector<u8> msg(input.begin(), input.end());
    u64 bitLen = msg.size() * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    for (int i = 7; i >= 0; i--) {
        msg.push_back(static_cast<u8>((bitLen >> (i * 8)) & 0xFF));
    }

    u32 h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        u32 w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = (msg[chunk + i * 4] << 24) | (msg[chunk + i * 4 + 1] << 16)
                 | (msg[chunk + i * 4 + 2] << 8) | msg[chunk + i * 4 + 3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = leftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        u32 a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            u32 f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            u32 temp = leftRotate(a, 5) + f + e + k + w[i];
            e = d; d = c; c = leftRotate(b, 30); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    auto toHex = [](u32 v) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (int i = 3; i >= 0; i--) ss << std::setw(2) << ((v >> (i * 8)) & 0xFF);
        return ss.str();
    };
    return toHex(h0) + toHex(h1) + toHex(h2) + toHex(h3) + toHex(h4);
}

inline std::string base64Encode(const std::string& input) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (size_t i = 0; i < input.size(); i += 3) {
        u32 val = (static_cast<u8>(input[i]) << 16);
        if (i + 1 < input.size()) val |= (static_cast<u8>(input[i + 1]) << 8);
        if (i + 2 < input.size()) val |= static_cast<u8>(input[i + 2]);
        result += chars[(val >> 18) & 0x3F];
        result += chars[(val >> 12) & 0x3F];
        result += (i + 1 < input.size()) ? chars[(val >> 6) & 0x3F] : '=';
        result += (i + 2 < input.size()) ? chars[val & 0x3F] : '=';
    }
    return result;
}

} // namespace ws_detail

// ===== WebSocket 操作 =====

struct WebSocket {
    SOCKET sock;
    bool handshakeDone = false;

    // 尝试握手（从 socket 读取 HTTP 升级请求，发送 101 响应）
    // 返回 true 表示握手成功
    bool tryHandshake() {
        // 读取 HTTP 请求
        char buf[4096] = {};
        int received = recv(sock, buf, sizeof(buf) - 1, 0);
        if (received <= 0) return false;
        std::string request(buf, received);

        // 提取 Sec-WebSocket-Key
        auto keyPos = request.find("Sec-WebSocket-Key:");
        if (keyPos == std::string::npos) return false;
        keyPos += 19; // 跳过 "Sec-WebSocket-Key:"
        while (keyPos < request.size() && request[keyPos] == ' ') keyPos++;
        auto keyEnd = request.find("\r\n", keyPos);
        if (keyEnd == std::string::npos) return false;
        std::string key = request.substr(keyPos, keyEnd - keyPos);

        // 计算 Accept Key
        std::string magic = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string sha1Hex = ws_detail::sha1(magic);
        // 将 hex 字符串转为原始字节
        std::string sha1Bytes;
        for (size_t i = 0; i < sha1Hex.size(); i += 2) {
            sha1Bytes += static_cast<char>(std::stoi(sha1Hex.substr(i, 2), nullptr, 16));
        }
        std::string acceptKey = ws_detail::base64Encode(sha1Bytes);

        // 发送 101 响应
        std::string response =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";

        ::send(sock, response.c_str(), static_cast<int>(response.size()), 0);
        handshakeDone = true;
        return true;
    }

    // 发送文本帧
    bool sendText(const std::string& text) {
        std::vector<u8> frame;
        // FIN + opcode(0x1=text)
        frame.push_back(0x81);
        // MASK=0, payload length
        if (text.size() <= 125) {
            frame.push_back(static_cast<u8>(text.size()));
        } else if (text.size() <= 65535) {
            frame.push_back(126);
            frame.push_back(static_cast<u8>((text.size() >> 8) & 0xFF));
            frame.push_back(static_cast<u8>(text.size() & 0xFF));
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; i--) {
                frame.push_back(static_cast<u8>((text.size() >> (i * 8)) & 0xFF));
            }
        }
        frame.insert(frame.end(), text.begin(), text.end());
        return ::send(sock, reinterpret_cast<const char*>(frame.data()),
                    static_cast<int>(frame.size()), 0) == static_cast<int>(frame.size());
    }

    // 发送二进制帧
    bool sendBinary(const std::vector<u8>& data) {
        std::vector<u8> frame;
        frame.push_back(0x82); // FIN + opcode(0x2=binary)
        size_t len = data.size();
        if (len <= 125) {
            frame.push_back(static_cast<u8>(len));
        } else if (len <= 65535) {
            frame.push_back(126);
            frame.push_back(static_cast<u8>((len >> 8) & 0xFF));
            frame.push_back(static_cast<u8>(len & 0xFF));
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; i--) {
                frame.push_back(static_cast<u8>((len >> (i * 8)) & 0xFF));
            }
        }
        frame.insert(frame.end(), data.begin(), data.end());
        return ::send(sock, reinterpret_cast<const char*>(frame.data()),
                    static_cast<int>(frame.size()), 0) == static_cast<int>(frame.size());
    }

    // 接收帧（返回 payload，空 vector 表示错误或断开）
    std::vector<u8> recvFrame() {
        // 读取前2字节
        u8 header[2];
        int received = recv(sock, reinterpret_cast<char*>(header), 2, MSG_PEEK);
        if (received <= 0) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) return {}; // 无数据
            return {u8(0)}; // 错误
        }
        if (received < 2) return {};

        // 读取完整帧头
        std::vector<u8> frameHeader;
        size_t headerSize = 2;
        u8 maskFlag = header[1] & 0x80;
        u64 payloadLen = header[1] & 0x7F;

        if (payloadLen == 126) headerSize += 2;
        else if (payloadLen == 127) headerSize += 8;
        if (maskFlag) headerSize += 4;

        // 读取完整头部
        frameHeader.resize(headerSize);
        received = recv(sock, reinterpret_cast<char*>(frameHeader.data()),
                        static_cast<int>(headerSize), 0);
        if (received <= 0) return {};
        if (static_cast<size_t>(received) < headerSize) return {};

        u8 opcode = frameHeader[0] & 0x0F;
        if (opcode == 0x8) return {u8(0)}; // 关闭帧
        if (opcode == 0x9) { // Ping → Pong
            std::vector<u8> pong = {0x8A, 0x00};
            ::send(sock, reinterpret_cast<const char*>(pong.data()), 2, 0);
            return {}; // 忽略，等待下一帧
        }

        // 解析 payload 长度
        size_t offset = 2;
        if (payloadLen == 126) {
            payloadLen = (frameHeader[2] << 8) | frameHeader[3];
            offset = 4;
        } else if (payloadLen == 127) {
            payloadLen = 0;
            for (int i = 0; i < 8; i++) {
                payloadLen = (payloadLen << 8) | frameHeader[2 + i];
            }
            offset = 10;
        }

        // 读取 payload
        std::vector<u8> payload(static_cast<size_t>(payloadLen));
        if (payloadLen > 0) {
            received = recv(sock, reinterpret_cast<char*>(payload.data()),
                            static_cast<int>(payloadLen), 0);
            if (received <= 0) return {};
            if (static_cast<u64>(received) < payloadLen) return {};
        }

        // 解掩码
        if (maskFlag) {
            u8 mask[4] = {frameHeader[offset], frameHeader[offset + 1],
                          frameHeader[offset + 2], frameHeader[offset + 3]};
            for (size_t i = 0; i < payload.size(); i++) {
                payload[i] ^= mask[i % 4];
            }
        }

        return payload;
    }
};