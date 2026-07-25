// 示例 11：UseDataPod — 不继承 Base 的 POD 类型序列化
// 展示 UseDataPod / UseDataPodXt / 嵌套 Pod / 容器成员 等用法
//
// UseDataPod 适用场景：
//   - 不需要多态序列化（不通过基类指针序列化）
//   - 不需要深拷贝（deepCopyFrom）
//   - 不需要 JSON 序列化
//   优点：零虚函数开销，更轻量

#include "../../include/stream-punk/StreamPunk.hpp"
using namespace sp;
#include <iostream>
#include <sstream>
#include <vector>
#include <optional>
#include <map>

// ===================== 1. 基本 UseDataPod =====================
// 使用 Xt_##TypeName 命名约定，与 UseData 风格一致

struct Point2D {
#define Xt_Point2D(X__) \
    X__(f64, x, 0.0) \
    X__(f64, y, 0.0)

    UseDataPod(Point2D);
};

// ===================== 2. UseDataPodXt — 显式指定 Xt 宏名 =====================
// 不依赖 Xt_##TypeName 命名约定，可自定义宏名

#define Xt_Color(X__) \
    X__(u8, r, 0) \
    X__(u8, g, 0) \
    X__(u8, b, 0) \
    X__(u8, a, 255)

struct Color {
    UseDataPodXt(Color, Xt_Color);
};

// ===================== 3. 嵌套 UseDataPod =====================
// Pod 类型可以嵌套其他 Pod 类型

struct Rect {
#define Xt_Rect(X__) \
    X__(Point2D, origin, Point2D{}) \
    X__(f64, width, 0.0) \
    X__(f64, height, 0.0)

    UseDataPod(Rect);
};

// ===================== 4. UseDataPod 含容器和 optional =====================
// 支持 STL 容器和 optional，模板参数中逗号用 DH 替代

struct Player {
#define Xt_Player(X__) \
    X__(std::string, name, "") \
    X__(i32, score, 0) \
    X__(std::vector<i32>, items, std::vector<i32>{}) \
    X__(std::optional<std::string>, title, std::optional<std::string>{}) \
    X__(std::map<std::string DH i32>, stats, std::map<std::string DH i32>{})

    UseDataPod(Player);
};

int main() {
    // ---- 基本往返 ----
    {
        Point2D p1{3.14, 2.71};
        std::stringstream ss;
        O o(ss);
        o << p1;

        Point2D p2;
        I i(ss);
        i >> p2;
        std::cout << "Point2D: (" << p2.x << ", " << p2.y << ")"
                  << "  size=" << ss.str().size() << " bytes" << std::endl;
    }

    // ---- UseDataPodXt 显式宏名 ----
    {
        Color c1{255, 128, 64, 255};
        std::stringstream ss;
        O o(ss);
        o << c1;

        Color c2;
        I i(ss);
        i >> c2;
        std::cout << "Color: rgba(" << (int)c2.r << "," << (int)c2.g
                  << "," << (int)c2.b << "," << (int)c2.a << ")"
                  << "  size=" << ss.str().size() << " bytes" << std::endl;
    }

    // ---- 嵌套 Pod ----
    {
        Rect r1;
        r1.origin = Point2D{10.0, 20.0};
        r1.width = 100.0;
        r1.height = 50.0;

        std::stringstream ss;
        O o(ss);
        o << r1;

        Rect r2;
        I i(ss);
        i >> r2;
        std::cout << "Rect: origin=(" << r2.origin.x << "," << r2.origin.y
                  << ") size=" << r2.width << "x" << r2.height
                  << "  bytes=" << ss.str().size() << std::endl;
    }

    // ---- 容器和 optional ----
    {
        Player p1;
        p1.name = "Alice";
        p1.score = 9999;
        p1.items = {1, 2, 3, 5, 8};
        p1.title = "Champion";
        p1.stats = {{"wins", 42}, {"losses", 7}};

        std::stringstream ss;
        O o(ss);
        o << p1;

        Player p2;
        I i(ss);
        i >> p2;
        std::cout << "Player: " << p2.name << " score=" << p2.score
                  << " items=" << p2.items.size()
                  << " title=" << (p2.title.has_value() ? *p2.title : "none")
                  << " stats=" << p2.stats.size()
                  << "  bytes=" << ss.str().size() << std::endl;
    }

    return 0;
}