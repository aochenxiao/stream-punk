/*
    spoi_ts_exec_generator.cpp — TypeScript SPOI Executor 注册表生成器

    从二进制元数据生成 TypeScript 类型注册表，供 spoi_executor.ts 使用。
    输出内容：TYPE_REGISTRY 常量，映射类型名到字段名列表。
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

std::string generateTsSpoiExec(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI Executor 类型注册表（自动生成，请勿手动修改）\n";
    ss << "// 由 sp-gen spoi-ts-exec 从 C++ 元数据生成\n";
    ss << "// ============================================================\n\n";
    ss << "// 格式: { TypeName: [field1, field2, ...] }\n";
    ss << "// 字段顺序与 UseData 宏中声明顺序一致\n\n";
    ss << "import { TypeRegistry } from './spoi_executor';\n\n";
    ss << "export const TYPE_REGISTRY: TypeRegistry = {\n";

    for (size_t i = 0; i < types.size(); ++i) {
        auto& t = types[i];
        ss << "  " << t.className << ": [";
        for (size_t j = 0; j < t.fields.size(); ++j) {
            ss << "\"" << t.fields[j].name << "\"";
            if (j + 1 < t.fields.size()) ss << ", ";
        }
        ss << "]";
        if (i + 1 < types.size()) ss << ",";
        ss << "\n";
    }

    ss << "};\n";

    return ss.str();
}

int generate_spoi_ts_exec(const std::string& output_path, const std::string& meta_path) {
    std::cout << "Reading binary metadata from: " << meta_path << std::endl;
    auto meta = sp_meta::readMetaFile(meta_path);
    auto types = extractSpoiTypes(meta);
    std::cout << "Extracted " << types.size() << " types" << std::endl;

    auto code = generateTsSpoiExec(types);

    std::ofstream ofs(output_path);
    if (!ofs) {
        std::cerr << "Error: Cannot open output file: " << output_path << std::endl;
        return 1;
    }
    ofs << code;
    ofs.close();

    return 0;
}