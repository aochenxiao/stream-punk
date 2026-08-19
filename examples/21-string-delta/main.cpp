// 示例 21：SPOI 字符串增量更新（std::string 当作字符容器）
// 展示：在 SPOI 层面对 std::string 字段做子串级 insert / erase / replace / move / append
//       —— shadow 端生成增量指令，executor 端应用到副本，全程只传变化的部分。
//
// 新增能力（v0.1 之前 string 只能整体覆盖）：
//   shadow.s.append(chunk)        e_append  追加到末尾
//   shadow.s.insert(pos, chunk)   e_insert  在 pos 处插入子串
//   shadow.s.erase(pos, len)      e_remove  删除 [pos, pos+len) 子串（len 默认 1）
//   shadow.s.replace(pos, len, chunk) e_replace 用 chunk 替换 [pos, pos+len)
//   shadow.s.move(from, len, to)  e_move    把 [from, from+len) 子串搬到 to 处（原子指令）
// 位置/长度越界时自动钳制（clamp）——与容器元素操作的语义一致；
// 注意与原生 std::string 的区别：原生 string 越界抛 out_of_range，SPOI 按钳制处理。

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../../include/stream-punk/StreamPunkJson.hpp" // 提供 Base::fromJsonStream 定义
#include "../../include/stream-punk/StreamPunkSPOIShadow.hpp"
#include "../../include/stream-punk/StreamPunkSPOIExecutor.hpp"
using namespace sp;
#include <iostream>
#include <sstream>
#include <string>

namespace sp {

// 一个文档/聊天消息结构：字符串字段 + 容器字段（验证两者共存）
struct DocState : public Base {
    #define Xt_DocState(X__) \
    X__(std::string, content, "") \
    X__(std::string, title, "untitled") \
    X__(std::vector<std::string>, tags, {})

    DocState() = default;
    UseData(DocState);
};
UseSPOIShadow(DocState, Xt_DocState);

} // namespace sp

// 简易断言
static int g_failures = 0;
static void check(bool ok, const char* what) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << what << "\n";
    if (!ok) ++g_failures;
}

// 把 shadow 写出的 SPOI 流应用到副本，并断言结果与 golden 一致
// 注意：shadow 只记录指令、不修改源对象，因此以 golden 为基准，而非源对象。
static void applyAndVerify(sp::DocState& replica, std::stringstream& stream,
                           const std::string& expect, const char* what) {
    stream.seekg(0);
    sp::SpoiExecutor exec(stream);
    exec >> replica;
    check(replica.content == expect, what);
}

// 与 SPOI 字符串操作一致的钳制语义 golden（显式处理越界，不抛异常）
// 注意：原生 std::string 越界会抛 out_of_range，这里手动 clamp 与 executor 语义对齐。
static std::string goldenClamp(std::string s) {
    // 1) insert(999, "XYZ")：pos 超长 → 追加到末尾
    s.insert(std::min<size_t>(999, s.size()), "XYZ");
    // 2) erase(100, 10)：pos 超长 → 不删
    s.erase(std::min<size_t>(100, s.size()), 10);
    // 3) replace(1, 100, "Q")：len 超长 → 截断到末尾
    s.replace(1, std::min<size_t>(100, s.size() - 1), "Q");
    // 4) move(0, 100, 1)：len 超长 → 只搬剩余部分
    std::string ch = s.substr(0, std::min<size_t>(100, s.size()));
    s.erase(0, std::min<size_t>(100, s.size()));
    s.insert(std::min<size_t>(1, s.size()), ch);
    return s;
}

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    std::cout << "=== 21. SPOI 字符串增量更新 ===\n\n";

    // ===== 1. 基线：整体赋值（原有能力，仍然可用） =====
    sp::DocState state;
    state.content = "Hello, World!";
    state.title   = "greeting";
    state.tags    = {"intro", "demo"};

    sp::DocState replica = state; // 接收端已有全量副本

    std::cout << "--- 1. 子串 insert / erase / replace / move / append ---\n";

    // golden reference：用 std::string 自己的语义直接算出期望结果
    std::string expect = state.content;
    std::stringstream stream1;

    {
        auto s = sp::spoi(state, stream1);

        // append
        s.content.append("!!");
        expect.append("!!");

        // insert：在偏移 7 处插入
        s.content.insert(7, "SPOI ");
        expect.insert(7, "SPOI ");

        // erase：删除前 5 个字符（"Hello"）
        s.content.erase(0, 5);
        expect.erase(0, 5);

        // replace：把 [2, 2+4) 换成 "NEW"
        s.content.replace(2, 4, "NEW");
        expect.replace(2, 4, "NEW");

        // move：把 [1, 1+3) 的子串搬到位置 6（目标按擦除后的串计）
        s.content.move(1, 3, 6);
        std::string chunk = expect.substr(1, 3);
        expect.erase(1, 3);
        expect.insert(6, chunk);

        // 顺带改容器字段，验证与字符串操作互不干扰
        s.tags.append("spoi");
        state.tags.push_back("spoi");
        s.title = "greeting_v2";
        state.title = "greeting_v2";
    }

    std::cout << "  golden 结果: \"" << expect << "\"\n";
    std::cout << "  shadow 增量: " << stream1.str().size() << " bytes\n";
    applyAndVerify(replica, stream1, expect, "round-trip: 5 条字符串增量全部正确应用");
    std::cout << "  最终 content = \"" << replica.content << "\"\n";
    check(replica.tags == state.tags, "容器字段 append 与字符串操作共存");
    check(replica.title == "greeting_v2", "字符串整体赋值 e_set 仍然可用");

    // ===== 2. 越界钳制：任意非法参数都不崩溃，语义与容器元素操作一致 =====
    std::cout << "\n--- 2. 越界钳制（clamp） ---\n";
    sp::DocState a, b;
    a.content = "abc";
    b.content = a.content;
    std::stringstream stream2;
    {
        auto s = sp::spoi(a, stream2);
        s.content.insert(999, "XYZ");   // pos 超长 → 追加到末尾
        s.content.erase(100, 10);       // pos 超长 → 不删
        s.content.replace(1, 100, "Q"); // len 超长 → 截断到末尾
        s.content.move(0, 100, 1);      // len 超长 → 只搬剩余部分
    }
    std::string ea = goldenClamp("abc");
    applyAndVerify(b, stream2, ea, "round-trip: 越界钳制场景");
    check(b.content == ea, "越界参数全部安全钳制且语义正确");
    std::cout << "  content = \"" << b.content << "\"\n";

    // ===== 3. 增量 vs 全量：只传变化的部分 =====
    std::cout << "\n--- 3. 增量 vs 全量 ---\n";
    {
        std::stringstream deltaStream;
        {
            auto s = sp::spoi(state, deltaStream);
            s.content.erase(0, 6);           // 只删一个小片段
        }
        std::stringstream fullStream;
        O o(fullStream);
        o << state;
        size_t deltaSize = deltaStream.str().size();
        size_t fullSize  = fullStream.str().size();
        std::cout << "  增量指令: " << deltaSize << " bytes, 全量: " << fullSize << " bytes\n";
        std::cout << "  节省: " << (1.0 - (double)deltaSize / fullSize) * 100 << "%\n";
        check(deltaSize < fullSize, "增量明显小于全量");
    }

    std::cout << "\n说明：字符串操作码路径约定 ——\n";
    std::cout << "  append/insert/replace 复用 0x06/0x08/0x09，erase 复用 0x07(e_remove)\n";
    std::cout << "  新增 0x23 e_move（原子搬移）；path 尾部分别携带 pos / (from,len,to)\n";
    std::cout << "  erase 的 len 与 replace 的 (len+chunk) 编码在 operand 中，复用 SP 序列化格式\n";

    if (g_failures == 0) std::cout << "\n全部断言通过 ✔\n";
    else std::cout << "\n" << g_failures << " 项断言失败 ✘\n";
    return g_failures == 0 ? 0 : 1;
}
