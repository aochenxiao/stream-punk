#pragma once

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/MetaData.hpp"
#include "MemberInfoBase.hpp"
#include "TypeDescInterpreter.hpp"
#include <string>
#include <vector>
#include <map>
#include <span>
#include <sstream>

using namespace sp;

// ============== 运行时 TypeDesc 解释器（Python 版本） ==============

inline std::map<Sz, MemberInfo> pyBasicTypeMap = {
    {E_type::u8  , {"int"  , "0"     , "o.write_u8({{_}})" , "i.read_u8()"}},
    {E_type::u16 , {"int"  , "0"     , "o.write_u16({{_}})", "i.read_u16()"}},
    {E_type::u32 , {"int"  , "0"     , "o.write_u32({{_}})", "i.read_u32()"}},
    {E_type::u64 , {"int"  , "0"     , "o.write_u64({{_}})", "i.read_u64()"}},
    {E_type::i8  , {"int"  , "0"     , "o.write_i8({{_}})" , "i.read_i8()"}},
    {E_type::i16 , {"int"  , "0"     , "o.write_i16({{_}})", "i.read_i16()"}},
    {E_type::i32 , {"int"  , "0"     , "o.write_i32({{_}})", "i.read_i32()"}},
    {E_type::i64 , {"int"  , "0"     , "o.write_i64({{_}})", "i.read_i64()"}},
    {E_type::f32 , {"float", "0.0"   , "o.write_f32({{_}})", "i.read_f32()"}},
    {E_type::f64 , {"float", "0.0"   , "o.write_f64({{_}})", "i.read_f64()"}},
    {E_type::bl  , {"bool" , "False" , "o.write_bl({{_}})" , "i.read_bl()"}},
    {E_type::ch  , {"str"  , "\"\""  , "o.write_ch({{_}})" , "i.read_ch()"}},
    {E_type::ch8 , {"str"  , "\"\""  , "o.write_ch8({{_}})", "i.read_ch8()"}},
    {E_type::ch16, {"str"  , "\"\""  , "o.write_ch16({{_}})", "i.read_ch16()"}},
    {E_type::ch32, {"str"  , "\"\""  , "o.write_ch32({{_}})", "i.read_ch32()"}},
};

inline MemberInfo getPyBasicTypeInfo(Sz typeId) {
    auto it = pyBasicTypeMap.find(typeId);
    if (it != pyBasicTypeMap.end()) return it->second;
    return nullInfo;
}

// 计算 TypeDesc 的 token 数量（不生成代码，仅用于确定子描述符的边界）
inline size_t getPyTypeDescLength(std::span<const sp_meta::SpToken> desc) {
    return getTypeDescLength(desc);
}

inline MemberInfo interpretPyTypeDescInner(std::span<const sp_meta::SpToken> desc,
                                            const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    if (desc.empty()) return nullInfo;

    auto first = desc[0];

    // 自定义类型（TypeDesc 中存储的是 E_type::name 的枚举值，范围为 [Base+1, e_customType)）
    if (first >= static_cast<Sz>(E_type::Base) + 1 && first < static_cast<Sz>(E_type::e_customType)) {
        uint32_t typeID = static_cast<uint32_t>(first);
        auto it = typeMap.find(typeID);
        if (it != typeMap.end()) {
            MemberInfo r;
            r.tname = it->second->className;
            r.ivalue = r.tname + "()";
            r.deserializeCode = r.tname + "().from_(i)";
            r.serializeCode = "value.to(o)";
            return r;
        }
        return nullInfo;
    }

    // Base 类型
    if (first == E_type::Base) {
        MemberInfo r;
        r.tname = "Base";
        r.ivalue = "";
        r.deserializeCode = "read_obj(i)";
        r.serializeCode = "value.to(o)";
        return r;
    }

    // 指针类型
    if (first == E_type::ptr || first == E_type::sptr || first == E_type::wptr || first == E_type::uptr) {
        auto subDesc = desc.subspan(1);
        auto vInfo = interpretPyTypeDescInner(subDesc, typeMap);
        bool isCustomPtr = (!subDesc.empty() && (subDesc[0] >= static_cast<Sz>(E_type::Base) + 1 && subDesc[0] < static_cast<Sz>(E_type::e_customType)))
                         || (!subDesc.empty() && subDesc[0] == E_type::Base);
        MemberInfo r;
        r.tname = "SpRef";
        r.ivalue = "SpRef(None, 0)";
        if (isCustomPtr) {
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value)";
        } else {
            r.deserializeCode = "i.read_ptr(lambda: " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_ptr({{_}}.value, {{_}}.address, lambda v: " + ser + ")";
        }
        return r;
    }

    // 容器类型
    if (first == E_type::vector || first == E_type::deque || first == E_type::list || first == E_type::flist) {
        if (desc.size() >= 2) {
            auto vInfo = interpretPyTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "list";
            r.ivalue = "[]";
            r.deserializeCode = "i.read_Array(lambda: " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_Array({{_}}, lambda v: " + ser + ")";
            return r;
        }
    }

    if (first == E_type::set || first == E_type::uset) {
        if (desc.size() >= 2) {
            auto vInfo = interpretPyTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "set";
            r.ivalue = "set()";
            r.deserializeCode = "i.read_set(lambda: " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_set({{_}}, lambda v: " + ser + ")";
            return r;
        }
    }

    if (first == E_type::map || first == E_type::umap) {
        if (desc.size() >= 3) {
            size_t keyLen = getPyTypeDescLength(desc.subspan(1));
            auto kDesc = desc.subspan(1, keyLen);
            auto vDesc = desc.subspan(1 + keyLen);
            auto kInfo = interpretPyTypeDescInner(kDesc, typeMap);
            auto vInfo = interpretPyTypeDescInner(vDesc, typeMap);
            MemberInfo r;
            r.tname = "dict";
            r.ivalue = "{}";
            r.deserializeCode = "i.read_map(lambda: " + kInfo.deserializeCode + ", lambda: " + vInfo.deserializeCode + ")";
            auto kSer = kInfo.serializeCode;
            auto vSer = vInfo.serializeCode;
            replaceAll(kSer, VAL, "k");
            replaceAll(vSer, VAL, "v");
            r.serializeCode = "o.write_map({{_}}, lambda k: " + kSer + ", lambda v: " + vSer + ")";
            return r;
        }
    }

    if (first == E_type::opt) {
        if (desc.size() >= 2) {
            auto vInfo = interpretPyTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = vInfo.tname;
            r.ivalue = "None";
            r.deserializeCode = "i.read_optional(lambda: " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_optional({{_}}, lambda v: " + ser + ")";
            return r;
        }
    }

    if (first == E_type::bitset) {
        MemberInfo r;
        r.tname = "list";
        r.ivalue = "[]";
        r.deserializeCode = "i.read_bitset()";
        r.serializeCode = "o.write_bitset({{_}})";
        return r;
    }

    if (first == E_type::atomic) {
        if (desc.size() >= 2) {
            return interpretPyTypeDescInner(desc.subspan(1), typeMap);
        }
    }

    if (first == E_type::array) {
        if (desc.size() >= 3) {
            auto sz = desc[1];
            auto vInfo = interpretPyTypeDescInner(desc.subspan(2), typeMap);
            MemberInfo r;
            auto szStr = std::to_string(sz);
            r.tname = "SpArray";
            r.ivalue = "SpArray(" + szStr + ")";
            r.deserializeCode = "i.read_SpArray(" + szStr + ", lambda: " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_SpArray({{_}}, lambda v: " + ser + ")";
            return r;
        }
    }

    // tuple：[tuple, elem1, elem2, ..., ed]
    if (first == E_type::tuple) {
        std::vector<MemberInfo> elemInfos;
        size_t pos = 1;
        while (pos < desc.size() && desc[pos] != E_type::ed) {
            size_t elemLen = getPyTypeDescLength(desc.subspan(pos));
            elemInfos.push_back(interpretPyTypeDescInner(desc.subspan(pos, elemLen), typeMap));
            pos += elemLen;
        }
        std::string deserElems, serElems;
        for (size_t i = 0; i < elemInfos.size(); ++i) {
            if (i > 0) { deserElems += ","; serElems += ","; }
            deserElems += elemInfos[i].deserializeCode;
            auto ser = elemInfos[i].serializeCode;
            replaceAll(ser, VAL, "{{_}}[" + std::to_string(i) + "]");
            serElems += ser;
        }
        MemberInfo r;
        r.tname = "tuple";
        r.ivalue = "()";
        r.deserializeCode = "(" + deserElems + ")";
        r.serializeCode = "(" + serElems + ")";
        return r;
    }

    // variant：[variant, elem1, elem2, ..., ed]
    if (first == E_type::variant) {
        std::vector<MemberInfo> elemInfos;
        size_t pos = 1;
        while (pos < desc.size() && desc[pos] != E_type::ed) {
            size_t elemLen = getPyTypeDescLength(desc.subspan(pos));
            elemInfos.push_back(interpretPyTypeDescInner(desc.subspan(pos, elemLen), typeMap));
            pos += elemLen;
        }
        std::string deserReaders, serWriters;
        for (size_t i = 0; i < elemInfos.size(); ++i) {
            if (i > 0) { deserReaders += ","; serWriters += ","; }
            deserReaders += "lambda: " + elemInfos[i].deserializeCode;
            auto ser = elemInfos[i].serializeCode;
            replaceAll(ser, VAL, "v");
            serWriters += "lambda v: " + ser;
        }
        MemberInfo r;
        r.tname = "SpVariant";
        r.ivalue = "SpVariant()";
        r.deserializeCode = "i.read_variant([" + deserReaders + "])";
        r.serializeCode = "o.write_variant({{_}}.value, {{_}}.type_index, [" + serWriters + "])";
        return r;
    }

    // 基础类型
    if (first == E_type::string) {
        MemberInfo r;
        r.tname = "str";
        r.ivalue = "\"\"";
        r.deserializeCode = "i.read_string()";
        r.serializeCode = "o.write_string({{_}})";
        return r;
    }

    if (first == E_type::u8 || first == E_type::u16 || first == E_type::u32 ||
        first == E_type::u64 || first == E_type::i8 || first == E_type::i16 ||
        first == E_type::i32 || first == E_type::i64 || first == E_type::f32 ||
        first == E_type::f64 || first == E_type::bl ||
        first == E_type::ch || first == E_type::ch8 || first == E_type::ch16 || first == E_type::ch32) {
        return getPyBasicTypeInfo(static_cast<Sz>(first));
    }

    if (first == E_type::path) {
        MemberInfo r;
        r.tname = "str";
        r.ivalue = "\"\"";
        r.deserializeCode = "i.read_string()";
        r.serializeCode = "o.write_string({{_}})";
        return r;
    }

    if (first == E_type::dur || first == E_type::timepoint) {
        MemberInfo r;
        r.tname = "int";
        r.ivalue = "0";
        r.deserializeCode = "i.read_stream_punk_time()";
        r.serializeCode = "o.write_stream_punk_time({{_}})";
        return r;
    }

    return nullInfo;
}

inline MemberInfo interpretPyTypeDesc(std::span<const sp_meta::SpToken> desc,
                                       const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    return interpretPyTypeDescInner(desc, typeMap);
}

// ============== 基于元数据生成 Python 类代码 ==============

inline std::string genPyClassCodeFromMeta(const sp_meta::TypeMeta& typeMeta,
                                           const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    std::stringstream ss;
    auto& className = typeMeta.className;
    auto& baseName = typeMeta.baseName;
    auto& members = typeMeta.members;

    std::vector<MemberInfo> memberInfos;
    memberInfos.reserve(members.size());
    for (auto& m : members) {
        memberInfos.push_back(interpretPyTypeDesc(m.typeDesc, typeMap));
    }

    ss << "class " << className << "(" << baseName << "):\n";
    ss << "    typeID = E_StreamPunkType." << className << "\n";
    ss << "\n";
    ss << "    def __init__(self):\n";
    ss << "        super().__init__()\n";
    for (size_t i = 0; i < members.size(); ++i) {
        ss << "        self." << members[i].name << " = " << memberInfos[i].ivalue << "\n";
    }
    ss << "\n";
    ss << "    def from_(self, i: I):\n";
    ss << "        super().from_(i)\n";
    for (size_t i = 0; i < members.size(); ++i) {
        ss << "        self." << members[i].name << " = " << memberInfos[i].deserializeCode << "\n";
    }
    ss << "        return self\n";
    ss << "\n";
    ss << "    def to(self, o: O):\n";
    ss << "        super().to(o)\n";
    for (size_t i = 0; i < members.size(); ++i) {
        auto code = memberInfos[i].serializeCode;
        replaceAll(code, VAL, std::string("self.") + members[i].name);
        ss << "        " << code << "\n";
    }
    ss << "        return self\n";
    ss << "\n";

    return ss.str();
}