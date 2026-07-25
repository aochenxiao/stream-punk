// 跨语言测试数据生成器
// 生成各种测试用的二进制数据文件，供 Node.js TS 测试读取
#include "web_server.hpp"
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

void writeFile(const std::string& name, const std::string& data) {
    // 使用 SolutionDir 作为基准路径
    fs::path outDir = fs::path(__FILE__).parent_path().parent_path() / "cross-lang-tests" / "test-data";
    fs::create_directories(outDir);
    std::ofstream out(outDir / name, std::ios::binary);
    out.write(data.data(), data.size());
    std::cout << "  Generated: " << name << " (" << data.size() << " bytes)" << std::endl;
}

int main() {
    std::cout << "=== StreamPunk Cross-Language Test Data Generator ===" << std::endl;
    std::cout << "Generating test data files for Node.js TS tests...\n" << std::endl;

    // 1. 基础类型测试数据
    std::cout << "1. Basic Types:" << std::endl;
    writeFile("basic_types.bin", genBasicTypes());

    // 2. 全量类型测试
    std::cout << "\n2. Full Type Test:" << std::endl;
    writeFile("full_types.bin", test1());

    // 3. SmartHomeSystem
    std::cout << "\n3. SmartHomeSystem:" << std::endl;
    writeFile("home_system.bin", genHomeSystem());

    // 4. 传感器更新数据（10个tick）
    std::cout << "\n4. Sensor Updates:" << std::endl;
    for (int i = 0; i < 10; i++) {
        writeFile("sensor_" + std::to_string(i) + ".bin", genSensorUpdate(i));
    }

    // 5. 实时批量数据
    std::cout << "\n5. Realtime Batch:" << std::endl;
    for (int i = 0; i < 5; i++) {
        writeFile("realtime_" + std::to_string(i) + ".bin", genRealtimeBatch(i));
    }

    std::cout << "\n=== All test data generated successfully! ===" << std::endl;
    std::cout << "Output directory: cross-lang-tests/test-data/" << std::endl;
    return 0;
}