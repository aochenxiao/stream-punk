#include "00-demo-types/Data.hpp"
#include "GoMemberInfo.hpp"
#include "GoMemberInfoImpl.hpp"
#include "GoGenClassCode.hpp"
#include "stream-punk/MetaData.hpp"
#include "meta-reader-go.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

int generate_go(const std::string& output_path) {
    std::ofstream outfile(output_path);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return -1;
    }

    namespace fs = std::filesystem;
    auto srcDir = fs::path(__FILE__).parent_path();
    auto runtimePath = (srcDir / "stream-punk.go").string();
    std::ifstream input(runtimePath);
    if (input.is_open()) {
        outfile << input.rdbuf();
        input.close();
    }
    else {
        std::cerr << "Warning: Could not open stream-punk.go at " << runtimePath << std::endl;
        return -1;
    }

    outfile << "\n\n/* =================== 类型枚举 =================== */\n\n";

#define X_outPutEnumMember(type, name, ...) outfile << "const E_StreamPunkType_" << #name << " uint32 = " << static_cast<int>(E_type::name) << "\n";

    Xt_Type(X_outPutEnumMember);

# undef X_outPutEnumMember

    outfile << "\n/* =================== 类型分发 =================== */\n\n";

    outfile << "func ReadObj(i *I) SpBase {\n";
    outfile << "    id := i.ReadU32()\n";
    outfile << "    switch id {\n";

#define X_case_type(typeName__, shortName__) \
    outfile << "    case E_StreamPunkType_" #shortName__ ":\n"; \
    outfile << "        obj := &" #shortName__ "{}\n"; \
    outfile << "        obj.From_(i)\n"; \
    outfile << "        return obj\n";

    Xt_CustomType(X_case_type);

# undef X_case_type

    outfile << "    default:\n";
    outfile << "        return nil\n";
    outfile << "    }\n";
    outfile << "}\n\n";

    outfile << "func WriteObj(o *O, obj SpBase) {\n";
    outfile << "    o.WriteU32(obj.TypeID())\n";
    outfile << "    obj.To(o)\n";
    outfile << "}\n\n";

    outfile << "/* =================== 自定义类型 =================== */\n\n";

#define X_outputClassCode(typeName__, ...) outfile << genGoClassCode<typeName__>();

    Xt_CustomType(X_outputClassCode)

# undef X_outputClassCode

    outfile << "/* =================== 生成代码结束 =================== */\n";

    outfile.close();
    std::cout << "Go code generated to " << output_path << std::endl;
    return 0;
}

int generate_go_meta(const std::string& output_path, const std::string& meta_path) {
    sp_meta::MetaFile meta;
    try {
        meta = sp_meta::readMetaFile(meta_path);
    } catch (const std::exception& e) {
        std::cerr << "Error reading metadata: " << e.what() << std::endl;
        return 1;
    }

    std::map<uint32_t, const sp_meta::TypeMeta*> typeMap;
    for (auto& t : meta.types) {
        typeMap[t.typeID] = &t;
    }

    std::ofstream outfile(output_path);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return 1;
    }

    namespace fs = std::filesystem;
    auto srcDir = fs::path(__FILE__).parent_path();
    auto runtimePath = (srcDir / "stream-punk.go").string();
    std::ifstream input(runtimePath);
    if (input.is_open()) {
        outfile << input.rdbuf();
        input.close();
    } else {
        std::cerr << "Warning: Could not open stream-punk.go at " << runtimePath << std::endl;
        return -1;
    }

    outfile << "\n\n/* =================== 类型枚举 =================== */\n\n";
    for (auto& t : meta.types) {
        outfile << "const E_StreamPunkType_" << t.className << " uint32 = " << t.typeID << "\n";
    }

    outfile << "\n/* =================== 类型分发 =================== */\n\n";
    outfile << "func ReadObj(i *I) SpBase {\n";
    outfile << "    id := i.ReadU32()\n";
    outfile << "    switch id {\n";
    for (auto& t : meta.types) {
        outfile << "    case E_StreamPunkType_" << t.className << ":\n";
        outfile << "        obj := &" << t.className << "{}\n";
        outfile << "        obj.From_(i)\n";
        outfile << "        return obj\n";
    }
    outfile << "    default:\n";
    outfile << "        return nil\n";
    outfile << "    }\n";
    outfile << "}\n\n";

    outfile << "func WriteObj(o *O, obj SpBase) {\n";
    outfile << "    o.WriteU32(obj.TypeID())\n";
    outfile << "    obj.To(o)\n";
    outfile << "}\n\n";

    outfile << "/* =================== 自定义类型 =================== */\n\n";
    for (auto& t : meta.types) {
        outfile << genGoClassCodeFromMeta(t, typeMap);
    }

    outfile << "/* =================== 生成代码结束 =================== */\n";
    outfile.close();

    std::cout << "StreamPunk Go code generated (meta): " << output_path << " (" << meta.types.size() << " types)" << std::endl;
    return 0;
}