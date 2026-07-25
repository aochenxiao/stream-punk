/*
    spoi_js_exec_generator.cpp — JavaScript SPOI Executor 注册表生成器

    从二进制元数据生成 JavaScript 类型注册表，供 spoi_executor.js 使用。
    输出内容：TYPE_REGISTRY 对象，映射类型名到字段名列表。
*/

#include "spoi_generator.hpp"
#include <sstream>
#include <iostream>

using namespace spoi_gen;

std::string generateJsSpoiExec(const std::vector<SpoiTypeInfo>& types) {
    std::stringstream ss;

    ss << "// ============================================================\n";
    ss << "// SPOI Executor 类型注册表（自动生成，请勿手动修改）\n";
    ss << "// 由 sp-gen spoi-js-exec 从 C++ 元数据生成\n";
    ss << "// ============================================================\n\n";
    ss << "// 格式: { TypeName: [field1, field2, ...] }\n";
    ss << "// 字段顺序与 UseData 宏中声明顺序一致\n\n";
    ss << "// 同时支持 CommonJS 和 ES Module\n";
    ss << "const TYPE_REGISTRY = {\n";

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

    ss << "};\n\n";
    ss << "if (typeof module !== 'undefined' && module.exports) {\n";
    ss << "  module.exports = { TYPE_REGISTRY };\n";
    ss << "}\n";

    return ss.str();
}

int generate_spoi_js_exec(const std::string& output_path, const std::string& meta_path) {
    std::cout << "Reading binary metadata from: " << meta_path << std::endl;
    auto meta = sp_meta::readMetaFile(meta_path);
    auto types = extractSpoiTypes(meta);
    std::cout << "Extracted " << types.size() << " types" << std::endl;

    auto code = generateJsSpoiExec(types);

    std::ofstream ofs(output_path);
    if (!ofs) {
        std::cerr << "Error: Cannot open output file: " << output_path << std::endl;
        return 1;
    }
    ofs << code;
    ofs.close();

    return 0;
}