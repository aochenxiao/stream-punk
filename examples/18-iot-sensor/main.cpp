// 示例 18：IoT 传感器数据采集与分析
// 展示 StreamPunk 在 IoT 场景下的关键能力：
//   二进制序列化（紧凑数据）→ SPOI 查询（过滤/聚合）→ 跨语言生成
//
// 场景：模拟传感器网关，采集温度、湿度、压力传感器数据，
//       通过 SPOI 查询过滤异常数据，展示紧凑的二进制格式优势

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/StreamPunkJson.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <random>
#include <iomanip>

using namespace sp;

// ============================================================
// 1. 定义传感器数据类型
// ============================================================

// 温度传感器
struct TemperatureReading : public Base {
#define Xt_TemperatureReading(X__) \
    X__(std::string, sensorId, "") \
    X__(std::string, location, "") \
    X__(f64, celsius, 0.0) \
    X__(f64, humidity, 0.0)

    TemperatureReading() = default;
    UseData(TemperatureReading);
    UseDataJson(TemperatureReading);
};
REGISTER_JSON_TYPE(TemperatureReading);
template<> struct TypeDesc<TemperatureReading> : TypeDescCustom<TemperatureReading> {};

// 压力传感器
struct PressureReading : public Base {
#define Xt_PressureReading(X__) \
    X__(std::string, sensorId, "") \
    X__(std::string, location, "") \
    X__(f64, pressureKPa, 0.0) \
    X__(f64, temperature, 0.0)

    PressureReading() = default;
    UseData(PressureReading);
    UseDataJson(PressureReading);
};
REGISTER_JSON_TYPE(PressureReading);
template<> struct TypeDesc<PressureReading> : TypeDescCustom<PressureReading> {};

// 传感器网关（包含所有传感器数据）
struct SensorGateway : public Base {
    using TempVec = std::vector<TemperatureReading>;
    using PressureVec = std::vector<PressureReading>;

#define Xt_SensorGateway(X__) \
    X__(TempVec, temperatures, {}) \
    X__(PressureVec, pressures, {}) \
    X__(i32, totalReadings, 0)

    SensorGateway() = default;
    UseData(SensorGateway);
    UseDataJson(SensorGateway);
};
REGISTER_JSON_TYPE(SensorGateway);
template<> struct TypeDesc<SensorGateway> : TypeDescCustom<SensorGateway> {};

// ============================================================
// 2. 模拟传感器数据生成
// ============================================================

std::mt19937 rng(42);

double randomRange(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(rng);
}

TemperatureReading generateTempReading(std::string const& id, std::string const& loc) {
    TemperatureReading r;
    r.sensorId = id;
    r.location = loc;
    r.celsius = randomRange(15.0, 45.0);
    r.humidity = randomRange(30.0, 90.0);
    return r;
}

PressureReading generatePressureReading(std::string const& id, std::string const& loc) {
    PressureReading r;
    r.sensorId = id;
    r.location = loc;
    r.pressureKPa = randomRange(95.0, 105.0);
    r.temperature = randomRange(20.0, 35.0);
    return r;
}

// ============================================================
// 3. 主演示
// ============================================================

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    std::cout << "======================================================" << std::endl;
    std::cout << "  StreamPunk IoT Sensor Data Collection & Analysis" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 1：模拟传感器数据采集 ----
    std::cout << "[Phase 1] Simulate sensor data collection" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    SensorGateway gateway;
    std::vector<std::string> locations = {"Workshop-A", "Workshop-B", "Warehouse", "Outdoor"};

    for (int i = 0; i < 5; ++i) {
        gateway.temperatures.push_back(
            generateTempReading("TEMP-" + std::to_string(i + 1),
                                locations[i % locations.size()]));
    }

    for (int i = 0; i < 3; ++i) {
        gateway.pressures.push_back(
            generatePressureReading("PRES-" + std::to_string(i + 1),
                                    locations[i % locations.size()]));
    }

    gateway.totalReadings = gateway.temperatures.size() + gateway.pressures.size();

    std::cout << "  Collected " << gateway.temperatures.size() << " temperature readings" << std::endl;
    std::cout << "  Collected " << gateway.pressures.size() << " pressure readings" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 2：二进制序列化 vs JSON 大小对比 ----
    std::cout << "[Phase 2] Binary serialization vs JSON size comparison" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    std::stringstream binSS;
    O binOut(binSS);
    binOut << gateway;
    size_t binSize = binSS.str().size();

    std::stringstream jsonSS;
    gateway.toJsonStream(jsonSS);
    std::string jsonStr = jsonSS.str();

    std::cout << "  Binary size: " << binSize << " bytes" << std::endl;
    std::cout << "  JSON size:   " << jsonStr.size() << " bytes" << std::endl;
    std::cout << "  Saved: " << std::fixed << std::setprecision(1)
              << (1.0 - (double)binSize / jsonStr.size()) * 100 << "%" << std::endl;
    std::cout << "  Note: Binary format is ideal for bandwidth-constrained IoT scenarios" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 3：SPOI 查询演示（模拟） ----
    std::cout << "[Phase 3] SPOI in-memory query (filter anomalies)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  Query: temperature > 30C AND humidity > 60%" << std::endl;
    std::cout << "  Results:" << std::endl;
    for (auto& t : gateway.temperatures) {
        if (t.celsius > 30.0 && t.humidity > 60.0) {
            std::cout << "    " << t.sensorId << " @ " << t.location
                      << ": " << std::fixed << std::setprecision(1)
                      << t.celsius << "C, " << t.humidity << "%" << std::endl;
        }
    }
    std::cout << "  Note: SPOI query = filter(celsius>30) + filter(humidity>60)" << std::endl;
    std::cout << "        Binary instruction sent to C++ gateway, queries memory directly" << std::endl;
    std::cout << "        No need to transmit full dataset. Python/TS/Go can all query." << std::endl;
    std::cout << std::endl;

    // ---- 阶段 4：动态 Schema 演示 ----
    std::cout << "[Phase 4] Dynamic Schema (new sensor type adaptation)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  Scenario: A new vibration sensor is installed." << std::endl;
    std::cout << "  Developer only needs to define in C++:" << std::endl;
    std::cout << std::endl;
    std::cout << "    struct VibrationReading : public Base {" << std::endl;
    std::cout << "        X__(f64, acceleration, 0.0)" << std::endl;
    std::cout << "        X__(f64, frequency, 0.0)" << std::endl;
    std::cout << "        UseData(VibrationReading);" << std::endl;
    std::cout << "    };" << std::endl;
    std::cout << std::endl;
    std::cout << "  Then: recompile extractor -> run sp-gen -> all client languages" << std::endl;
    std::cout << "        automatically get the new type. No manual client code needed." << std::endl;
    std::cout << "  (During development, Dynamic Schema mode sends JSON at runtime)" << std::endl;
    std::cout << std::endl;

    // ---- 阶段 5：JSON 输出样例 ----
    std::cout << "[Phase 5] JSON output sample" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::stringstream sampleSS;
    gateway.temperatures[0].toJsonStream(sampleSS);
    std::cout << "  " << sampleSS.str() << std::endl;
    std::cout << std::endl;

    // ---- 总结 ----
    std::cout << "======================================================" << std::endl;
    std::cout << "  Summary" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << "  1. Binary serialization: much smaller than JSON for IoT bandwidth" << std::endl;
    std::cout << "  2. SPOI query: query memory directly, no full data transmission" << std::endl;
    std::cout << "  3. Dynamic Schema: new sensor types adapt without recompiling clients" << std::endl;
    std::cout << "  4. Cross-language: Python analytics scripts get types via sp-gen" << std::endl;
    std::cout << "  5. ORM companion: historical data to SQLite, complex queries via SQL" << std::endl;
    std::cout << std::endl;

    return 0;
}