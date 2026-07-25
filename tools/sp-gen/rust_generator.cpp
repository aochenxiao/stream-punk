#include "00-demo-types/Data.hpp"
#include "RustMemberInfo.hpp"
#include "RustMemberInfoImpl.hpp"
#include "RustGenClassCode.hpp"
#include "stream-punk/MetaData.hpp"
#include "meta-reader-rust.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

int generate_rust(const std::string& output_path) {
    std::ofstream outfile(output_path);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return -1;
    }

    namespace fs = std::filesystem;
    auto srcDir = fs::path(__FILE__).parent_path();
    auto runtimePath = (srcDir / "stream-punk.rs").string();
    std::ifstream input(runtimePath);
    if (input.is_open()) {
        outfile << input.rdbuf();
        input.close();
    }
    else {
        std::cerr << "Warning: Could not open stream-punk.rs at " << runtimePath << std::endl;
        return -1;
    }

    outfile << "\n\n/* =================== 类型枚举 =================== */\n\n";
    outfile << "pub mod E_StreamPunkType {\n";

#define X_outPutEnumMember(type, name, ...) outfile << "    pub const " << #name << ": u32 = " << static_cast<int>(E_type::name) << ";\n";

    Xt_Type(X_outPutEnumMember);

# undef X_outPutEnumMember

    outfile << "}\n\n";

    outfile << "/* =================== 类型分发函数 =================== */\n\n";

    outfile << "pub fn read_obj(i: &mut I) -> Option<Box<dyn SpBase>> {\n";
    outfile << "    let id = i.read_u32();\n";
    outfile << "    match id {\n";

#define X_case_type(typeName__, shortName__) \
    outfile << "        E_StreamPunkType::" #shortName__ " => {\n"; \
    outfile << "            let mut obj = Box::new(" #shortName__ "::default());\n"; \
    outfile << "            obj.from_(i);\n"; \
    outfile << "            Some(obj)\n"; \
    outfile << "        }\n";

    Xt_CustomType(X_case_type);

# undef X_case_type

    outfile << "        _ => None,\n";
    outfile << "    }\n";
    outfile << "}\n\n";

    outfile << "pub fn write_obj(o: &mut O, obj: &dyn SpBase) {\n";
    outfile << "    o.write_u32(obj.type_id());\n";
    outfile << "    obj.to(o);\n";
    outfile << "}\n\n";

    outfile << "/* =================== 自定义类型 =================== */\n\n";

#define X_outputClassCode(typeName__, ...) outfile << genRustClassCode<typeName__>();

    Xt_CustomType(X_outputClassCode)

# undef X_outputClassCode

    outfile << "/* =================== 生成代码结束 =================== */\n";

    outfile.close();
    std::cout << "Rust code generated to " << output_path << std::endl;
    return 0;
}

int generate_rust_meta(const std::string& output_path, const std::string& meta_path) {
    // 读取元数据
    sp_meta::MetaFile meta;
    try {
        meta = sp_meta::readMetaFile(meta_path);
    } catch (const std::exception& e) {
        std::cerr << "Error reading metadata: " << e.what() << std::endl;
        return 1;
    }

    // 构建 typeID → TypeMeta* 映射
    std::map<uint32_t, const sp_meta::TypeMeta*> typeMap;
    for (auto& t : meta.types) {
        typeMap[t.typeID] = &t;
    }

    std::ofstream outfile(output_path);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return 1;
    }

    // 输出运行时代码
    namespace fs = std::filesystem;
    auto srcDir = fs::path(__FILE__).parent_path();
    auto runtimePath = (srcDir / "stream-punk.rs").string();
    std::ifstream input(runtimePath);
    if (input.is_open()) {
        outfile << input.rdbuf();
        input.close();
    } else {
        std::cerr << "Warning: Could not open stream-punk.rs at " << runtimePath << std::endl;
        return -1;
    }

    // 杈撳嚭 E_StreamPunkType 鏋氫妇
    outfile << "\n\n/* =================== 类型枚举 =================== */\n\n";
    outfile << "pub mod E_StreamPunkType {\n";
    for (auto& t : meta.types) {
        outfile << "    pub const " << t.className << ": u32 = " << t.typeID << ";\n";
    }
    outfile << "}\n\n";

    // 类型分发函数
    outfile << "/* =================== 类型分发函数 =================== */\n\n";
    outfile << "pub fn read_obj(i: &mut I) -> Option<Box<dyn SpBase>> {\n";
    outfile << "    let id = i.read_u32();\n";
    outfile << "    match id {\n";
    for (auto& t : meta.types) {
        outfile << "        E_StreamPunkType::" << t.className << " => {\n";
        outfile << "            let mut obj = Box::new(" << t.className << "::default());\n";
        outfile << "            obj.from_(i);\n";
        outfile << "            Some(obj)\n";
        outfile << "        }\n";
    }
    outfile << "        _ => None,\n";
    outfile << "    }\n";
    outfile << "}\n\n";

    outfile << "pub fn write_obj(o: &mut O, obj: &dyn SpBase) {\n";
    outfile << "    o.write_u32(obj.type_id());\n";
    outfile << "    obj.to(o);\n";
    outfile << "}\n\n";

    // 生成每个类型的类代码
    outfile << "/* =================== 自定义类型 =================== */\n\n";
    for (auto& t : meta.types) {
        outfile << genRustClassCodeFromMeta(t, typeMap);
    }

    outfile << "/* =================== 生成代码结束 =================== */\n";
    outfile.close();

    std::cout << "StreamPunk Rust code generated (meta): " << output_path << " (" << meta.types.size() << " types)" << std::endl;
    return 0;
}