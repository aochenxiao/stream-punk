#include "00-demo-types/Data.hpp"
#include "generators.hpp"
#include "stream-punk/MetaData.hpp"
#include "meta-reader-kotlin.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

int generate_kotlin_meta(const std::string& output_path, const std::string& meta_path) {
    // 1. 读取元数据
    sp_meta::MetaFile meta;
    try {
        meta = sp_meta::readMetaFile(meta_path);
    } catch (const std::exception& e) {
        std::cerr << "[Kotlin Meta] Failed to read meta file: " << meta_path << " - " << e.what() << std::endl;
        return 1;
    }
    auto& types = meta.types;
    std::cout << "[Kotlin Meta] Read " << types.size() << " types from " << meta_path << std::endl;

    // 构建 typeMap
    std::map<uint32_t, const sp_meta::TypeMeta*> typeMap;
    for (auto& t : types) {
        typeMap[t.typeID] = &t;
    }

    std::stringstream outfile;

    // 2. 拷贝运行时库
    {
        std::ifstream runtime("stream-punk.kt");
        if (runtime.is_open()) {
            outfile << runtime.rdbuf();
            outfile << "\n\n/* =================== 自动生成的类型代码 =================== */\n\n";
        } else {
            std::cerr << "[Kotlin Meta] Warning: stream-punk.kt not found, runtime code not included" << std::endl;
        }
    }

    // 3. 生成类型枚举
    outfile << "// 类型枚举\n";
    outfile << "object E_StreamPunkType {\n";
    outfile << "    const val Base = 0\n";
    for (auto& t : types) {
        outfile << "    const val " << t.className << " = " << t.typeID << "\n";
    }
    outfile << "}\n\n";

    // 4. 生成类型工厂
    outfile << "// 类型工厂\n";
    outfile << "private val __typeFactory = mapOf<Int, () -> Base>(\n";
    for (auto& t : types) {
        outfile << "    E_StreamPunkType." << t.className << " to { " << t.className << "() },\n";
    }
    outfile << ")\n\n";

    // 5. 生成类代码
    for (auto& t : types) {
        outfile << genKotlinClassCodeFromMeta(t, typeMap);
    }

    // 6. 写入输出文件
    auto outPath = output_path + "/stream-punk-data.kt";
    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        std::cerr << "[Kotlin Meta] Failed to open output file: " << outPath << std::endl;
        return 1;
    }
    ofs << outfile.str();
    std::cout << "[Kotlin Meta] Generated: " << outPath << std::endl;

    return 0;
}