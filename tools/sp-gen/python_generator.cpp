#include "00-demo-types/Data.hpp"
#include "PyMemberInfo.hpp"
#include "PyMemberInfoImpl.hpp"
#include "PyGenClassCode.hpp"
#include "stream-punk/MetaData.hpp"
#include "meta-reader-py.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

static std::string findPyRuntime(const std::string& runtimes_dir) {
    fs::path runtimePath;
    if (!runtimes_dir.empty()) {
        runtimePath = fs::path(runtimes_dir) / "py" / "stream-punk.py";
    } else {
        runtimePath = fs::path(__FILE__).parent_path().parent_path().parent_path() / "runtimes" / "py" / "stream-punk.py";
    }
    return runtimePath.string();
}

int generate_python(const std::string& output_path, const std::string& runtimes_dir) {
    std::ofstream outfile(output_path);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return -1;
    }

    std::string pyRuntimePath = findPyRuntime(runtimes_dir);
    std::ifstream input(pyRuntimePath);
    if (input.is_open()) {
        outfile << input.rdbuf();
        input.close();
    }
    else {
        std::cerr << "Warning: Could not open stream-punk.py at " << pyRuntimePath << std::endl;
        return -1;
    }

    outfile << "\n\nclass E_StreamPunkType:\n";

#define X_outPutEnumMember(type, name, ...) outfile << "    " << #name << " = " << static_cast<int>(E_type::name) << "\n";

    Xt_Type(X_outPutEnumMember);

    outfile << "\n\n";
    outfile << R"(
class Base:
    typeID = E_StreamPunkType.Base

    def __init__(self):
        pass

    def from_(self, i: I):
        return self

    def to(self, o: O):
        return self


def read_obj(i: I):
    id_ = i.read_u32()
)";
    bool first = true;
#define X_case_type(typeName__, shortName__) \
    if (!first) outfile << "    elif "; else { outfile << "    if "; first = false; } \
    outfile << "id_ == E_StreamPunkType." #shortName__ ":\n"; \
    outfile << "        obj = " #shortName__ "()\n"; \
    outfile << "        obj.from_(i)\n"; \
    outfile << "        return obj\n";
    Xt_CustomType(X_case_type);
    outfile << R"(
    return None


def write_obj(o: O, obj: Base):
    o.write_u32(type(obj).typeID)
    obj.to(o)


)";

#define X_outputClassCode(typeName__, ...) outfile << genPyClassCode<typeName__>();
    Xt_CustomType(X_outputClassCode)

    outfile.close();
    std::cout << "Python code generated to " << output_path << std::endl;
    return 0;
}

int generate_python_meta(const std::string& output_path, const std::string& meta_path) {
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

    std::ifstream input("stream-punk.py");
    if (input.is_open()) {
        outfile << input.rdbuf();
        input.close();
    } else {
        std::cerr << "Warning: Could not open stream-punk.py" << std::endl;
        return 1;
    }

    outfile << "\n\nclass E_StreamPunkType:\n";
    outfile << "    Base = " << static_cast<int>(E_type::Base) << "\n";
    for (auto& t : meta.types) {
        outfile << "    " << t.className << " = " << t.typeID << "\n";
    }
    outfile << "\n\n";

    outfile << R"(
class Base:
    typeID = E_StreamPunkType.Base

    def __init__(self):
        pass

    def from_(self, i: I):
        return self

    def to(self, o: O):
        return self


def read_obj(i: I):
    id_ = i.read_u32()
)";
    bool first_meta = true;
    for (auto& t : meta.types) {
        if (!first_meta) outfile << "    elif ";
        else { outfile << "    if "; first_meta = false; }
        outfile << "id_ == E_StreamPunkType." << t.className << ":\n";
        outfile << "        obj = " << t.className << "()\n";
        outfile << "        obj.from_(i)\n";
        outfile << "        return obj\n";
    }
    outfile << R"(
    return None


def write_obj(o: O, obj: Base):
    o.write_u32(type(obj).typeID)
    obj.to(o)


)";

    for (auto& t : meta.types) {
        outfile << genPyClassCodeFromMeta(t, typeMap);
    }

    outfile.close();
    std::cout << "Python code generated (meta): " << output_path << " (" << meta.types.size() << " types)" << std::endl;
    return 0;
}