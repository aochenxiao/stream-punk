#pragma once

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/MetaData.hpp"
#include "MemberInfoBase.hpp"
#include "JavaMemberInfo.hpp"
#include "TypeDescInterpreter.hpp"
#include <string>
#include <vector>
#include <map>
#include <span>
#include <sstream>

using namespace sp;

// ============== 运行时 TypeDesc 解释器（Java 版本） ==============

inline std::map<Sz, MemberInfo> javaBasicTypeMap = {
    {E_type::u8  , {"int"    , "0"    , "o.write_u8({{_}})"   , "i.read_u8()"  }},
    {E_type::u16 , {"int"    , "0"    , "o.write_u16({{_}})"  , "i.read_u16()" }},
    {E_type::u32 , {"long"   , "0L"   , "o.write_u32({{_}})"  , "i.read_u32()" }},
    {E_type::u64 , {"long"   , "0L"   , "o.write_u64({{_}})"  , "i.read_u64()" }},
    {E_type::i8  , {"byte"   , "0"    , "o.write_i8({{_}})"   , "i.read_i8()"  }},
    {E_type::i16 , {"short"  , "0"    , "o.write_i16({{_}})"  , "i.read_i16()" }},
    {E_type::i32 , {"int"    , "0"    , "o.write_i32({{_}})"  , "i.read_i32()" }},
    {E_type::i64 , {"long"   , "0L"   , "o.write_i64({{_}})"  , "i.read_i64()" }},
    {E_type::f32 , {"float"  , "0.0f" , "o.write_f32({{_}})"  , "i.read_f32()" }},
    {E_type::f64 , {"double" , "0.0"  , "o.write_f64({{_}})"  , "i.read_f64()" }},
    {E_type::bl  , {"boolean","false" , "o.write_bl({{_}})"   , "i.read_bl()"  }},
    {E_type::ch  , {"char"   ,"'\\0'" , "o.write_ch({{_}})"   , "i.read_ch()"  }},
    {E_type::ch8 , {"char"   ,"'\\0'" , "o.write_ch8({{_}})"  , "i.read_ch8()" }},
    {E_type::ch16, {"char"   ,"'\\0'" , "o.write_ch16({{_}})" , "i.read_ch16()"}},
    {E_type::ch32, {"int"    , "0"    , "o.write_ch32({{_}})" , "i.read_ch32()"}},
};

inline MemberInfo getJavaBasicTypeInfo(Sz typeId) {
    auto it = javaBasicTypeMap.find(typeId);
    if (it != javaBasicTypeMap.end()) return it->second;
    return nullInfo;
}

// 计算 TypeDesc 的 token 数量（不生成代码，仅用于确定子描述符的边界）
inline size_t getJavaTypeDescLength(std::span<const sp_meta::SpToken> desc) {
    return getTypeDescLength(desc);
}

inline MemberInfo interpretJavaTypeDescInner(std::span<const sp_meta::SpToken> desc,
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
            r.ivalue = "new " + cn + "()";
            r.deserializeCode = "new " + cn + "().from_(i)";
            r.serializeCode = "write_obj(o, {{_}})";
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
        r.serializeCode = "write_obj(o, {{_}})";
        return r;
    }

    // 指针类型
    if (first == E_type::ptr || first == E_type::sptr || first == E_type::wptr || first == E_type::uptr) {
        auto subDesc = desc.subspan(1);
        auto vInfo = interpretJavaTypeDescInner(subDesc, typeMap);
        bool isCustomPtr = (!subDesc.empty() && (subDesc[0] >= static_cast<Sz>(E_type::Base) + 1 && subDesc[0] < static_cast<Sz>(E_type::e_customType)))
                         || (!subDesc.empty() && subDesc[0] == E_type::Base);
        MemberInfo r;
        if (isCustomPtr) {
            r.tname = "SpRef<Base>";
            r.ivalue = "new SpRef<>(null, 0)";
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value)";
        } else {
            r.tname = "SpRef<" + javaBoxedType(vInfo.tname) + ">";
            r.ivalue = "new SpRef<>(null, 0)";
            r.deserializeCode = "i.read_ptr(() -> " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_ptr({{_}}.value, {{_}}.address, v -> { " + ser + "; })";
        }
        return r;
    }

    // 容器类型 - list
    if (first == E_type::vector || first == E_type::deque || first == E_type::list || first == E_type::flist) {
        if (desc.size() >= 2) {
            auto vInfo = interpretJavaTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "ArrayList<" + javaBoxedType(vInfo.tname) + ">";
            r.ivalue = "new ArrayList<>()";
            r.deserializeCode = "i.read_Array(() -> " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_Array({{_}}, v -> { " + ser + "; })";
            return r;
        }
    }

    // 容器类型 - set
    if (first == E_type::set || first == E_type::uset) {
        if (desc.size() >= 2) {
            auto vInfo = interpretJavaTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "HashSet<" + javaBoxedType(vInfo.tname) + ">";
            r.ivalue = "new HashSet<>()";
            r.deserializeCode = "i.read_set(() -> " + vInfo.deserializeCode + ")";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.write_set({{_}}, v -> { " + ser + "; })";
            return r;
        }
    }

    // 容器类型 - map
    if (first == E_type::map || first == E_type::umap) {
        if (desc.size() >= 3) {
            size_t keyLen = getJavaTypeDescLength(desc.subspan(1));
            auto kDesc = desc.subspan(1, keyLen);
            auto vDesc = desc.subspan(1 + keyLen);
            auto kInfo = interpretJavaTypeDescInner(kDesc, typeMap);
            auto vInfo = interpretJavaTypeDescInner(vDesc, typeMap);
            MemberInfo r;
            r.tname = "HashMap<" + javaBoxedType(kInfo.tname) + ", " + javaBoxedType(vInfo.tname) + ">";
            r.ivalue = "new HashMap<>()";
            r.deserializeCode = "i.read_map(() -> " + kInfo.deserializeCode + ", () -> " + vInfo.deserializeCode + ")";
            auto kSer = kInfo.serializeCode;
            auto vSer = vInfo.serializeCode;
            replaceAll(kSer, VAL, "k");
            replaceAll(vSer, VAL, "v");
            r.serializeCode = "o.write_map({{_}}, k -> { " + kSer + "; }, v -> { " + vSer + "; })";
            return r;
        }
    }

    // optional
    if (first == E_type::opt) {
        if (desc.size() >= 2) {
            auto vInfo = interpretJavaTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = vInfo.tname;
            r.ivalue = vInfo.ivalue;
            r.deserializeCode = vInfo.deserializeCode;
            r.serializeCode = vInfo.serializeCode;
            return r;
        }
    }

    // bitset
    if (first == E_type::bitset) {
        MemberInfo r;
        r.tname = "java.util.BitSet";
        r.ivalue = "new java.util.BitSet()";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }

    // atomic
    if (first == E_type::atomic) {
        if (desc.size() >= 2) {
            return interpretJavaTypeDescInner(desc.subspan(1), typeMap);
        }
    }

    // array
    if (first == E_type::array) {
        if (desc.size() >= 3) {
            auto sz = desc[1];
            auto vInfo = interpretJavaTypeDescInner(desc.subspan(2), typeMap);
            MemberInfo r;
            auto szStr = std::to_string(sz);
            r.tname = vInfo.tname + "[]";
            r.ivalue = "new " + vInfo.tname + "[" + szStr + "]";
            r.deserializeCode = vInfo.deserializeCode;
            r.arraySize = static_cast<int>(sz);
            r.arrayElemType = vInfo.tname;
            r.serializeCode = "";
            return r;
        }
    }

    // tuple：[tuple, elem1, elem2, ..., ed]
    if (first == E_type::tuple) {
        std::vector<MemberInfo> elemInfos;
        size_t pos = 1;
        while (pos < desc.size() && desc[pos] != E_type::ed) {
            size_t elemLen = getJavaTypeDescLength(desc.subspan(pos));
            elemInfos.push_back(interpretJavaTypeDescInner(desc.subspan(pos, elemLen), typeMap));
            pos += elemLen;
        }
        std::string deserElems, serStmts;
        for (size_t i = 0; i < elemInfos.size(); ++i) {
            if (i > 0) { deserElems += ", "; }
            deserElems += elemInfos[i].deserializeCode;
            auto ser = elemInfos[i].serializeCode;
            if (!ser.empty()) {
                replaceAll(ser, VAL, "((Object[]){{_}})[" + std::to_string(i) + "]");
                serStmts += ser + "; ";
            }
        }
        MemberInfo r;
        r.tname = "Object[]";
        r.ivalue = "new Object[0]";
        r.deserializeCode = "new Object[]{" + deserElems + "}";
        r.serializeCode = serStmts;
        return r;
    }

    // variant：[variant, elem1, elem2, ..., ed]
    if (first == E_type::variant) {
        MemberInfo r;
        r.tname = "Object";
        r.ivalue = "null";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }

    // 基础类型
    if (first == E_type::string) {
        MemberInfo r;
        r.tname = "String";
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
        return getJavaBasicTypeInfo(static_cast<Sz>(first));
    }

    if (first == E_type::path) {
        MemberInfo r;
        r.tname = "String";
        r.ivalue = "\"\"";
        r.deserializeCode = "i.read_string()";
        r.serializeCode = "o.write_string({{_}})";
        return r;
    }

    if (first == E_type::dur || first == E_type::timepoint) {
        MemberInfo r;
        r.tname = "long";
        r.ivalue = "0L";
        r.deserializeCode = "i.read_i64()";
        r.serializeCode = "o.write_i64({{_}})";
        return r;
    }

    return nullInfo;
}

inline MemberInfo interpretJavaTypeDesc(std::span<const sp_meta::SpToken> desc,
                                         const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    return interpretJavaTypeDescInner(desc, typeMap);
}

// ============== 基于元数据生成 Java 类代码 ==============

inline std::string genJavaClassCodeFromMeta(const sp_meta::TypeMeta& typeMeta,
                                             const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    std::stringstream ss;
    auto& className = typeMeta.className;
    auto& baseName = typeMeta.baseName;
    auto& members = typeMeta.members;

    std::vector<MemberInfo> memberInfos;
    memberInfos.reserve(members.size());
    for (auto& m : members) {
        memberInfos.push_back(interpretJavaTypeDesc(m.typeDesc, typeMap));
    }

    ss << "class " << className << " extends " << baseName << " {\n";
    ss << "    public static final int typeID = E_StreamPunkType." << className << ";\n";
    ss << "\n";
    for (size_t i = 0; i < members.size(); ++i) {
        ss << "    public " << memberInfos[i].tname << " " << members[i].name << " = " << memberInfos[i].ivalue << ";\n";
    }
    ss << "\n";
    ss << "    public " << className << "() {\n";
    ss << "        super();\n";
    ss << "    }\n";
    ss << "\n";
    ss << "    @Override\n";
    ss << "    public " << className << " from_(I i) {\n";
    ss << "        super.from_(i);\n";
    for (size_t i = 0; i < members.size(); ++i) {
        auto& info = memberInfos[i];
        if (info.arraySize > 0) {
            ss << "        for (int _i = 0; _i < " << info.arraySize << "; _i++) this." << members[i].name << "[_i] = " << info.deserializeCode << ";\n";
        } else {
            ss << "        this." << members[i].name << " = " << info.deserializeCode << ";\n";
        }
    }
    ss << "        return this;\n";
    ss << "    }\n";
    ss << "\n";
    ss << "    @Override\n";
    ss << "    public void to(O o) {\n";
    ss << "        super.to(o);\n";
    for (size_t i = 0; i < members.size(); ++i) {
        auto& info = memberInfos[i];
        auto code = info.serializeCode;
        if (!code.empty()) {
            replaceAll(code, VAL, std::string("this.") + members[i].name);
            ss << "        " << code << ";\n";
        }
    }
    ss << "    }\n";
    ss << "}\n";
    ss << "\n";

    return ss.str();
}