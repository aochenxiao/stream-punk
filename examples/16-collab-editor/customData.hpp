#pragma once

namespace sp {
struct TextOp;
struct CursorInfo;
struct JoinRequest;
struct JoinResponse;
struct UserListUpdate;
}

#define Xt_CustomType(X__) \
X__(sp::TextOp, TextOp) \
X__(sp::CursorInfo, CursorInfo) \
X__(sp::JoinRequest, JoinRequest) \
X__(sp::JoinResponse, JoinResponse) \
X__(sp::UserListUpdate, UserListUpdate)