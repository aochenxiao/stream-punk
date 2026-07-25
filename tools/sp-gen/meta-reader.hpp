#pragma once

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/MetaData.hpp"
#include "TsMemberInfo.hpp"
#include "TypeDescInterpreter.hpp"
#include <string>
#include <vector>
#include <map>
#include <span>

using namespace sp;

// ============== 运行时 TypeDesc 解释器（TS 版本） ==============

// 从 TypeDesc 的 SpToken 数组中解释出 MemberInfo
// 这是 getTsMemberInfo<T>() 的运行时版本

// 补充 boolInfo（TsMemberInfo.hpp 中未定义）
inline MemberInfo boolInfo { "boolean", "false", "o.writeBl({{_}})",  "i.readBl()" };

// 运行时基础类型映射表（与 typeToTs 一致，但用于运行时 SpToken 查找）
inline std::map<Sz, MemberInfo> basicTypeMap = {
    {E_type::u8  , {"number" , "0"     , "o.writeU8({{_}})" , "i.readU8()"}},
    {E_type::u16 , {"number" , "0"     , "o.writeU16({{_}})", "i.readU16()"}},
    {E_type::u32 , {"number" , "0"     , "o.writeU32({{_}})", "i.readU32()"}},
    {E_type::u64 , {"bigint" , "0n"    , "o.writeU64({{_}})", "i.readU64()"}},
    {E_type::i8  , {"number" , "0"     , "o.writeI8({{_}})" , "i.readI8()"}},
    {E_type::i16 , {"number" , "0"     , "o.writeI16({{_}})", "i.readI16()"}},
    {E_type::i32 , {"number" , "0"     , "o.writeI32({{_}})", "i.readI32()"}},
    {E_type::i64 , {"bigint" , "0n"    , "o.writeI64({{_}})", "i.readI64()"}},
    {E_type::f32 , {"number" , "0.0"   , "o.writeF32({{_}})", "i.readF32()"}},
    {E_type::f64 , {"number" , "0.0"   , "o.writeF64({{_}})", "i.readF64()"}},
    {E_type::bl  , {"boolean","false"  , "o.writeBl({{_}})" , "i.readBl()"}},
    {E_type::ch  , {"string" ,"\"\""   , "o.writeCh({{_}})" , "i.readCh()"}},
    {E_type::ch8 , {"string" ,"\"\""   , "o.writeCh8({{_}})", "i.readCh8()"}},
    {E_type::ch16, {"string" ,"\"\""   , "o.writeCh16({{_}})", "i.readCh16()"}},
    {E_type::ch32, {"string" ,"\"\""   , "o.writeCh32({{_}})", "i.readCh32()"}},
};

inline MemberInfo getBasicTypeInfo(Sz typeId) {
    auto it = basicTypeMap.find(typeId);
    if (it != basicTypeMap.end()) return it->second;
    return nullInfo;
}

inline MemberInfo interpretTsTypeDesc(std::span<const sp_meta::SpToken> desc,
                                       const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap);

// 计算 TypeDesc 的 token 数量（不生成代码，仅用于确定子描述符的边界）
inline size_t getTsTypeDescLength(std::span<const sp_meta::SpToken> desc) {
    return getTypeDescLength(desc);
}

inline MemberInfo interpretTsTypeDescInner(std::span<const sp_meta::SpToken> desc,
                                            const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    if (desc.empty()) return nullInfo;

    auto first = desc[0];

    // ============ 自定义类型（TypeDesc 中存储的是 E_type::name 的枚举值，范围为 [Base+1, e_customType)） ============
    if (first >= static_cast<Sz>(E_type::Base) + 1 && first < static_cast<Sz>(E_type::e_customType)) {
        uint32_t typeID = static_cast<uint32_t>(first);
        auto it = typeMap.find(typeID);
        if (it != typeMap.end()) {
            MemberInfo r;
            r.tname = it->second->className;
            r.ivalue = "new " + r.tname + "()";
            r.deserializeCode = "new " + r.tname + "().from(i)";
            r.serializeCode = "writeObj(o, {{_}})";
            return r;
        }
        return nullInfo;
    }

    // ============ Base 类型 ============
    if (first == E_type::Base) {
        MemberInfo r;
        r.tname = "Base";
        r.ivalue = "";
        r.deserializeCode = "read_obj(i)";
        r.serializeCode = "writeObj(o, {{_}})";
        return r;
    }

    // ============ 指针类型 ============
    if (first == E_type::ptr || first == E_type::sptr || first == E_type::wptr || first == E_type::uptr) {
        auto subDesc = desc.subspan(1);
        auto vInfo = interpretTsTypeDescInner(subDesc, typeMap);

        bool isCustomPtr = (!subDesc.empty() && (subDesc[0] >= static_cast<Sz>(E_type::Base) + 1 && subDesc[0] < static_cast<Sz>(E_type::e_customType)))
                         || (!subDesc.empty() && subDesc[0] == E_type::Base);

        MemberInfo r;
        if (isCustomPtr) {
            r.tname = "SpRef<" + vInfo.tname + " | null>";
            r.ivalue = "new SpRef(null, 0)";
            r.deserializeCode = "i.readPtrWithTypeID()";
            r.serializeCode = "o.writePtrWithTypeID({{_}}.value)";
        } else {
            r.tname = "SpRef<" + vInfo.tname + " | null>";
            r.ivalue = "new SpRef(null, 0)";
            r.deserializeCode = "i.readPtr(() => " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.writePtr({{_}}.value, {{_}}.address, (v) => { " + ser + " })";
        }
        return r;
    }

    // ============ 容器类型 ============

    // vector / deque / list / forward_list → Array
    if (first == E_type::vector || first == E_type::deque || first == E_type::list || first == E_type::flist) {
        if (desc.size() >= 2) {
            auto vInfo = interpretTsTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "Array<" + vInfo.tname + ">";
            r.ivalue = "[]";
            r.deserializeCode = "i.readArray(() => " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.writeArray({{_}}, (v) => { " + ser + " })";
            return r;
        }
    }

    // set / unordered_set → Set
    if (first == E_type::set || first == E_type::uset) {
        if (desc.size() >= 2) {
            auto vInfo = interpretTsTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "Set<" + vInfo.tname + ">";
            r.ivalue = "new Set()";
            r.deserializeCode = "i.readSet(() => " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.writeSet({{_}}, (v) => { " + ser + " })";
            return r;
        }
    }

    // map / unordered_map → Map
    if (first == E_type::map || first == E_type::umap) {
        if (desc.size() >= 3) {
            size_t keyLen = getTsTypeDescLength(desc.subspan(1));
            auto kDesc = desc.subspan(1, keyLen);
            auto vDesc = desc.subspan(1 + keyLen);
            auto kInfo = interpretTsTypeDescInner(kDesc, typeMap);
            auto vInfo = interpretTsTypeDescInner(vDesc, typeMap);
            MemberInfo r;
            r.tname = "Map<" + kInfo.tname + ", " + vInfo.tname + ">";
            r.ivalue = "new Map()";
            r.deserializeCode = "i.readMap(() => " + kInfo.deserializeCode + ", () => " + vInfo.deserializeCode + ")";
            auto kSer = kInfo.serializeCode;
            auto vSer = vInfo.serializeCode;
            replaceAll(kSer, VAL, "k");
            replaceAll(vSer, VAL, "v");
            r.serializeCode = "o.writeMap({{_}}, (k) => { " + kSer + " }, (v) => { " + vSer + " })";
            return r;
        }
    }

    // optional → T | null
    if (first == E_type::opt) {
        if (desc.size() >= 2) {
            auto vInfo = interpretTsTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = vInfo.tname;
            r.ivalue = "null";
            r.deserializeCode = "i.readOptional(() => " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.writeOptional({{_}}, (v) => { " + ser + " })";
            return r;
        }
    }

    // bitset → Array<boolean>
    if (first == E_type::bitset) {
        MemberInfo r;
        r.tname = "Array<boolean>";
        r.ivalue = "[]";
        r.deserializeCode = "i.readBitset()";
        r.serializeCode = "o.writeBitset({{_}})";
        return r;
    }

    // atomic → 递归解释内部类型
    if (first == E_type::atomic) {
        if (desc.size() >= 2) {
            return interpretTsTypeDescInner(desc.subspan(1), typeMap);
        }
    }

    // std::array → SpArray
    if (first == E_type::array) {
        if (desc.size() >= 3) {
            auto sz = desc[1];
            auto vInfo = interpretTsTypeDescInner(desc.subspan(2), typeMap);
            MemberInfo r;
            auto szStr = std::to_string(sz);
            r.tname = "SpArray";
            r.ivalue = "new SpArray(" + szStr + ")";
            r.deserializeCode = "i.readSpArray(" + szStr + ", () => " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.writeSpArray({{_}}, (v) => { " + ser + " })";
            return r;
        }
    }

    // tuple：[tuple, elem1, elem2, ..., ed]
    if (first == E_type::tuple) {
        std::vector<MemberInfo> elemInfos;
        size_t pos = 1;
        while (pos < desc.size() && desc[pos] != E_type::ed) {
            size_t elemLen = getTsTypeDescLength(desc.subspan(pos));
            elemInfos.push_back(interpretTsTypeDescInner(desc.subspan(pos, elemLen), typeMap));
            pos += elemLen;
        }
        std::string typeNames, ivalues, deserElems, serStmts;
        for (size_t i = 0; i < elemInfos.size(); ++i) {
            if (i > 0) { typeNames += ", "; ivalues += ", "; deserElems += ", "; }
            typeNames += elemInfos[i].tname;
            ivalues += elemInfos[i].ivalue;
            deserElems += elemInfos[i].deserializeCode;
            auto ser = elemInfos[i].serializeCode;
            replaceAll(ser, VAL, "{{_}}[" + std::to_string(i) + "]");
            serStmts += ser + "; ";
        }
        MemberInfo r;
        r.tname = "[" + typeNames + "]";
        r.ivalue = "[" + ivalues + "]";
        r.deserializeCode = "[" + deserElems + "]";
        r.serializeCode = serStmts;
        return r;
    }

    // variant：[variant, elem1, elem2, ..., ed]
    if (first == E_type::variant) {
        std::vector<MemberInfo> elemInfos;
        size_t pos = 1;
        while (pos < desc.size() && desc[pos] != E_type::ed) {
            size_t elemLen = getTsTypeDescLength(desc.subspan(pos));
            elemInfos.push_back(interpretTsTypeDescInner(desc.subspan(pos, elemLen), typeMap));
            pos += elemLen;
        }
        std::string typeNames, deserReaders, serWriters;
        for (size_t i = 0; i < elemInfos.size(); ++i) {
            if (i > 0) { typeNames += " | "; deserReaders += ", "; serWriters += ", "; }
            typeNames += elemInfos[i].tname;
            deserReaders += "() => " + elemInfos[i].deserializeCode;
            auto ser = elemInfos[i].serializeCode;
            replaceAll(ser, VAL, "v");
            serWriters += "(v: any) => " + ser;
        }
        MemberInfo r;
        r.tname = "SpVariant<[" + typeNames + "]>";
        r.ivalue = "new SpVariant(0, 0)";
        r.deserializeCode = "i.readVariant([" + deserReaders + "])";
        r.serializeCode = "o.writeVariant({{_}}.value, {{_}}.typeIndex, [" + serWriters + "])";
        return r;
    }

    // ============ 基础类型 ============
    // 字符串类型: std::string → E_type::string
    if (first == E_type::string) {
        return strInfo;
    }
    // 单字符类型和基本数值类型 → 使用 basicTypeMap 查找
    if (first == E_type::u8 || first == E_type::u16 || first == E_type::u32 ||
        first == E_type::u64 || first == E_type::i8 || first == E_type::i16 ||
        first == E_type::i32 || first == E_type::i64 || first == E_type::f32 ||
        first == E_type::f64 || first == E_type::bl ||
        first == E_type::ch || first == E_type::ch8 || first == E_type::ch16 || first == E_type::ch32) {
        return getBasicTypeInfo(static_cast<Sz>(first));
    }

    // 文件路径
    if (first == E_type::path) {
        MemberInfo r;
        r.tname = "string";
        r.ivalue = "\"\"";
        r.deserializeCode = "i.readString()";
        r.serializeCode = "o.writeString({{_}})";
        return r;
    }

    // 时间类型 → bigint
    if (first == E_type::dur || first == E_type::timepoint) {
        MemberInfo r;
        r.tname = "bigint";
        r.ivalue = "0n";
        r.deserializeCode = "i.readStreamPunkTime()";
        r.serializeCode = "o.writeStreamPunkTime({{_}})";
        return r;
    }

    return nullInfo;
}

inline MemberInfo interpretTsTypeDesc(std::span<const sp_meta::SpToken> desc,
                                       const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    return interpretTsTypeDescInner(desc, typeMap);
}

// ============== 基于元数据生成 TS 类代码 ==============

inline std::string genTsClassCodeFromMeta(const sp_meta::TypeMeta& typeMeta,
                                           const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    std::stringstream ss;
    auto& className = typeMeta.className;
    auto& baseName = typeMeta.baseName;
    auto& members = typeMeta.members;

    // 预先解释所有成员的类型
    std::vector<MemberInfo> memberInfos;
    memberInfos.reserve(members.size());
    for (auto& m : members) {
        memberInfos.push_back(interpretTsTypeDesc(m.typeDesc, typeMap));
    }

    // 类声明
    ss << "export class " << className << " extends " << baseName << " {\n";
    ss << "    static readonly typeID = E_StreamPunkType." << className << ";\n";
    ss << "\n";

    // 成员声明
    for (size_t i = 0; i < members.size(); ++i) {
        ss << "    " << members[i].name << ": " << memberInfos[i].tname << ";\n";
    }
    ss << "\n";

    // 构造函数
    ss << "    constructor() {\n";
    ss << "        super();\n";
    for (size_t i = 0; i < members.size(); ++i) {
        ss << "        this." << members[i].name << " = " << memberInfos[i].ivalue << ";\n";
    }
    ss << "    }\n";
    ss << "\n";

    // from 反序列化（实例方法）
    ss << "    from(i: I): this {\n";
    ss << "        super.from(i);\n";
    for (size_t i = 0; i < members.size(); ++i) {
        ss << "        this." << members[i].name << " = " << memberInfos[i].deserializeCode << ";\n";
    }
    ss << "        return this;\n";
    ss << "    }\n";
    ss << "\n";

    // to 序列化
    ss << "    to(o: O): this {\n";
    ss << "        super.to(o);\n";
    for (size_t i = 0; i < members.size(); ++i) {
        auto code = memberInfos[i].serializeCode;
        replaceAll(code, VAL, std::string("this.") + members[i].name);
        ss << "        " << code << ";\n";
    }
    ss << "        return this;\n";
    ss << "    }\n";
    ss << "}\n";

    return ss.str();
}