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

// ============== 运行时 TypeDesc 解释器（Rust 版本） ==============

inline std::map<Sz, MemberInfo> rustBasicTypeMap = {
    {E_type::u8  , {"u8"  , "0"     , "o.write_u8({{_}})"  , "i.read_u8()"  }},
    {E_type::u16 , {"u16" , "0"     , "o.write_u16({{_}})" , "i.read_u16()" }},
    {E_type::u32 , {"u32" , "0"     , "o.write_u32({{_}})" , "i.read_u32()" }},
    {E_type::u64 , {"u64" , "0"     , "o.write_u64({{_}})" , "i.read_u64()" }},
    {E_type::i8  , {"i8"  , "0"     , "o.write_i8({{_}})"  , "i.read_i8()"  }},
    {E_type::i16 , {"i16" , "0"     , "o.write_i16({{_}})" , "i.read_i16()" }},
    {E_type::i32 , {"i32" , "0"     , "o.write_i32({{_}})" , "i.read_i32()" }},
    {E_type::i64 , {"i64" , "0"     , "o.write_i64({{_}})" , "i.read_i64()" }},
    {E_type::f32 , {"f32" , "0.0"   , "o.write_f32({{_}})" , "i.read_f32()" }},
    {E_type::f64 , {"f64" , "0.0"   , "o.write_f64({{_}})" , "i.read_f64()" }},
    {E_type::bl  , {"bool", "false" , "o.write_bl({{_}})"  , "i.read_bl()"  }},
    {E_type::ch  , {"char", "'\\0'" , "o.write_ch({{_}})"  , "i.read_ch()"  }},
    {E_type::ch8 , {"char", "'\\0'" , "o.write_ch8({{_}})" , "i.read_ch8()" }},
    {E_type::ch16, {"char", "'\\0'" , "o.write_ch16({{_}})", "i.read_ch16()"}},
    {E_type::ch32, {"u32" , "0"     , "o.write_ch32({{_}})", "i.read_ch32()"}},
};

inline MemberInfo getRustBasicTypeInfo(Sz typeId) {
    auto it = rustBasicTypeMap.find(typeId);
    if (it != rustBasicTypeMap.end()) return it->second;
    return nullInfo;
}

// 计算 TypeDesc 的 token 数量（不生成代码，仅用于确定子描述符的边界）
inline size_t getRustTypeDescLength(std::span<const sp_meta::SpToken> desc) {
    return getTypeDescLength(desc);
}

inline MemberInfo interpretRustTypeDescInner(std::span<const sp_meta::SpToken> desc,
                                              const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    if (desc.empty()) return nullInfo;

    auto first = desc[0];

    // 自定义类型（TypeDesc 中存储的是 E_type::name 的枚举值，范围为 [Base+1, e_customType)）
    if (first >= static_cast<Sz>(E_type::Base) + 1 && first < static_cast<Sz>(E_type::e_customType)) {
        uint32_t typeID = static_cast<uint32_t>(first);
        auto it = typeMap.find(typeID);
        if (it != typeMap.end()) {
            MemberInfo r;
            auto& cn = it->second->className;
            r.tname = cn;
            r.ivalue = cn + "::default()";
            r.deserializeCode = cn + "::from_i(i)";
            r.serializeCode = "write_obj(o, {{_}})";
            return r;
        }
        return nullInfo;
    }

    // Base 类型
    if (first == E_type::Base) {
        MemberInfo r;
        r.tname = "Option<Arc<dyn SpBase>>";
        r.ivalue = "None";
        r.deserializeCode = "read_obj(i).map(|b| Arc::from(b))";
        r.serializeCode = "if let Some(ref v) = {{_}} { write_obj(o, v.as_ref()) }";
        return r;
    }

    // 指针类型
    if (first == E_type::ptr || first == E_type::sptr || first == E_type::wptr || first == E_type::uptr) {
        auto subDesc = desc.subspan(1);
        auto vInfo = interpretRustTypeDescInner(subDesc, typeMap);
        bool isCustomPtr = (!subDesc.empty() && (subDesc[0] >= static_cast<Sz>(E_type::Base) + 1 && subDesc[0] < static_cast<Sz>(E_type::e_customType)))
                         || (!subDesc.empty() && subDesc[0] == E_type::Base);
        MemberInfo r;
        if (isCustomPtr) {
            r.tname = "SpRef<Arc<dyn SpBase>>";
            r.ivalue = "SpRef::none()";
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value.as_deref())";
        } else {
            r.tname = "SpRef<" + vInfo.tname + ">";
            r.ivalue = "SpRef::none()";
            r.deserializeCode = "i.read_ptr(|i| " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "*v");
            r.serializeCode = "o.write_ptr({{_}}.value.as_ref(), {{_}}.address, |o, v| { " + ser + " })";
        }
        return r;
    }

    // 容器类型 - list
    if (first == E_type::vector || first == E_type::deque || first == E_type::list || first == E_type::flist) {
        if (desc.size() >= 2) {
            auto vInfo = interpretRustTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "Vec<" + vInfo.tname + ">";
            r.ivalue = "Vec::new()";
            r.deserializeCode = "i.read_Array(|i| " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "*v");
            r.serializeCode = "o.write_Array({{_}}, |o, v| { " + ser + " })";
            return r;
        }
    }

    // 容器类型 - set
    if (first == E_type::set || first == E_type::uset) {
        if (desc.size() >= 2) {
            auto vInfo = interpretRustTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "HashSet<" + vInfo.tname + ">";
            r.ivalue = "HashSet::new()";
            r.deserializeCode = "i.read_set(|i| " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "*v");
            r.serializeCode = "o.write_set({{_}}, |o, v| { " + ser + " })";
            return r;
        }
    }

    // 容器类型 - map
    if (first == E_type::map || first == E_type::umap) {
        if (desc.size() >= 3) {
            size_t keyLen = getRustTypeDescLength(desc.subspan(1));
            auto kDesc = desc.subspan(1, keyLen);
            auto vDesc = desc.subspan(1 + keyLen);
            auto kInfo = interpretRustTypeDescInner(kDesc, typeMap);
            auto vInfo = interpretRustTypeDescInner(vDesc, typeMap);
            MemberInfo r;
            r.tname = "HashMap<" + kInfo.tname + ", " + vInfo.tname + ">";
            r.ivalue = "HashMap::new()";
            r.deserializeCode = "i.read_map(|i| " + kInfo.deserializeCode + ", |i| " + vInfo.deserializeCode + ")";
            auto kSer = kInfo.serializeCode;
            auto vSer = vInfo.serializeCode;
            replaceAll(kSer, VAL, "*k");
            replaceAll(vSer, VAL, "*v");
            r.serializeCode = "o.write_map({{_}}, |o, k| { " + kSer + " }, |o, v| { " + vSer + " })";
            return r;
        }
    }

    // optional
    if (first == E_type::opt) {
        if (desc.size() >= 2) {
            auto vInfo = interpretRustTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "Option<" + vInfo.tname + ">";
            r.ivalue = "None";
            r.deserializeCode = "if i.read_bl() { Some(" + vInfo.deserializeCode + ") } else { None }";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "match {{_}} { Some(v) => { o.write_bl(true); " + ser + " } None => o.write_bl(false) }";
            return r;
        }
    }

    // bitset
    if (first == E_type::bitset) {
        MemberInfo r;
        r.tname = "Vec<bool>";
        r.ivalue = "Vec::new()";
        r.deserializeCode = "i.read_Array(|i| i.read_bl())";
        r.serializeCode = "o.write_Array({{_}}, |o, v| o.write_bl(*v))";
        return r;
    }

    // atomic
    if (first == E_type::atomic) {
        if (desc.size() >= 2) {
            return interpretRustTypeDescInner(desc.subspan(1), typeMap);
        }
    }

    // array
    if (first == E_type::array) {
        if (desc.size() >= 3) {
            auto sz = desc[1];
            auto vInfo = interpretRustTypeDescInner(desc.subspan(2), typeMap);
            MemberInfo r;
            auto szStr = std::to_string(sz);
            r.tname = "[" + vInfo.tname + "; " + szStr + "]";
            r.ivalue = "Default::default()";
            r.arraySize = static_cast<int>(sz);
            r.arrayElemType = vInfo.tname;
            r.deserializeCode = "{ let mut arr = Default::default(); for _idx in 0.." + szStr + " { arr[_idx] = " + vInfo.deserializeCode + "; } arr }";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "{{_}}[_idx]");
            r.serializeCode = "for _idx in 0.." + szStr + " { " + ser + "; }";
            return r;
        }
    }

    // tuple：[tuple, elem1, elem2, ..., ed]
    if (first == E_type::tuple) {
        std::vector<MemberInfo> elemInfos;
        size_t pos = 1;
        while (pos < desc.size() && desc[pos] != E_type::ed) {
            size_t elemLen = getRustTypeDescLength(desc.subspan(pos));
            elemInfos.push_back(interpretRustTypeDescInner(desc.subspan(pos, elemLen), typeMap));
            pos += elemLen;
        }
        std::string typeNames, deserElems, serStmts;
        for (size_t i = 0; i < elemInfos.size(); ++i) {
            if (i > 0) { typeNames += ", "; deserElems += ", "; }
            typeNames += elemInfos[i].tname;
            deserElems += elemInfos[i].deserializeCode;
            auto ser = elemInfos[i].serializeCode;
            if (!ser.empty()) {
                replaceAll(ser, VAL, "{{_}}." + std::to_string(i));
                serStmts += ser + "; ";
            }
        }
        MemberInfo r;
        r.tname = "(" + typeNames + ")";
        r.ivalue = "Default::default()";
        r.deserializeCode = "(" + deserElems + ")";
        r.serializeCode = serStmts;
        return r;
    }

    // variant：[variant, elem1, elem2, ..., ed]
    if (first == E_type::variant) {
        MemberInfo r;
        r.tname = "SpVariant";
        r.ivalue = "SpVariant::new()";
        r.deserializeCode = "SpVariant::new()";
        r.serializeCode = "";
        return r;
    }

    // 基础类型
    if (first == E_type::string) {
        MemberInfo r;
        r.tname = "String";
        r.ivalue = "String::new()";
        r.deserializeCode = "i.read_string()";
        r.serializeCode = "o.write_string(&{{_}})";
        return r;
    }

    if (first == E_type::u8 || first == E_type::u16 || first == E_type::u32 ||
        first == E_type::u64 || first == E_type::i8 || first == E_type::i16 ||
        first == E_type::i32 || first == E_type::i64 || first == E_type::f32 ||
        first == E_type::f64 || first == E_type::bl ||
        first == E_type::ch || first == E_type::ch8 || first == E_type::ch16 || first == E_type::ch32) {
        return getRustBasicTypeInfo(static_cast<Sz>(first));
    }

    if (first == E_type::path) {
        MemberInfo r;
        r.tname = "String";
        r.ivalue = "String::new()";
        r.deserializeCode = "i.read_string()";
        r.serializeCode = "o.write_string(&{{_}})";
        return r;
    }

    if (first == E_type::dur || first == E_type::timepoint) {
        MemberInfo r;
        r.tname = "i64";
        r.ivalue = "0";
        r.deserializeCode = "i.read_i64()";
        r.serializeCode = "o.write_i64({{_}})";
        return r;
    }

    return nullInfo;
}

inline MemberInfo interpretRustTypeDesc(std::span<const sp_meta::SpToken> desc,
                                         const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    return interpretRustTypeDescInner(desc, typeMap);
}

// ============== 基于元数据生成 Rust 类代码 ==============

inline std::string genRustClassCodeFromMeta(const sp_meta::TypeMeta& typeMeta,
                                             const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    std::stringstream ss;
    auto& className = typeMeta.className;
    auto& members = typeMeta.members;

    std::vector<MemberInfo> memberInfos;
    memberInfos.reserve(members.size());
    for (auto& m : members) {
        memberInfos.push_back(interpretRustTypeDesc(m.typeDesc, typeMap));
    }

    ss << "#[derive(Debug, Clone)]\n";
    ss << "pub struct " << className << " {\n";
    for (size_t i = 0; i < members.size(); ++i) {
        ss << "    pub " << members[i].name << ": " << memberInfos[i].tname << ",\n";
    }
    ss << "}\n\n";

    ss << "impl Default for " << className << " {\n";
    ss << "    fn default() -> Self {\n";
    ss << "        Self {\n";
    for (size_t i = 0; i < members.size(); ++i) {
        ss << "            " << members[i].name << ": " << memberInfos[i].ivalue << ",\n";
    }
    ss << "        }\n";
    ss << "    }\n";
    ss << "}\n\n";

    ss << "impl SpBase for " << className << " {\n";
    ss << "    fn type_id(&self) -> u32 {\n";
    ss << "        E_StreamPunkType::" << className << "\n";
    ss << "    }\n\n";
    ss << "    fn from_(&mut self, i: &mut I) {\n";
    for (size_t i = 0; i < members.size(); ++i) {
        auto& info = memberInfos[i];
        if (info.arraySize > 0) {
            ss << "        for _idx in 0.." << info.arraySize << " {\n";
            ss << "            self." << members[i].name << "[_idx] = " << info.deserializeCode << ";\n";
            ss << "        }\n";
        } else {
            ss << "        self." << members[i].name << " = " << info.deserializeCode << ";\n";
        }
    }
    ss << "    }\n\n";
    ss << "    fn to(&self, o: &mut O) {\n";
    for (size_t i = 0; i < members.size(); ++i) {
        auto& info = memberInfos[i];
        if (!info.serializeCode.empty()) {
            auto code = info.serializeCode;
            replaceAll(code, VAL, std::string("self.") + members[i].name);
            ss << "        " << code << ";\n";
        }
    }
    ss << "    }\n";
    ss << "}\n\n";

    ss << "impl " << className << " {\n";
    ss << "    pub fn from_i(i: &mut I) -> Self {\n";
    ss << "        let mut obj = Self::default();\n";
    ss << "        obj.from_(i);\n";
    ss << "        obj\n";
    ss << "    }\n";
    ss << "}\n\n";

    return ss.str();
}