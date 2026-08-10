#pragma once
#include "customData.hpp"
#include <stream-punk/StreamPunk.hpp>

namespace sp {

// ==================== 文本操作类型 ====================
// opType: 0=insert, 1=delete
struct TextOp : public Base {
    #define Xt_TextOp(X__) \
    X__(i32, opType, 0) \
    X__(i32, position, 0) \
    X__(std::string, text, "") \
    X__(i32, userId, 0) \
    X__(i32, version, 0)
    TextOp() = default;
    UseData(TextOp);
};

// ==================== 光标信息 ====================
struct CursorInfo : public Base {
    #define Xt_CursorInfo(X__) \
    X__(i32, userId, 0) \
    X__(std::string, userName, "") \
    X__(i32, position, 0) \
    X__(std::string, color, "")
    CursorInfo() = default;
    UseData(CursorInfo);
};

// ==================== 加入请求 ====================
struct JoinRequest : public Base {
    #define Xt_JoinRequest(X__) \
    X__(std::string, userName, "")
    JoinRequest() = default;
    UseData(JoinRequest);
};

// ==================== 加入响应 ====================
struct JoinResponse : public Base {
    #define Xt_JoinResponse(X__) \
    X__(i32, userId, 0) \
    X__(std::string, document, "") \
    X__(std::vector<CursorInfo>, users, std::vector<CursorInfo>{})
    JoinResponse() = default;
    UseData(JoinResponse);
};

// ==================== 用户列表更新 ====================
struct UserListUpdate : public Base {
    #define Xt_UserListUpdate(X__) \
    X__(std::vector<CursorInfo>, users, std::vector<CursorInfo>{})
    UserListUpdate() = default;
    UseData(UserListUpdate);
};

} // namespace sp