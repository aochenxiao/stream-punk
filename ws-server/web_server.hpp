#pragma once

#include "..\stream-punk\StreamPunk.hpp"
#include "..\stream-punk\Data.hpp"
#include "..\stream-punk\customData.hpp"
#include "..\code-generator\TsMemberInfo.hpp"

// 不建议往里面塞处在栈中的对象,在经过释放-再分配,容易产生不同对象但地址重复的问题.
struct SpObjProtocolOutput {
    O o;
    SpObjProtocolOutput(std::ostream& o_) : o(o_) {}
    template<typename T> SpObjProtocolOutput& operator<<(T const& v) {
        if constexpr (std::is_pointer_v<T> || is_specialization_of_any_v<T, std::shared_ptr, std::unique_ptr>) {
            o << v;
        }
        else if constexpr (is_specialization_of_any_v<T, std::weak_ptr>) {
            auto var = v.lock();
            operator<<(var);
        }
        else {
            o << &v;
        }
        return *this;
    }
};

std::string test1();


#include <drogon/WebSocketController.h>
#include <drogon/HttpAppFramework.h>
#include <mutex>
#include <unordered_map>

using namespace drogon;

class ConnectionManager {
public:
    void addConnection(const WebSocketConnectionPtr& conn, const std::string& room) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_[conn] = room;
        rooms_[room].insert(conn);
    }

    void removeConnection(const WebSocketConnectionPtr& conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(conn);
        if (it != connections_.end()) {
            std::string room = it->second;
            connections_.erase(it);
            rooms_[room].erase(conn);
        }
    }

    void broadcastToRoom(const std::string& room, const std::string& message, const WebSocketConnectionPtr& exclude = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto roomIt = rooms_.find(room);
        if (roomIt != rooms_.end()) {
            for (const auto& conn : roomIt->second) {
                if (conn != exclude) {
                    conn->send(message.data(),message.size(),WebSocketMessageType::Binary);
                }
            }
        }
    }

    void broadcastToAll(const std::string& message, const WebSocketConnectionPtr& exclude = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [conn, room] : connections_) {
            if (conn != exclude) {
                conn->send(message.data(), message.size(), WebSocketMessageType::Binary);
            }
        }
    }

private:
    std::mutex mutex_;
    std::unordered_map<WebSocketConnectionPtr, std::string> connections_;
    std::unordered_map<std::string, std::unordered_set<WebSocketConnectionPtr>> rooms_;
};

class WebSocketChat : public drogon::WebSocketController<WebSocketChat> {
public:

    void handleNewMessage(
        const WebSocketConnectionPtr& wsConnPtr,
        std::string&& message,
        const WebSocketMessageType& type
    ) override {
        if (message.size() == 1 && message.data()[0] == E_type::e_unknowType) {
            auto msg = test1();
            if (msg.size() > 0) {
                wsConnPtr->send(msg.data(), msg.size(), WebSocketMessageType::Binary);
            }
        }
        //if (type == WebSocketMessageType::Text) {
        //    connectionManager_.broadcastToAll(message, wsConnPtr);
        //}
    }

    void handleConnectionClosed(const WebSocketConnectionPtr& conn) override {
        connectionManager_.removeConnection(conn);
    }

    void handleNewConnection(
        const HttpRequestPtr& req,
        const WebSocketConnectionPtr& conn
    ) override {
        std::string room = req->getParameter("room_name");
        if (room.empty()) {
            room = "default";
        }
        connectionManager_.addConnection(conn, room);
        //conn->send(test1());
    }

    void deliverToRoom(const std::string& room, const std::string& message) {
        connectionManager_.broadcastToRoom(room, message);
    }

    void deliverToAll(const std::string& message) {
        connectionManager_.broadcastToAll(message);
    }

    static void initPathRouting() {
        registerSelf__("/chat", { Get });
        registerSelfRegex__("/[^/]*", { Get });
    }

    ConnectionManager connectionManager_;
};

