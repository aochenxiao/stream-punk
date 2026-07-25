// 示例 02：C++ ↔ TypeScript 跨语言序列化
// 展示：C++ 定义类型 → 生成 TS 代码 → 二进制数据在两端互通

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../00-demo-types/Data.hpp"
using namespace sp;
#include "../common/locale_init.hpp"
#include <iostream>
#include <sstream>
#include <fstream>

// 定义跨语言共享的类型
struct ChatMessage : public Base {
    #define Xt_ChatMessage(X__) \
    X__(std::string, sender, "") \
    X__(std::string, content, "") \
    X__(i64, timestamp, 0)

    ChatMessage() = default;
    UseData(ChatMessage);
};

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    // 1. 构造数据
    ChatMessage msg;
    msg.sender = "Alice";
    msg.content = "Hello from C++!";
    msg.timestamp = 1717171200000;

    // 2. 序列化到二进制文件
    std::stringstream ss;
    O output{ss};
    output << msg;

    std::ofstream file("chat_message.bin", std::ios::binary);
    file << ss.str();
    file.close();

    std::cout << "已生成 chat_message.bin (" << ss.str().size() << " bytes)" << std::endl;
    std::cout << std::endl;
    std::cout << "--- 下一步 ---" << std::endl;
    std::cout << "1. 运行 sp-gen ts 生成 TypeScript 类型代码" << std::endl;
    std::cout << "2. 复制 runtimes/ts/stream-punk.ts 到前端项目" << std::endl;
    std::cout << "3. 在 TypeScript 端用 I 读取 chat_message.bin" << std::endl;
    std::cout << std::endl;
    std::cout << "TypeScript 端示例代码：" << std::endl;
    std::cout << "  import { ChatMessage, I } from './stream-punk';" << std::endl;
    std::cout << "  const data = readFileSync('chat_message.bin');" << std::endl;
    std::cout << "  const msg = new ChatMessage().from(new I(data));" << std::endl;
    std::cout << "  console.log(msg.sender, msg.content);" << std::endl;

    return 0;
}