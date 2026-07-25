#include "00-demo-types/Data.hpp"
#include "TsMemberInfo.hpp"
#include "MemberInfoBase.hpp"
#include "stream-punk/MetaData.hpp"
#include "meta-reader.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ranges>
#include <type_traits>
#include <map>

namespace fs = std::filesystem;

using Sz = unsigned int;

template<typename T> MemberInfo getMemberInfo() { return getMemberInfo_V2<T>(); }

template<typename T, size_t... Is>
constexpr auto buildMemberInfoArray_V2(std::index_sequence<Is...>) {
    using Extractor = type_sequence_extractor<T>;
    return std::array<MemberInfo, sizeof...(Is)>{ getMemberInfo_V2<typename Extractor::template element<Is>>()... };
}

template<typename T> inline MemberInfo getMemberInfo_V2() {
    if constexpr (TypeID_t<T>::kind == E_type::Base) {
        return typeToTs[TypeID_t<T>::id];
    }
    if constexpr (TypeID_t<T>::id == E_type::Base) {
        MemberInfo r;
        r.tname = "Base";
        r.ivalue = "";
        r.deserializeCode = "readObj(i)";
        r.serializeCode = "writeObj(o, {{_}})";
        return r;
    }
    if constexpr (TypeID_t<T>::kind == E_type::e_customType && TypeID_t<T>::id != E_type::Base) {
        MemberInfo r;
        r.tname = T::_className;
        r.ivalue = "new " + r.tname + "()";
        r.deserializeCode = "new " + r.tname + "().from(i)";
        r.serializeCode = "writeObj(o, {{_}})";
        return r;
    }
    if constexpr (specialization_of<T, std::basic_string>) {
        if constexpr (std::is_same_v<typename T::value_type, wchar_t>) {
            if constexpr (sizeof(wchar_t) == 2) return u16StrInfo;
            else if constexpr (sizeof(wchar_t) == 4) return u32StrInfo;
            else return nullInfo;
        }
        if constexpr (std::is_same_v<typename T::value_type, char>)     return strInfo;
        if constexpr (std::is_same_v<typename T::value_type, char8_t>)  return u8StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char16_t>) return u16StrInfo;
        if constexpr (std::is_same_v<typename T::value_type, char32_t>) return u32StrInfo;
        return nullInfo;
    }
    if constexpr (specialization_of_any<T, std::vector, std::deque, std::list, std::forward_list>) {
        MemberInfo r;
        auto vInfo = getMemberInfo_V2<typename T::value_type>();
        r.tname = "Array<" + vInfo.tname + ">";
        r.ivalue = "[]";
        r.deserializeCode = "i.readArray(() => " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.writeArray({{_}}, (v) => " + ser + ")";
        return r;
    }
    if constexpr (specialization_of_any<T, std::set, std::unordered_set>) {
        MemberInfo r;
        auto vInfo = getMemberInfo_V2<typename T::value_type>();
        r.tname = "Set<" + vInfo.tname + ">";
        r.ivalue = "new Set()";
        r.deserializeCode = "i.readSet(() => " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.writeSet({{_}}, (v) => " + ser + ")";
        return r;
    }
    if constexpr (specialization_of_any<T, std::map, std::unordered_map>) {
        MemberInfo r;
        auto kInfo = getMemberInfo_V2<typename T::key_type>();
        auto vInfo = getMemberInfo_V2<typename T::mapped_type>();
        r.tname = "Map<" + kInfo.tname + ", " + vInfo.tname + ">";
        r.ivalue = "new Map()";
        r.deserializeCode = "i.readMap(() => " + kInfo.deserializeCode + ", () => " + vInfo.deserializeCode + ")";
        auto kSer = kInfo.serializeCode;
        auto vSer = vInfo.serializeCode;
        replaceAll(kSer, VAL, "k");
        replaceAll(vSer, VAL, "v");
        r.serializeCode = "o.writeMap({{_}}, (k) => " + kSer + ", (v) => " + vSer + ")";
        return r;
    }
    if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_pointer_t<T>;
        MemberInfo r;
        auto vInfo = getMemberInfo_V2<type>();
        r.tname = "SpRef<" + vInfo.tname + " | null>";
        r.ivalue = "new SpRef(null)";
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.deserializeCode = "i.readPtrWithTypeID((i) => readObj(i) as " + vInfo.tname + " | null)";
            r.serializeCode = "o.writePtrWithTypeID({{_}}.value, {{_}}.address, (o, v) => writeObj(o, v))";
        } else {
            r.deserializeCode = "i.readPtr(() => " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.writePtr({{_}}.value, {{_}}.address, (v) => " + ser + ")";
        }
        return r;
    }
    if constexpr (specialization_of_any<T, std::shared_ptr, std::weak_ptr, std::unique_ptr>) {
        using type = typename T::element_type;
        MemberInfo r;
        auto vInfo = getMemberInfo_V2<type>();
        r.tname = "SpRef<" + vInfo.tname + " | null>";
        r.ivalue = "new SpRef(null)";
        if constexpr (TypeID_t<type>::kind == E_type::e_customType || std::is_same_v<type, Base>) {
            r.deserializeCode = "i.readPtrWithTypeID((i) => readObj(i) as " + vInfo.tname + " | null)";
            r.serializeCode = "o.writePtrWithTypeID({{_}}.value, {{_}}.address, (o, v) => writeObj(o, v))";
        } else {
            r.deserializeCode = "i.readPtr(() => " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.writePtr({{_}}.value, {{_}}.address, (v) => " + ser + ")";
        }
        return r;
    }
    if constexpr (is_std_bitset_v<T>) {
        MemberInfo r;
        r.tname = "Array<boolean>";
        r.ivalue = "[]";
        r.deserializeCode = "i.readBitset()";
        r.serializeCode = "o.writeBitset({{_}})";
        return r;
    }
    if constexpr (specialization_of<T, std::optional>) {
        MemberInfo r;
        auto vInfo = getMemberInfo_V2<typename T::value_type>();
        r.tname = vInfo.tname + " | null";
        r.ivalue = "null";
        r.deserializeCode = "i.readOptional(() => " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.writeOptional({{_}}, (v) => { " + ser + "; })";
        return r;
    }
    if constexpr (specialization_of<T, std::atomic>) {
        return getMemberInfo_V2<typename T::value_type>();
    }
    if constexpr (std::is_same_v<T, std::filesystem::path>) {
        MemberInfo r;
        r.tname = "string";
        r.ivalue = "\"\"";
        r.deserializeCode = "i.readString()";
        r.serializeCode = "o.writeString({{_}})";
        return r;
    }
    if constexpr (specialization_of<T, std::tuple>) {
        MemberInfo r;
        r.tname = "[";
        r.ivalue = "[";
        r.deserializeCode = "[";
        r.serializeCode = "";
        auto arr = buildMemberInfoArray_V2<T>(std::make_index_sequence<std::tuple_size_v<T>>{});
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i != 0) {
                r.tname += ", ";
                r.ivalue += ", ";
                r.deserializeCode += ", ";
            }
            r.tname += arr[i].tname;
            r.ivalue += arr[i].ivalue;
            r.deserializeCode += arr[i].deserializeCode;
            auto ser = arr[i].serializeCode;
            replaceAll(ser, VAL, std::string("{{_}}[") + std::to_string(i) + "]");
            r.serializeCode += ser + "; ";
        }
        r.tname += "]";
        r.ivalue += "]";
        r.deserializeCode += "]";
        return r;
    }
    if constexpr (specialization_of<T, std::variant>) {
        MemberInfo r;
        auto arr = buildMemberInfoArray_V2<T>(std::make_index_sequence<std::variant_size_v<T>>{});
        r.tname = "SpVariant<[";
        r.ivalue = "new SpVariant(" + arr[0].ivalue + ", 0)";
        r.deserializeCode = "i.readVariant([";
        r.serializeCode = "o.writeVariant({{_}}.value, {{_}}.typeIndex, [";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i != 0) {
                r.tname += " | ";
                r.deserializeCode += ", ";
                r.serializeCode += ", ";
            }
            r.tname += arr[i].tname;
            r.deserializeCode += "() => " + arr[i].deserializeCode;
            auto ser = arr[i].serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode += "(v: any) => " + ser;
        }
        r.tname += "]>";
        r.deserializeCode += "])";
        r.serializeCode += "])";
        return r;
    }
    if constexpr (is_std_array_v<T>) {
        MemberInfo r;
        using type = typename T::value_type;
        auto vInfo = getMemberInfo_V2<type>();
        auto szStr = std::to_string(std::tuple_size_v<T>);
        r.tname = "SpArray<" + vInfo.tname + ">";
        r.ivalue = "new SpArray(" + szStr + ")";
        r.deserializeCode = "i.readSpArray(" + szStr + ", () => " + vInfo.deserializeCode + ")";
        auto ser = vInfo.serializeCode;
        replaceAll(ser, VAL, "v");
        r.serializeCode = "o.writeSpArray({{_}}, (v) => " + ser + ")";
        return r;
    }
    if constexpr (
        specialization_of_any<T, std::chrono::duration, std::chrono::time_point>
        || std::is_same_v<T, std::chrono::system_clock::time_point>
        || std::is_same_v<T, std::chrono::steady_clock::time_point>
    ) {
        MemberInfo r;
        r.tname = "Date";
        r.ivalue = "new Date(0)";
        r.deserializeCode = "i.readTime()";
        r.serializeCode = "o.writeTime({{_}})";
        return r;
    }
    return nullInfo;
}

template<typename CT> std::string genClassCode_V2() {
    using TupleType = typename CT::M::TypeList;
    constexpr size_t tupleSize = std::tuple_size<TupleType>::value;
    std::stringstream ss;

    auto typeStrs = []<size_t... Is>(std::index_sequence<Is...>) {
        return std::vector<MemberInfo>{getMemberInfo_V2<std::tuple_element_t<Is, TupleType>>()...};
    }(std::make_index_sequence<tupleSize>{});
    auto sz = std::size(CT::_membersName);

    ss << "export class " << CT::_className << " extends " << CT::_baseName << " {\n";
    ss << "  static readonly typeID = E_StreamPunkType." << CT::_className << ";\n";

    for (size_t i = 0; i < sz; ++i) {
        auto& info = typeStrs[i];
        ss << "  " << CT::_membersName[i] << ": " << info.tname << " = " << info.ivalue << ";\n";
    }

    ss << "\n  from(i: I): this {\n";
    ss << "    super.from(i);\n";
    for (size_t i = 0; i < sz; ++i) {
        auto& info = typeStrs[i];
        ss << "    this." << CT::_membersName[i] << " = " << info.deserializeCode << ";\n";
    }
    ss << "    return this;\n";
    ss << "  }\n";

    ss << "\n  to(o: O): this {\n";
    ss << "    super.to(o);\n";
    for (size_t i = 0; i < sz; ++i) {
        auto code = typeStrs[i].serializeCode;
        replaceAll(code, VAL, std::string("this.") + CT::_membersName[i]);
        ss << "    " << code << ";\n";
    }
    ss << "    return this;\n";
    ss << "  }\n";

    ss << "}\n";
    return ss.str();
}

// 从 runtimes/ts/stream-punk-gen.ts 读取运行时库代码
static std::string readTSRuntime(const std::string& runtimes_dir) {
    fs::path runtimePath;
    if (!runtimes_dir.empty()) {
        runtimePath = fs::path(runtimes_dir) / "ts" / "stream-punk-gen.ts";
    } else {
        // 回退：基于源码树位置计算
        runtimePath = fs::path(__FILE__).parent_path().parent_path().parent_path() / "runtimes" / "ts" / "stream-punk-gen.ts";
    }

    std::ifstream f(runtimePath);
    if (!f.is_open()) {
        std::cerr << "Warning: Could not read TS runtime from " << runtimePath.string() << std::endl;
        return "";
    }
    std::stringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

// 不嵌入运行时时，将运行时文件复制到输出目录旁
static bool copyTSRuntime(const std::string& output_path, const std::string& runtimes_dir) {
    fs::path src;
    if (!runtimes_dir.empty()) {
        src = fs::path(runtimes_dir) / "ts" / "stream-punk-gen.ts";
    } else {
        src = fs::path(__FILE__).parent_path().parent_path().parent_path() / "runtimes" / "ts" / "stream-punk-gen.ts";
    }
    if (src.empty() || !fs::exists(src)) {
        std::cerr << "Error: Could not locate TS runtime to copy: " << src.string() << std::endl;
        return false;
    }

    fs::path dst = fs::path(output_path).parent_path() / "stream-punk.ts";
    try {
        fs::create_directories(dst.parent_path());
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        std::cout << "Copied TS runtime to " << dst.string() << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error copying TS runtime: " << e.what() << std::endl;
        return false;
    }
}

static const char* tsRuntimeImport = R"(import { I, O, Base, SpArray, SpRef, SpVariant } from './stream-punk';
import type { SpStdArray } from './stream-punk';

)";

static const char* tsRuntimeReexport = R"(
export { I, O, Base, SpArray, SpRef, SpVariant } from './stream-punk';
export type { SpStdArray } from './stream-punk';
)";

int generate_ts_2(const std::string& output_path, const std::string& runtimes_dir, bool no_embed_runtime) {
    std::ofstream outfile(output_path);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return 1;
    }

    if (no_embed_runtime) {
        if (!copyTSRuntime(output_path, runtimes_dir)) {
            outfile.close();
            return 1;
        }
        outfile << tsRuntimeImport;
    } else {
        outfile << readTSRuntime(runtimes_dir);
    }

    outfile << "export enum E_StreamPunkType {\n";
# define X_outPutEnumMember(type, name, ...) outfile << "  " << #name << " = " << static_cast<int>(E_type::name) << ",\n";
    Xt_Type(X_outPutEnumMember);
    outfile << "}\n";

    if (no_embed_runtime) {
        outfile << "\nconst __typeFactory = new Map<number, () => Base>();\n";
    } else {
        outfile << R"(
export class Base {
  static readonly typeID: E_StreamPunkType = E_StreamPunkType.Base;
  from(i: I): this { void i; return this; }
  to(o: O): this  { void o; return this; }
}

const __typeFactory = new Map<number, () => Base>();
)";
    }

# define X_reg_factory(typeName__, ...) outfile << "__typeFactory.set(E_StreamPunkType." #typeName__ ", () => new " #typeName__ "());\n";
    Xt_CustomType(X_reg_factory);

    outfile << R"(
export function readObj(i: I): Base | null {
  const id = i.readU32();
  const factory = __typeFactory.get(id);
  if (!factory) return null;
  const obj = factory();
  obj.from(i);
  return obj;
}

export function writeObj(o: O, obj: Base): void {
  o.writeU32((obj.constructor as typeof Base).typeID);
  obj.to(o);
}

)";

# define X_outputClassCode(typeName__, ...) outfile << genClassCode_V2<typeName__>();
    Xt_CustomType(X_outputClassCode);

    if (no_embed_runtime) {
        outfile << tsRuntimeReexport;
    }

    outfile << "\n";
    outfile.close();

    std::cout << "StreamPunk v2 TS code generated: " << output_path << std::endl;
    return 0;
}

// ============== 基于元数据的 TS 生成器 ==============

int generate_ts_meta(const std::string& output_path, const std::string& meta_path, bool no_embed_runtime) {
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
    if (no_embed_runtime) {
        if (!copyTSRuntime(output_path, "")) {
            outfile.close();
            return 1;
        }
        outfile << tsRuntimeImport;
    } else {
        outfile << readTSRuntime("");  // meta 生成器使用默认路径
    }

    // 输出 E_StreamPunkType 枚举
    outfile << "export enum E_StreamPunkType {\n";
    outfile << "  Base = " << static_cast<int>(E_type::Base) << ",\n";
    for (auto& t : meta.types) {
        outfile << "  " << t.className << " = " << t.typeID << ",\n";
    }
    outfile << "}\n";

    // Base 类和工厂注册
    if (no_embed_runtime) {
        outfile << "\nconst __typeFactory = new Map<number, () => Base>();\n";
    } else {
        outfile << R"(
export class Base {
  static readonly typeID: E_StreamPunkType = E_StreamPunkType.Base;
  from(i: I): this { void i; return this; }
  to(o: O): this  { void o; return this; }
}

const __typeFactory = new Map<number, () => Base>();
)";
    }

    for (auto& t : meta.types) {
        outfile << "__typeFactory.set(E_StreamPunkType." << t.className << ", () => new " << t.className << "());\n";
    }

    outfile << R"(
export function readObj(i: I): Base | null {
  const id = i.readU32();
  const factory = __typeFactory.get(id);
  if (!factory) return null;
  const obj = factory();
  obj.from(i);
  return obj;
}

export function writeObj(o: O, obj: Base): void {
  o.writeU32((obj.constructor as typeof Base).typeID);
  obj.to(o);
}

)";

    // 生成每个类型的类代码
    for (auto& t : meta.types) {
        outfile << genTsClassCodeFromMeta(t, typeMap);
    }

    if (no_embed_runtime) {
        outfile << tsRuntimeReexport;
    }

    outfile << "\n";
    outfile.close();

    std::cout << "StreamPunk TS code generated (meta): " << output_path << " (" << meta.types.size() << " types)" << std::endl;
    return 0;
}