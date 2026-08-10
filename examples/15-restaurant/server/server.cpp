// 餐厅管理系统 WebSocket 服务端
// 使用 StreamPunk 进行二进制序列化

#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>
#include <map>
#include <algorithm>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ws2_32.lib")

#include "../Data.hpp"
#include <stream-punk/StreamPunkJson.hpp>
using namespace sp;

// ==================== 日志 ====================
inline std::string nowStr() {
    auto t = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count() % 1000;
    auto tt = std::chrono::system_clock::to_time_t(t);
    std::tm tm;
    localtime_s(&tm, &tt);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03lld", tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
    return buf;
}
#define LOG(tag, ...) printf("[%s] %s ", nowStr().c_str(), tag); printf(__VA_ARGS__); printf("\n"); fflush(stdout)

// ==================== WebSocket 帧协议 ====================
enum class WsOpcode : u8 { Text = 0x1, Binary = 0x2, Close = 0x8, Ping = 0x9, Pong = 0xA };

enum class MsgType : u8 {
    PlaceOrder = 0x01, UpdateStatus = 0x02, Payment = 0x03,
    RoleLogin = 0x04, ChangeTable = 0x05, MergeOrders = 0x06,
    SplitOrder = 0x07, UrgeDish = 0x08,
    ServerState = 0x10, Notification = 0x11, RoleAssigned = 0x12,
};

std::vector<u8> makeWsFrame(WsOpcode opcode, const std::vector<u8>& payload) {
    std::vector<u8> frame;
    frame.push_back(0x80 | static_cast<u8>(opcode));
    size_t len = payload.size();
    if (len <= 125) { frame.push_back(static_cast<u8>(len)); }
    else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<u8>((len >> 8) & 0xFF));
        frame.push_back(static_cast<u8>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) frame.push_back(static_cast<u8>((len >> (i * 8)) & 0xFF));
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

bool parseWsFrame(const std::vector<u8>& buf, size_t& consumed, WsOpcode& opcode, std::vector<u8>& payload) {
    if (buf.size() < 2) return false;
    size_t pos = 0;
    opcode = static_cast<WsOpcode>(buf[pos++] & 0x0F);
    bool masked = (buf[pos] & 0x80) != 0;
    size_t len = buf[pos++] & 0x7F;
    if (len == 126) { if (buf.size() < pos + 2) return false; len = (static_cast<size_t>(buf[pos]) << 8) | buf[pos + 1]; pos += 2; }
    else if (len == 127) { if (buf.size() < pos + 8) return false; len = 0; for (int i = 0; i < 8; ++i) len = (len << 8) | buf[pos + i]; pos += 8; }
    u8 mask[4] = {};
    if (masked) { if (buf.size() < pos + 4) return false; for (int i = 0; i < 4; ++i) mask[i] = buf[pos + i]; pos += 4; }
    if (buf.size() < pos + len) return false;
    payload.resize(len);
    for (size_t i = 0; i < len; ++i) payload[i] = buf[pos + i] ^ mask[i % 4];
    consumed = pos + len;
    return true;
}

// ==================== HTTP 握手 ====================
std::string readHttpRequest(SOCKET sock) {
    std::string req; char buf[4096]; int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) { buf[n] = '\0'; req = buf; } return req;
}
std::string extractHeader(const std::string& req, const std::string& key) {
    size_t pos = req.find(key + ": "); if (pos == std::string::npos) return "";
    pos += key.length() + 2; size_t end = req.find("\r\n", pos); return req.substr(pos, end - pos);
}
static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string base64Encode(const std::vector<u8>& data) {
    std::string r; size_t i = 0;
    while (i < data.size()) {
        u32 t = 0; int p = 0;
        for (int j = 0; j < 3; ++j) { t <<= 8; if (i < data.size()) t |= data[i++]; else ++p; }
        r += B64[(t >> 18) & 0x3F]; r += B64[(t >> 12) & 0x3F];
        r += (p >= 2) ? '=' : B64[(t >> 6) & 0x3F];
        r += (p >= 1) ? '=' : B64[t & 0x3F];
    } return r;
}
std::string sha1(const std::string& input) {
    HCRYPTPROV hP = 0; HCRYPTHASH hH = 0;
    if (!CryptAcquireContextW(&hP, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) return {};
    if (!CryptCreateHash(hP, CALG_SHA1, 0, 0, &hH)) { CryptReleaseContext(hP, 0); return {}; }
    CryptHashData(hH, reinterpret_cast<const BYTE*>(input.data()), static_cast<DWORD>(input.size()), 0);
    DWORD hL = 20; BYTE h[20] = {}; CryptGetHashParam(hH, HP_HASHVAL, h, &hL, 0);
    CryptDestroyHash(hH); CryptReleaseContext(hP, 0);
    return std::string(reinterpret_cast<char*>(h), hL);
}
bool doHandshake(SOCKET sock, const std::string& req) {
    std::string key = extractHeader(req, "Sec-WebSocket-Key"); if (key.empty()) return false;
    auto hash = sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    std::string r = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + base64Encode(std::vector<u8>(hash.begin(), hash.end())) + "\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    return send(sock, r.c_str(), static_cast<int>(r.size()), 0) > 0;
}

// ==================== 业务逻辑 ====================
struct ClientInfo {
    SOCKET sock; int id; int role = -1; std::string name;
    std::vector<u8> recvBuf; bool connected = true;
    ClientInfo(SOCKET s, int cid) : sock(s), id(cid) {}
};

struct RestaurantEngine {
    ServerState state;
    std::vector<ClientInfo> clients;
    std::mutex mtx;
    int nextClientId = 1;
    int nextOrderId = 1;

    void initData() {
        // 餐桌
        std::vector<std::string> tns = {"A01","A02","A03","A04","A05","B01包","B02包","B03包","C01","C02"};
        for (size_t i = 0; i < tns.size(); ++i) {
            Table t; t.id = static_cast<i32>(i + 1); t.name = tns[i];
            t.type = (tns[i].find("包") != std::string::npos) ? TableType::PrivateRoom : TableType::Table;
            t.status = TableStatus::Available; t.capacity = (t.type == TableType::PrivateRoom) ? 8 : 4;
            state.tables.push_back(t);
        }
        // 原材料
        struct II { int id; std::string n, u; double s, ms; };
        std::vector<II> ings = {
            {1,"猪肉","斤",20,5},{2,"青椒","斤",15,3},{3,"牛肉","斤",18,4},{4,"土豆","斤",25,5},
            {5,"鸡蛋","个",60,12},{6,"番茄","斤",12,3},{7,"鱼","条",10,2},{8,"豆腐","块",20,5},
            {9,"白菜","斤",15,3},{10,"辣椒","斤",8,2},{11,"面粉","斤",30,5},{12,"大米","斤",50,10},
            {13,"食用油","升",10,2},{14,"盐","袋",20,3},{15,"酱油","瓶",15,3},{16,"糖","袋",10,2},
            {17,"鸡肉","斤",15,3},{18,"虾","斤",8,2},{19,"葱","斤",5,1},{20,"姜","斤",5,1},
        };
        for (auto& x : ings) {
            Ingredient it; it.id = x.id; it.name = x.n; it.unit = x.u; it.stock = x.s; it.minStock = x.ms;
            state.ingredients.push_back(it);
        }
        // 菜品
        struct MI { int id; std::string n; double p; std::string c; };
        std::vector<MI> menus = {
            {1,"青椒肉丝",28,"热菜"},{2,"红烧牛肉",58,"热菜"},{3,"番茄炒蛋",18,"热菜"},
            {4,"酸辣土豆丝",16,"热菜"},{5,"清蒸鱼",68,"海鲜"},{6,"麻婆豆腐",22,"热菜"},
            {7,"辣子鸡",48,"热菜"},{8,"油焖大虾",78,"海鲜"},{9,"蛋炒饭",12,"主食"},
            {10,"白米饭",3,"主食"},{11,"酸辣汤",15,"汤类"},{12,"家常豆腐",20,"热菜"},
        };
        for (auto& m : menus) {
            MenuItem it; it.id = m.id; it.name = m.n; it.price = m.p; it.category = m.c; it.available = true;
            state.menu.push_back(it);
        }
        // 配方 BOM
        struct BE { int mi, ii; double q; };
        std::vector<BE> boms = {
            {1,1,0.3},{1,2,0.2},{1,13,0.05},{1,14,0.01},{1,15,0.02},
            {2,3,0.5},{2,4,0.3},{2,13,0.05},{2,14,0.01},{2,15,0.03},
            {3,5,3},{3,6,0.3},{3,13,0.03},{3,14,0.01},
            {4,4,0.5},{4,10,0.1},{4,13,0.03},{4,14,0.01},
            {5,7,1},{5,19,0.05},{5,20,0.05},{5,14,0.01},
            {6,8,2},{6,10,0.1},{6,13,0.03},{6,14,0.01},
            {7,17,0.5},{7,10,0.2},{7,13,0.05},{7,14,0.01},
            {8,18,0.5},{8,19,0.05},{8,20,0.05},{8,13,0.05},
            {12,8,2},{12,2,0.1},{12,13,0.03},{12,14,0.01},
        };
        std::map<int, Recipe> rm;
        for (auto& b : boms) {
            auto& r = rm[b.mi]; r.menuItemId = b.mi;
            RecipeItem ri; ri.ingredientId = b.ii; ri.quantity = b.q;
            for (auto& ing : state.ingredients) { if (ing.id == b.ii) { ri.ingredientName = ing.name; break; } }
            r.items.push_back(ri);
        }
        for (auto& kv : rm) state.recipes.push_back(kv.second);
        LOG("INIT", "Tables:%zu Menu:%zu Ingredients:%zu Recipes:%zu",
            state.tables.size(), state.menu.size(), state.ingredients.size(), state.recipes.size());
    }

    void updateMenuAvailability() {
        for (auto& menu : state.menu) {
            Recipe* rp = nullptr;
            for (auto& r : state.recipes) { if (r.menuItemId == menu.id) { rp = &r; break; } }
            if (!rp) { menu.available = true; continue; }
            bool ok = true;
            for (auto& ri : rp->items) {
                for (auto& ing : state.ingredients) {
                    if (ing.id == ri.ingredientId && ing.stock < ri.quantity) { ok = false; break; }
                }
                if (!ok) break;
            }
            menu.available = ok;
        }
    }

    bool deductInventory(int menuId, int qty) {
        Recipe* rp = nullptr;
        for (auto& r : state.recipes) { if (r.menuItemId == menuId) { rp = &r; break; } }
        if (!rp) return true;
        for (auto& ri : rp->items) {
            for (auto& ing : state.ingredients) {
                if (ing.id == ri.ingredientId) {
                    double need = ri.quantity * qty;
                    if (ing.stock < need) return false;
                    ing.stock -= need;
                }
            }
        }
        updateMenuAvailability();
        return true;
    }

    void broadcastState() {
        std::stringstream ss; O o{ss}; o << state;
        auto str = ss.str();
        std::vector<u8> payload;
        payload.push_back(static_cast<u8>(MsgType::ServerState));
        payload.insert(payload.end(), reinterpret_cast<const u8*>(str.data()), reinterpret_cast<const u8*>(str.data()) + str.size());
        auto frame = makeWsFrame(WsOpcode::Binary, payload);
        for (auto& c : clients) { int s = send(c.sock, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()), 0); if (s <= 0) c.connected = false; }
    }

    void sendRoleAssigned(SOCKET sock, int role, const std::string& name, i32 tableId = 0) {
        RoleAssigned ra; ra.role = role; ra.name = name; ra.tableId = tableId;
        std::stringstream ss; O o{ss}; o << ra;
        auto str = ss.str();
        std::vector<u8> p; p.push_back(static_cast<u8>(MsgType::RoleAssigned));
        p.insert(p.end(), reinterpret_cast<const u8*>(str.data()), reinterpret_cast<const u8*>(str.data()) + str.size());
        auto f = makeWsFrame(WsOpcode::Binary, p);
        send(sock, reinterpret_cast<const char*>(f.data()), static_cast<int>(f.size()), 0);
    }

    void sendNotification(const std::string& msg, int type, int targetRole = -1) {
        Notification n; n.message = msg; n.type = type; n.targetRole = targetRole;
        std::stringstream ss; O o{ss}; o << n;
        auto str = ss.str();
        std::vector<u8> p; p.push_back(static_cast<u8>(MsgType::Notification));
        p.insert(p.end(), reinterpret_cast<const u8*>(str.data()), reinterpret_cast<const u8*>(str.data()) + str.size());
        auto f = makeWsFrame(WsOpcode::Binary, p);
        for (auto& c : clients) {
            if (targetRole < 0 || c.role == targetRole)
                send(c.sock, reinterpret_cast<const char*>(f.data()), static_cast<int>(f.size()), 0);
        }
    }

    void handlePlaceOrder(ClientInfo& cl, const std::vector<u8>& payload) {
        std::stringstream ss; ss.write(reinterpret_cast<const char*>(&payload[1]), payload.size() - 1);
        PlaceOrderRequest req; I i{ss}; i >> req;
        Table* tb = nullptr;
        for (auto& t : state.tables) { if (t.id == req.tableId) { tb = &t; break; } }
        if (!tb) { sendNotification("餐桌不存在", 2, cl.role); return; }
        Order o; o.id = nextOrderId++; o.tableId = req.tableId; o.tableName = tb->name;
        o.status = OrderStatus::Pending; o.discount = 1.0;
        double total = 0;
        for (auto& item : req.items) {
            for (auto& m : state.menu) { if (m.id == item.menuItemId) { item.menuItemName = m.name; total += m.price * item.quantity; break; } }
            item.status = OrderStatus::Pending;
            o.items.push_back(item);
        }
        o.totalPrice = total;
        for (auto& item : o.items) {
            if (!deductInventory(item.menuItemId, item.quantity)) { sendNotification("库存不足：" + item.menuItemName, 2, cl.role); return; }
        }
        state.orders.push_back(o);
        tb->status = TableStatus::Occupied;
        LOG("ORDER", "#%d %s %zu items %.2f", o.id, o.tableName.c_str(), o.items.size(), total);
        sendNotification("新订单#" + std::to_string(o.id) + " " + o.tableName, 0, Role::Cutter);
        sendNotification("新订单#" + std::to_string(o.id) + " " + o.tableName, 0, Role::HeadChef);
        broadcastState();
    }

    void handleUpdateStatus(ClientInfo& cl, const std::vector<u8>& payload) {
        std::stringstream ss; ss.write(reinterpret_cast<const char*>(&payload[1]), payload.size() - 1);
        UpdateStatusRequest req; I i{ss}; i >> req;
        Order* o = nullptr;
        for (auto& x : state.orders) { if (x.id == req.orderId) { o = &x; break; } }
        if (!o) { sendNotification("订单不存在", 2, cl.role); return; }
        if (req.itemIndex < 0 || req.itemIndex >= static_cast<i32>(o->items.size())) { sendNotification("索引无效", 2, cl.role); return; }
        auto& item = o->items[req.itemIndex];
        i32 oldS = item.status, newS = req.newStatus;
        bool valid = false;
        if (newS == OrderStatus::Cutting && (oldS == OrderStatus::Pending || oldS == OrderStatus::Remake)) valid = true;
        else if (newS == OrderStatus::ReadyToCook && oldS == OrderStatus::Cutting) valid = true;
        else if (newS == OrderStatus::Cooking && oldS == OrderStatus::ReadyToCook) valid = true;
        else if (newS == OrderStatus::ReadyToServe && (oldS == OrderStatus::Cooking || oldS == OrderStatus::InTransit)) valid = true;
        else if (newS == OrderStatus::InTransit && oldS == OrderStatus::ReadyToServe) valid = true;
        else if (newS == OrderStatus::Delivered && oldS == OrderStatus::InTransit) valid = true;
        else if (newS == OrderStatus::Remake && (oldS == OrderStatus::Cooking || oldS == OrderStatus::ReadyToServe)) valid = true;
        else if (newS == OrderStatus::Damaged && (oldS == OrderStatus::InTransit || oldS == OrderStatus::ReadyToServe)) valid = true;
        else if (newS == OrderStatus::Urgent && oldS != OrderStatus::Completed && oldS != OrderStatus::Cancelled && oldS != OrderStatus::Damaged) valid = true;
        if (!valid) { sendNotification("状态转换无效 " + std::to_string(oldS) + "->" + std::to_string(newS), 2, cl.role); return; }
        if (newS == OrderStatus::Urgent) {
            sendNotification("催菜!" + o->tableName + " " + item.menuItemName, 3, Role::Chef);
            sendNotification("催菜!" + o->tableName + " " + item.menuItemName, 3, Role::HeadChef);
            broadcastState(); return;
        }
        item.status = newS; if (!req.note.empty()) item.note = req.note;
        const char* sn[] = {"待处理","切配中","待炒","烹饪中","待传菜","传菜中","已上桌","已完成","重做","报废","已取消","催菜"};
        std::string msg = o->tableName + " " + item.menuItemName + " -> " + sn[newS];
        if (newS == OrderStatus::ReadyToCook) sendNotification(msg, 0, Role::Chef);
        else if (newS == OrderStatus::ReadyToServe) sendNotification(msg, 0, Role::Runner);
        else if (newS == OrderStatus::Remake) sendNotification("重做!" + msg, 2, Role::Cutter);
        updateOrderStatus(*o);
        LOG("STATUS", "#%d[%d] %s->%s", o->id, req.itemIndex, sn[oldS], sn[newS]);
        broadcastState();
    }

    void updateOrderStatus(Order& o) {
        bool allDone = true, allDel = true;
        for (auto& item : o.items) {
            if (item.status != OrderStatus::Completed && item.status != OrderStatus::Cancelled && item.status != OrderStatus::Damaged) allDone = false;
            if (item.status != OrderStatus::Delivered && item.status != OrderStatus::Completed && item.status != OrderStatus::Cancelled && item.status != OrderStatus::Damaged) allDel = false;
        }
        if (allDel) o.status = OrderStatus::Delivered;
        if (allDone) o.status = OrderStatus::Completed;
    }

    void handlePayment(ClientInfo& cl, const std::vector<u8>& payload) {
        std::stringstream ss; ss.write(reinterpret_cast<const char*>(&payload[1]), payload.size() - 1);
        PaymentRequest req; I i{ss}; i >> req;
        double total = 0;
        for (auto& o : state.orders) {
            if (o.tableId == req.tableId && o.status != OrderStatus::Completed && o.status != OrderStatus::Cancelled && o.status != OrderStatus::Damaged) {
                o.paymentMethod = req.paymentMethod; o.discount = req.discount;
                o.status = OrderStatus::Completed; total += o.totalPrice * o.discount;
            }
        }
        for (auto& t : state.tables) { if (t.id == req.tableId) { t.status = TableStatus::Dirty; break; } }
        LOG("PAYMENT", "Table %d %.2f %s", req.tableId, total, req.paymentMethod.c_str());
        sendNotification("桌" + std::to_string(req.tableId) + " 结账 " + std::to_string(total).substr(0, 6), 0);
        broadcastState();
    }

    void handleChangeTable(ClientInfo& cl, const std::vector<u8>& payload) {
        std::stringstream ss; ss.write(reinterpret_cast<const char*>(&payload[1]), payload.size() - 1);
        ChangeTableRequest req; I i{ss}; i >> req;
        Table *ft = nullptr, *tt = nullptr;
        for (auto& t : state.tables) { if (t.id == req.fromTableId) ft = &t; if (t.id == req.toTableId) tt = &t; }
        if (!ft || !tt) { sendNotification("餐桌不存在", 2, cl.role); return; }
        if (tt->status != TableStatus::Available) { sendNotification("目标桌不可用", 2, cl.role); return; }
        for (auto& o : state.orders) { if (o.tableId == req.fromTableId) { o.tableId = req.toTableId; o.tableName = tt->name; } }
        tt->status = ft->status; ft->status = TableStatus::Available;
        LOG("CHANGE", "%s->%s", ft->name.c_str(), tt->name.c_str());
        broadcastState();
    }

    void handleUrgeDish(ClientInfo&, const std::vector<u8>& payload) {
        if (payload.size() < 5) return;
        std::stringstream ss; ss.write(reinterpret_cast<const char*>(&payload[1]), payload.size() - 1);
        UrgeDishRequest req; I i{ss}; i >> req;
        Order* o = nullptr; for (auto& x : state.orders) { if (x.id == req.orderId) { o = &x; break; } }
        if (!o) return;
        sendNotification("催菜!" + o->tableName + " #" + std::to_string(req.orderId), 3, Role::Chef);
        sendNotification("催菜!" + o->tableName + " #" + std::to_string(req.orderId), 3, Role::HeadChef);
        broadcastState();
    }

    void handleMergeOrders(ClientInfo&, const std::vector<u8>& payload) {
        if (payload.size() < 9) return;
        std::stringstream ss; ss.write(reinterpret_cast<const char*>(&payload[1]), payload.size() - 1);
        MergeOrdersRequest req; I i{ss}; i >> req;
        for (auto& o : state.orders) {
            if (o.tableId == req.tableId2) { o.tableId = req.tableId1; for (auto& t : state.tables) { if (t.id == req.tableId1) { o.tableName = t.name; break; } } }
        }
        for (auto& t : state.tables) { if (t.id == req.tableId2) t.status = TableStatus::Dirty; }
        LOG("MERGE", "%d+%d", req.tableId1, req.tableId2);
        broadcastState();
    }
};

// ==================== 主循环 ====================
int main() {
    INIT_StreamPunk();
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET ls = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    sockaddr_in addr = {}; addr.sin_family = AF_INET; addr.sin_port = htons(9004); addr.sin_addr.s_addr = INADDR_ANY;
    bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    listen(ls, SOMAXCONN);
    LOG("INIT", "=== Restaurant System ws://0.0.0.0:9004 ===");

    RestaurantEngine eng; eng.initData();
    std::vector<SOCKET> pending;
    u_long nb = 1; ioctlsocket(ls, FIONBIO, &nb);

    while (true) {
        SOCKET cs = accept(ls, nullptr, nullptr);
        if (cs != INVALID_SOCKET) { ioctlsocket(cs, FIONBIO, &nb); pending.push_back(cs); }

        for (auto it = pending.begin(); it != pending.end();) {
            std::string req = readHttpRequest(*it);
            if (!req.empty()) {
                if (doHandshake(*it, req)) {
                    std::lock_guard<std::mutex> lk(eng.mtx);
                    int cid = eng.nextClientId++;
                    eng.clients.emplace_back(*it, cid);
                    LOG("CONNECT", "Client #%d", cid);
                } else closesocket(*it);
                it = pending.erase(it);
            } else ++it;
        }

        {
            std::lock_guard<std::mutex> lk(eng.mtx);
            for (auto& cl : eng.clients) {
                if (!cl.connected) continue;
                char buf[65536]; int n = recv(cl.sock, buf, sizeof(buf), 0);
                if (n > 0) {
                    cl.recvBuf.insert(cl.recvBuf.end(), reinterpret_cast<u8*>(buf), reinterpret_cast<u8*>(buf) + n);
                    size_t consumed; WsOpcode op; std::vector<u8> pl;
                    while (parseWsFrame(cl.recvBuf, consumed, op, pl)) {
                        cl.recvBuf.erase(cl.recvBuf.begin(), cl.recvBuf.begin() + consumed);
                        if (op == WsOpcode::Binary && pl.size() > 0) {
                            MsgType t = static_cast<MsgType>(pl[0]);
                            switch (t) {
                            case MsgType::RoleLogin: {
                                std::stringstream ss; ss.write(reinterpret_cast<const char*>(&pl[1]), pl.size() - 1);
                                RoleLoginRequest req; I i{ss}; i >> req;
                                cl.role = req.role; cl.name = req.name;
                                i32 tableId = 0;
                                if (req.role == 9) {
                                    for (auto& t : eng.state.tables) {
                                        if (t.name == req.name) { tableId = t.id; break; }
                                    }
                                }
                                eng.sendRoleAssigned(cl.sock, req.role, req.name, tableId);
                                eng.broadcastState();
                                LOG("ROLE", "#%d role=%d %s table=%d", cl.id, req.role, req.name.c_str(), tableId);
                                break;
                            }
                            case MsgType::PlaceOrder: eng.handlePlaceOrder(cl, pl); break;
                            case MsgType::UpdateStatus: eng.handleUpdateStatus(cl, pl); break;
                            case MsgType::Payment: eng.handlePayment(cl, pl); break;
                            case MsgType::ChangeTable: eng.handleChangeTable(cl, pl); break;
                            case MsgType::UrgeDish: eng.handleUrgeDish(cl, pl); break;
                            case MsgType::MergeOrders: eng.handleMergeOrders(cl, pl); break;
                            default: break;
                            }
                        } else if (op == WsOpcode::Close) cl.connected = false;
                    }
                } else if (n == 0 || WSAGetLastError() == WSAECONNRESET) cl.connected = false;
            }
            for (auto it = eng.clients.begin(); it != eng.clients.end();) {
                if (!it->connected) { LOG("DC", "#%d", it->id); closesocket(it->sock); it = eng.clients.erase(it); }
                else ++it;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    closesocket(ls); WSACleanup(); return 0;
}