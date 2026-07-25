#include "00-demo-types/Data.hpp"
#include "JavaMemberInfo.hpp"
#include "JavaMemberInfoImpl.hpp"
#include "JavaGenClassCode.hpp"
#include "stream-punk/MetaData.hpp"
#include "meta-reader-java.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

static std::string findJavaRuntime(const std::string& runtimes_dir) {
    if (!runtimes_dir.empty()) {
        return (fs::path(runtimes_dir) / "java" / "stream-punk.java").string();
    }
    return (fs::path(__FILE__).parent_path().parent_path().parent_path() / "runtimes" / "java" / "stream-punk.java").string();
}

int generate_java(const std::string& output_path, const std::string& runtimes_dir) {
    std::ofstream outfile(output_path);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return -1;
    }

    auto runtimePath = findJavaRuntime(runtimes_dir);
    std::ifstream input(runtimePath);
    if (input.is_open()) {
        outfile << input.rdbuf();
        input.close();
    }
    else {
        std::cerr << "Warning: Could not open stream-punk.java at " << runtimePath << std::endl;
        return -1;
    }

    outfile << "\n\n/* =================== 类型枚举 =================== */\n\n";
    outfile << "class E_StreamPunkType {\n";

#define X_outPutEnumMember(type, name, ...) outfile << "    public static final int " << #name << " = " << static_cast<int>(E_type::name) << ";\n";

    Xt_Type(X_outPutEnumMember);

# undef X_outPutEnumMember

    outfile << "}\n";
    outfile << "\n";
    outfile << "/* =================== Base 基类 =================== */\n";
    outfile << R"(
class Base {
    public int typeID = E_StreamPunkType.Base;

    public Base() {}

    public Base from_(I i) {
        return this;
    }

    public void to(O o) {
    }
)";

    outfile << "\n";
    outfile << "    public static Base read_obj(I i) {\n";
    outfile << "        long id_ = i.read_u32();\n";
    bool first = true;

#define X_case_type(typeName__, shortName__) \
    if (!first) outfile << "        else "; else { first = false; } \
    outfile << "if (id_ == E_StreamPunkType." #shortName__ ") {\n"; \
    outfile << "            " #shortName__ " obj = new " #shortName__ "();\n"; \
    outfile << "            obj.from_(i);\n"; \
    outfile << "            return obj;\n"; \
    outfile << "        }\n";

    Xt_CustomType(X_case_type);

# undef X_case_type

    outfile << "        return null;\n";
    outfile << "    }\n";
    outfile << "\n";
    outfile << "    public static void write_obj(O o, Base obj) {\n";
    outfile << "        o.write_u32(obj.typeID);\n";
    outfile << "        obj.to(o);\n";
    outfile << "    }\n";
    outfile << "}\n";
    outfile << "\n";

    outfile << "/* =================== 自定义类型 =================== */\n\n";

#define X_outputClassCode(typeName__, ...) outfile << genJavaClassCode<typeName__>();

    Xt_CustomType(X_outputClassCode)

# undef X_outputClassCode

    outfile << "/* =================== 生成代码结束 =================== */\n";

    return 0;
}

int generate_java_meta(const std::string& output_path, const std::string& meta_path, const std::string& runtimes_dir) {
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
    auto runtimePath = findJavaRuntime(runtimes_dir);
    std::ifstream input(runtimePath);
    if (input.is_open()) {
        outfile << input.rdbuf();
        input.close();
    } else {
        std::cerr << "Warning: Could not open stream-punk.java at " << runtimePath << std::endl;
        return -1;
    }

    // 输出 E_StreamPunkType 枚举
    outfile << "\n\n/* =================== 类型枚举 =================== */\n\n";
    outfile << "class E_StreamPunkType {\n";
    outfile << "    public static final int Base = " << static_cast<int>(E_type::Base) << ";\n";
    for (auto& t : meta.types) {
        outfile << "    public static final int " << t.className << " = " << t.typeID << ";\n";
    }
    outfile << "}\n";

    // Base 类与工厂方法
    outfile << "\n/* =================== Base 基类 =================== */\n";
    outfile << R"(
class Base {
    public int typeID = E_StreamPunkType.Base;

    public Base() {}

    public Base from_(I i) {
        return this;
    }

    public void to(O o) {
    }

    public static Base read_obj(I i) {
        long id_ = i.read_u32();
)";

    bool first_meta = true;
    for (auto& t : meta.types) {
        if (!first_meta) outfile << "        else ";
        else { first_meta = false; }
        outfile << "if (id_ == E_StreamPunkType." << t.className << ") {\n";
        outfile << "            " << t.className << " obj = new " << t.className << "();\n";
        outfile << "            obj.from_(i);\n";
        outfile << "            return obj;\n";
        outfile << "        }\n";
    }

    outfile << R"(        return null;
    }

    public static void write_obj(O o, Base obj) {
        o.write_u32(obj.typeID);
        obj.to(o);
    }
}
)";

    // 生成每个类型的类代码
    outfile << "\n/* =================== 自定义类型 =================== */\n\n";
    for (auto& t : meta.types) {
        outfile << genJavaClassCodeFromMeta(t, typeMap);
    }

    outfile << "\n/* =================== 生成代码结束 =================== */\n";
    outfile.close();

    std::cout << "StreamPunk Java code generated (meta): " << output_path << " (" << meta.types.size() << " types)" << std::endl;
    return 0;
}