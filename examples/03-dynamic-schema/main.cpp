// 示例 03：动态 Schema 解析
// 展示：C++ 端生成 Schema → 动态语言端用通用解析器解析数据
// 与预生成代码模式对比：类型变化时无需重新编译客户端

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../00-demo-types/Data.hpp"
using namespace sp;
#include "../common/locale_init.hpp"
#include "../../include/stream-punk/StreamPunkJson.hpp"
#include "../../include/stream-punk/StreamPunkSchema.hpp"
#include <iostream>
#include <sstream>
#include <fstream>

// 模拟一个 IoT 设备上报的数据结构
struct SensorData : public Base {
    #define Xt_SensorData(X__) \
    X__(std::string, deviceId, "unknown") \
    X__(f64, temperature, 0.0) \
    X__(f64, humidity, 0.0) \
    X__(i64, timestamp, 0)

    SensorData() = default;
    UseData(SensorData);
    UseDataJson(SensorData);
};

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    // 1. 创建示例对象，用于生成 Schema
    SensorData example;
    std::vector<Base*> examples = {&example};

    // 2. 生成 Schema（描述所有类型结构）
    auto schemas = sp::buildSchemas(examples);
    std::string schemaJson = schemas.dump(2);
    std::cout << "=== Schema 元数据 ===" << std::endl;
    std::cout << schemaJson << std::endl;

    // 3. 将 Schema 保存为文件（实际场景中通过网络发送给客户端）
    std::ofstream schemaFile("sensor_schema.json");
    schemaFile << schemaJson;
    schemaFile.close();
    std::cout << "Schema 已保存到 sensor_schema.json" << std::endl;

    // 4. 构造并序列化数据
    SensorData data;
    data.deviceId = "TH-001";
    data.temperature = 25.6;
    data.humidity = 68.3;
    data.timestamp = 1717171200000;

    std::stringstream ss;
    O output{ss};
    output << data;

    std::ofstream dataFile("sensor_data.bin", std::ios::binary);
    dataFile << ss.str();
    dataFile.close();
    std::cout << "数据已保存到 sensor_data.bin (" << ss.str().size() << " bytes)" << std::endl;

    std::cout << std::endl;
    std::cout << "--- 动态 Schema 模式的优势 ---" << std::endl;
    std::cout << "1. 将 sensor_schema.json 发送给 Python/JS 客户端" << std::endl;
    std::cout << "2. 客户端加载 Schema 后，用通用解析器即可解析所有后续数据" << std::endl;
    std::cout << "3. 如果 SensorData 新增字段，只需重新发送 Schema 即可" << std::endl;
    std::cout << "4. 无需为每个类型预生成代码" << std::endl;
    std::cout << std::endl;
    std::cout << "Python 端示例：" << std::endl;
    std::cout << "  from stream_punk import SchemaRegistry, I" << std::endl;
    std::cout << "  registry = SchemaRegistry()" << std::endl;
    std::cout << "  registry.load_schema(open('sensor_schema.json').read())" << std::endl;
    std::cout << "  reader = I(binary_data, registry)" << std::endl;
    std::cout << "  obj = reader.read_any()  # 通用解析" << std::endl;
    std::cout << "  print(obj.deviceId, obj.temperature)" << std::endl;

    return 0;
}