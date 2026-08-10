// 示例 20：多语言微服务网格
// 展示 StreamPunk 在微服务场景下的跨语言通信能力：
//   C++ 认证服务 + Python 数据分析 + Go API 网关，
//   三服务之间通过 SPOI 协议互相查询，全程二进制通信
//
// 场景：微服务架构中，不同语言的服务需要互相查询数据，
//       StreamPunk 提供统一的 SPOI 查询协议，任何语言都可以
//       当查询方，任何语言都可以当被查询方

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/StreamPunkJson.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <iomanip>

using namespace sp;

// ============================================================
// 1. 定义微服务数据类型
// ============================================================

// 用户信息（认证服务）
struct UserInfo : public Base {
#define Xt_UserInfo(X__) \
    X__(std::string, userId, "") \
    X__(std::string, username, "") \
    X__(std::string, role, "user") \
    X__(i32, level, 1) \
    X__(std::chrono::system_clock::time_point, createdAt, {})

    UserInfo() = default;
    UseData(UserInfo);
    UseDataJson(UserInfo);
};
REGISTER_JSON_TYPE(UserInfo);
template<> struct TypeDesc<UserInfo> : TypeDescCustom<UserInfo> {};

// 数据指标（分析服务）
struct MetricPoint : public Base {
#define Xt_MetricPoint(X__) \
    X__(std::string, metricName, "") \
    X__(f64, value, 0.0) \
    X__(std::string, serviceName, "") \
    X__(std::chrono::system_clock::time_point, timestamp, {})

    MetricPoint() = default;
    UseData(MetricPoint);
    UseDataJson(MetricPoint);
};
REGISTER_JSON_TYPE(MetricPoint);
template<> struct TypeDesc<MetricPoint> : TypeDescCustom<MetricPoint> {};

// API 请求日志（网关服务）
struct ApiRequest : public Base {
#define Xt_ApiRequest(X__) \
    X__(std::string, requestId, "") \
    X__(std::string, userId, "") \
    X__(std::string, endpoint, "") \
    X__(i32, statusCode, 200) \
    X__(f64, latencyMs, 0.0) \
    X__(std::chrono::system_clock::time_point, timestamp, {})

    ApiRequest() = default;
    UseData(ApiRequest);
    UseDataJson(ApiRequest);
};
REGISTER_JSON_TYPE(ApiRequest);
template<> struct TypeDesc<ApiRequest> : TypeDescCustom<ApiRequest> {};

// 微服务网格注册表
struct ServiceMesh : public Base {
    using UserVec = std::vector<UserInfo>;
    using MetricVec = std::vector<MetricPoint>;
    using RequestVec = std::vector<ApiRequest>;

#define Xt_ServiceMesh(X__) \
    X__(UserVec, users, {}) \
    X__(MetricVec, metrics, {}) \
    X__(RequestVec, requests, {}) \
    X__(std::string, meshName, "default")

    ServiceMesh() = default;
    UseData(ServiceMesh);
    UseDataJson(ServiceMesh);
};
REGISTER_JSON_TYPE(ServiceMesh);
template<> struct TypeDesc<ServiceMesh> : TypeDescCustom<ServiceMesh> {};

// ============================================================
// 2. 服务模拟
// ============================================================

// 模拟 C++ 认证服务
class AuthService {
public:
    std::vector<UserInfo> users;

    void init() {
        std::vector<std::string> names = {"alice", "bob", "charlie", "diana", "eve"};
        std::vector<std::string> roles = {"admin", "user", "user", "moderator", "user"};
        for (size_t i = 0; i < names.size(); ++i) {
            UserInfo u;
            u.userId = "usr-" + std::to_string(1000 + i);
            u.username = names[i];
            u.role = roles[i];
            u.level = (int)(i + 1) * 10;
            u.createdAt = std::chrono::system_clock::now();
            users.push_back(u);
        }
    }

    // 模拟 SPOI 查询：按角色过滤
    std::vector<UserInfo> queryByRole(std::string const& role) {
        std::vector<UserInfo> result;
        for (auto& u : users) {
            if (u.role == role) result.push_back(u);
        }
        return result;
    }

    // 模拟 SPOI 查询：查单个用户
    UserInfo const* findUser(std::string const& userId) {
        for (auto& u : users) {
            if (u.userId == userId) return &u;
        }
        return nullptr;
    }
};

// 模拟 Python 分析服务
class AnalyticsService {
public:
    std::vector<MetricPoint> metrics;

    void init() {
        std::vector<std::string> names = {"cpu_usage", "mem_usage", "qps", "latency_p99", "error_rate"};
        std::vector<double> values = {45.2, 72.8, 1250.0, 230.5, 0.5};
        for (size_t i = 0; i < names.size(); ++i) {
            MetricPoint m;
            m.metricName = names[i];
            m.value = values[i];
            m.serviceName = "auth-service";
            m.timestamp = std::chrono::system_clock::now();
            metrics.push_back(m);
        }
    }

    // 模拟 SPOI 查询：过滤高负载指标
    std::vector<MetricPoint> highLoadMetrics(double threshold) {
        std::vector<MetricPoint> result;
        for (auto& m : metrics) {
            if (m.value > threshold) result.push_back(m);
        }
        return result;
    }
};

// 模拟 Go API 网关
class ApiGateway {
public:
    std::vector<ApiRequest> requests;

    void init() {
        std::vector<std::string> endpoints = {"/api/users", "/api/items", "/api/order", "/api/login", "/api/stats"};
        std::vector<int> codes = {200, 200, 500, 200, 200};
        std::vector<double> latencies = {12.5, 45.2, 320.0, 8.3, 67.1};
        for (size_t i = 0; i < endpoints.size(); ++i) {
            ApiRequest r;
            r.requestId = "req-" + std::to_string(9000 + i);
            r.userId = "usr-" + std::to_string(1000 + (i % 5));
            r.endpoint = endpoints[i];
            r.statusCode = codes[i];
            r.latencyMs = latencies[i];
            r.timestamp = std::chrono::system_clock::now();
            requests.push_back(r);
        }
    }

    // 模拟 SPOI 查询：慢请求
    std::vector<ApiRequest> slowRequests(double thresholdMs) {
        std::vector<ApiRequest> result;
        for (auto& r : requests) {
            if (r.latencyMs > thresholdMs) result.push_back(r);
        }
        return result;
    }

    // 模拟 SPOI 查询：错误请求
    std::vector<ApiRequest> errorRequests() {
        std::vector<ApiRequest> result;
        for (auto& r : requests) {
            if (r.statusCode >= 400) result.push_back(r);
        }
        return result;
    }
};

// ============================================================
// 3. 主演示
// ============================================================

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    std::cout << "======================================================" << std::endl;
    std::cout << "  StreamPunk 多语言微服务网格 演示" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << std::endl;

    std::cout << "  架构：" << std::endl;
    std::cout << "  ┌───────────────┐  SPOI  ┌───────────────┐" << std::endl;
    std::cout << "  │  C++ 认证服务  │◄──────►│  Go API 网关   │" << std::endl;
    std::cout << "  └───────┬───────┘        └───────┬───────┘" << std::endl;
    std::cout << "          │ SPOI                   │ SPOI" << std::endl;
    std::cout << "  ┌───────┴───────┐                │" << std::endl;
    std::cout << "  │ Python 分析   │◄───────────────┘" << std::endl;
    std::cout << "  └───────────────┘" << std::endl;
    std::cout << "  任何语言都可以当查询方，任何语言都可以当被查询方" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 1：初始化各服务 ----
    std::cout << "【阶段 1】初始化各微服务" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    AuthService auth;
    auth.init();
    std::cout << "  C++ 认证服务：已加载 " << auth.users.size() << " 个用户" << std::endl;

    AnalyticsService analytics;
    analytics.init();
    std::cout << "  Python 分析服务：已加载 " << analytics.metrics.size() << " 个指标" << std::endl;

    ApiGateway gateway;
    gateway.init();
    std::cout << "  Go API 网关：已记录 " << gateway.requests.size() << " 条请求" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 2：跨服务 SPOI 查询 ----
    std::cout << "【阶段 2】跨服务 SPOI 查询" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    // 2.1 Go 网关 → C++ 认证服务：查询管理员用户
    std::cout << "  [Go 网关 → C++ 认证服务] 查询角色=admin 的用户" << std::endl;
    std::cout << "    SPOI: filter(role == \"admin\")" << std::endl;
    auto admins = auth.queryByRole("admin");
    for (auto& u : admins) {
        std::cout << "      " << u.username << " (role=" << u.role << ", level=" << u.level << ")" << std::endl;
    }
    std::cout << std::endl;

    // 2.2 Python 分析 → C++ 认证服务：查询用户详情
    std::cout << "  [Python 分析 → C++ 认证服务] 查询用户 usr-1002" << std::endl;
    std::cout << "    SPOI: filter(userId == \"usr-1002\")" << std::endl;
    auto* user = auth.findUser("usr-1002");
    if (user) {
        std::cout << "      " << user->username << " (role=" << user->role << ", level=" << user->level << ")" << std::endl;
    }
    std::cout << std::endl;

    // 2.3 Go 网关 → Python 分析：查询高负载指标
    std::cout << "  [Go 网关 → Python 分析] 查询高负载指标(value > 50)" << std::endl;
    std::cout << "    SPOI: filter(value > 50)" << std::endl;
    auto highLoad = analytics.highLoadMetrics(50.0);
    for (auto& m : highLoad) {
        std::cout << "      " << m.metricName << " = " << m.value << " (" << m.serviceName << ")" << std::endl;
    }
    std::cout << std::endl;

    // 2.4 Python 分析 → Go 网关：查询慢请求
    std::cout << "  [Python 分析 → Go 网关] 查询延迟 > 50ms 的请求" << std::endl;
    std::cout << "    SPOI: filter(latencyMs > 50)" << std::endl;
    auto slowReqs = gateway.slowRequests(50.0);
    for (auto& r : slowReqs) {
        std::cout << "      " << r.endpoint << " latency=" << r.latencyMs << "ms" << std::endl;
    }
    std::cout << std::endl;

    // 2.5 C++ 认证 → Go 网关：查询错误请求
    std::cout << "  [C++ 认证 → Go 网关] 查询错误请求(statusCode >= 400)" << std::endl;
    std::cout << "    SPOI: filter(statusCode >= 400)" << std::endl;
    auto errors = gateway.errorRequests();
    for (auto& r : errors) {
        std::cout << "      " << r.endpoint << " status=" << r.statusCode << std::endl;
    }
    std::cout << std::endl;

    // ---- 阶段 3：二进制序列化对比 ----
    std::cout << "【阶段 3】二进制序列化 vs JSON" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    ServiceMesh mesh;
    mesh.meshName = "production";
    mesh.users = auth.users;
    mesh.metrics = analytics.metrics;
    mesh.requests = gateway.requests;

    std::stringstream binSS;
    O binOut(binSS);
    binOut << mesh;
    size_t binSize = binSS.str().size();

    std::stringstream jsonSS;
    mesh.toJsonStream(jsonSS);
    std::string jsonStr = jsonSS.str();

    std::cout << "  全量数据：" << std::endl;
    std::cout << "    二进制: " << binSize << " bytes" << std::endl;
    std::cout << "    JSON:   " << jsonStr.size() << " bytes" << std::endl;
    std::cout << "    节省:   " << std::fixed << std::setprecision(1)
              << (1.0 - (double)binSize / jsonStr.size()) * 100 << "%" << std::endl;
    std::cout << "  说明：SPOI 查询只返回结果，不传输全量数据，更省带宽" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 4：跨语言代码生成 ----
    std::cout << "【阶段 4】跨语言代码生成" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  C++ 定义类型后，运行 sp-gen 一键生成各语言代码：" << std::endl;
    std::cout << "    sp-gen -t py-meta  -p ./data.py      → Python 类型" << std::endl;
    std::cout << "    sp-gen -t ts-meta  -p ./data.ts      → TypeScript 类型" << std::endl;
    std::cout << "    sp-gen -t go-meta  -p ./data.go      → Go 类型" << std::endl;
    std::cout << "    sp-gen -t spoi-py  -p ./builder.py   → Python SPOI Builder" << std::endl;
    std::cout << "    sp-gen -t spoi-go  -p ./builder.go   → Go SPOI Builder" << std::endl;
    std::cout << std::endl;
    std::cout << "  改了 C++ 类型？重跑 sp-gen，所有语言自动同步" << std::endl;
    std::cout << "  编译报错提醒遗漏，不会等到运行时才出问题" << std::endl;
    std::cout << std::endl;

    // ---- 总结 ----
    std::cout << "======================================================" << std::endl;
    std::cout << "  总结" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << "  1. 跨语言 SPOI：C++ ↔ Python ↔ Go 互相查询" << std::endl;
    std::cout << "  2. 二进制协议：指令 + 结果都是二进制，极省带宽" << std::endl;
    std::cout << "  3. 内存直查：不经过 SQL 解析器，比 NoSQL 还快" << std::endl;
    std::cout << "  4. 类型安全：sp-gen 编译期保证类型一致" << std::endl;
    std::cout << "  5. 适用场景：微服务治理、多语言混部、服务网格" << std::endl;
    std::cout << std::endl;

    return 0;
}