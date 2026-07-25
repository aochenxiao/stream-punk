#pragma once

#include "stream-punk/StreamPunk.hpp"
#include "stream-punk/MetaData.hpp"
#include "KotlinMemberInfo.hpp"
#include "KotlinMemberInfoImpl.hpp"
#include "TypeDescInterpreter.hpp"
#include <string>
#include <vector>
#include <map>
#include <span>
#include <sstream>

using namespace sp;

// ============== 运行时 TypeDesc 解释器（Kotlin 版本） ==============

inline std::map<Sz, MemberInfo> kotlinBasicTypeMap = {
    {E_type::u8  , {"Int"    , "0"    , "o.write_u8({{_}})"    , "i.read_u8()"   }},
    {E_type::u16 , {"Int"    , "0"    , "o.write_u16({{_}})"   , "i.read_u16()"  }},
    {E_type::u32 , {"Long"   , "0L"   , "o.write_u32({{_}})"   , "i.read_u32()"  }},
    {E_type::u64 , {"Long"   , "0L"   , "o.write_u64({{_}})"   , "i.read_u64()"  }},
    {E_type::i8  , {"Byte"   , "0"    , "o.write_i8({{_}})"    , "i.read_i8()"   }},
    {E_type::i16 , {"Short"  , "0"    , "o.write_i16({{_}})"   , "i.read_i16()"  }},
    {E_type::i32 , {"Int"    , "0"    , "o.write_i32({{_}})"   , "i.read_i32()"  }},
    {E_type::i64 , {"Long"   , "0L"   , "o.write_i64({{_}})"   , "i.read_i64()"  }},
    {E_type::f32 , {"Float"  , "0.0f" , "o.write_f32({{_}})"   , "i.read_f32()"  }},
    {E_type::f64 , {"Double" , "0.0"  , "o.write_f64({{_}})"   , "i.read_f64()"  }},
    {E_type::bl  , {"Boolean","false" , "o.write_bl({{_}})"    , "i.read_bl()"   }},
    {E_type::ch  , {"Char"   ,"0.toChar()", "o.write_ch({{_}})"  , "i.read_ch()"   }},
    {E_type::ch8 , {"Char"   ,"0.toChar()", "o.write_ch8({{_}})" , "i.read_ch8()"  }},
    {E_type::ch16, {"Char"   ,"0.toChar()", "o.write_ch16({{_}})", "i.read_ch16()" }},
    {E_type::ch32, {"Int"    , "0"    , "o.write_ch32({{_}})"  , "i.read_ch32()" }},
};

inline MemberInfo getKotlinBasicTypeInfo(Sz typeId) {
    auto it = kotlinBasicTypeMap.find(typeId);
    if (it != kotlinBasicTypeMap.end()) return it->second;
    return nullInfo;
}

// 计算 TypeDesc 的 token 数量（不生成代码，仅用于确定子描述符的边界）
inline size_t getKotlinTypeDescLength(std::span<const sp_meta::SpToken> desc) {
    return getTypeDescLength(desc);
}

inline MemberInfo interpretKotlinTypeDescInner(std::span<const sp_meta::SpToken> desc,
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
            r.ivalue = cn + "()";
            r.deserializeCode = cn + "().from_(i)";
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
        auto vInfo = interpretKotlinTypeDescInner(subDesc, typeMap);
        bool isCustomPtr = (!subDesc.empty() && (subDesc[0] >= static_cast<Sz>(E_type::Base) + 1 && subDesc[0] < static_cast<Sz>(E_type::e_customType)))
                         || (!subDesc.empty() && subDesc[0] == E_type::Base);
        MemberInfo r;
        if (isCustomPtr) {
            r.tname = "SpRef<Base?>";
            r.ivalue = "SpRef<Base?>(null, 0)";
            r.deserializeCode = "i.read_ptr_with_typeID()";
            r.serializeCode = "o.write_ptr_with_typeID({{_}}.value)";
        } else {
            r.tname = "SpRef<" + kotlinBoxedType(vInfo.tname) + "?>";
            r.ivalue = "SpRef<" + kotlinBoxedType(vInfo.tname) + "?>(null, 0)";
            r.deserializeCode = "i.read_ptr { " + vInfo.deserializeCode + " }";
            auto ser = vInfo.serializeCode;
            std::string varName = nextKotlinVar();
            replaceAll(ser, VAL, varName);
            r.serializeCode = "o.write_ptr({{_}}.value, {{_}}.address) { " + varName + " -> " + ser + " }";
        }
        return r;
    }

    // 容器类型 - list
    if (first == E_type::vector || first == E_type::deque || first == E_type::list || first == E_type::flist) {
        if (desc.size() >= 2) {
            auto vInfo = interpretKotlinTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "ArrayList<" + kotlinBoxedType(vInfo.tname) + ">";
            r.ivalue = "ArrayList()";
            if (vInfo.arraySize > 0) {
                std::string block = "Array<" + vInfo.arrayElemType + ">(" + std::to_string(vInfo.arraySize) + ") { " + vInfo.deserializeCode + " }";
                r.deserializeCode = "i.read_Array { " + block + " }";
            } else {
                r.deserializeCode = "i.read_Array { " + vInfo.deserializeCode + " }";
            }
            auto ser = vInfo.serializeCode;
            std::string varName = nextKotlinVar();
            replaceAll(ser, VAL, varName);
            r.serializeCode = "o.write_Array({{_}}) { " + varName + " -> " + ser + " }";
            return r;
        }
    }

    // 容器类型 - set
    if (first == E_type::set || first == E_type::uset) {
        if (desc.size() >= 2) {
            auto vInfo = interpretKotlinTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "HashSet<" + kotlinBoxedType(vInfo.tname) + ">";
            r.ivalue = "HashSet()";
            r.deserializeCode = "i.read_set { " + vInfo.deserializeCode + " }";
            auto ser = vInfo.serializeCode;
            std::string varName = nextKotlinVar();
            replaceAll(ser, VAL, varName);
            r.serializeCode = "o.write_set({{_}}) { " + varName + " -> " + ser + " }";
            return r;
        }
    }

    // 容器类型 - map
    if (first == E_type::map || first == E_type::umap) {
        if (desc.size() >= 3) {
            size_t keyLen = getKotlinTypeDescLength(desc.subspan(1));
            auto kDesc = desc.subspan(1, keyLen);
            auto vDesc = desc.subspan(1 + keyLen);
            auto kInfo = interpretKotlinTypeDescInner(kDesc, typeMap);
            auto vInfo = interpretKotlinTypeDescInner(vDesc, typeMap);
            MemberInfo r;
            r.tname = "HashMap<" + kotlinBoxedType(kInfo.tname) + ", " + kotlinBoxedType(vInfo.tname) + ">";
            r.ivalue = "HashMap()";
            r.deserializeCode = "i.read_map({ " + kInfo.deserializeCode + " }, { " + vInfo.deserializeCode + " })";
            auto kSer = kInfo.serializeCode;
            auto vSer = vInfo.serializeCode;
            std::string kVarName = nextKotlinVar();
            std::string vVarName = nextKotlinVar();
            replaceAll(kSer, VAL, kVarName);
            replaceAll(vSer, VAL, vVarName);
            r.serializeCode = "o.write_map({{_}}, { " + kVarName + " -> " + kSer + " }, { " + vVarName + " -> " + vSer + " })";
            return r;
        }
    }

    // optional — 直接透传内部类型
    if (first == E_type::opt) {
        if (desc.size() >= 2) {
            return interpretKotlinTypeDescInner(desc.subspan(1), typeMap);
        }
    }

    // bitset
    if (first == E_type::bitset) {
        MemberInfo r;
        r.tname = "java.util.BitSet?";
        r.ivalue = "null";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }

    // atomic
    if (first == E_type::atomic) {
        if (desc.size() >= 2) {
            return interpretKotlinTypeDescInner(desc.subspan(1), typeMap);
        }
    }

    // array
    if (first == E_type::array) {
        if (desc.size() >= 3) {
            auto sz = desc[1];
            auto vInfo = interpretKotlinTypeDescInner(desc.subspan(2), typeMap);
            MemberInfo r;
            auto szStr = std::to_string(sz);
            r.tname = "Array<" + vInfo.tname + ">";
            r.ivalue = "Array<" + vInfo.tname + ">(" + szStr + ") { " + vInfo.ivalue + " }";
            r.deserializeCode = vInfo.deserializeCode;
            r.arraySize = static_cast<int>(sz);
            r.arrayElemType = vInfo.tname;
            r.serializeCode = "";
            return r;
        }
    }

    // tuple：[tuple, elem1, elem2, ..., ed]
    if (first == E_type::tuple) {
        MemberInfo r;
        r.tname = "Array<Any?>?";
        r.ivalue = "null";
        r.deserializeCode = "null";
        r.serializeCode = "";
        return r;
    }

    // variant：[variant, elem1, elem2, ..., ed]
    if (first == E_type::variant) {
        MemberInfo r;
        r.tname = "Any?";
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
        return getKotlinBasicTypeInfo(static_cast<Sz>(first));
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
        r.tname = "Long";
        r.ivalue = "0L";
        r.deserializeCode = "i.read_i64()";
        r.serializeCode = "o.write_i64({{_}})";
        return r;
    }

    return nullInfo;
}

inline MemberInfo interpretKotlinTypeDesc(std::span<const sp_meta::SpToken> desc,
                                           const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    return interpretKotlinTypeDescInner(desc, typeMap);
}

// ============== 基于元数据生成 Kotlin 类代码 ==============

inline std::string genKotlinClassCodeFromMeta(const sp_meta::TypeMeta& typeMeta,
                                               const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    resetKotlinVarCounter();

    std::stringstream ss;
    auto& className = typeMeta.className;
    auto& baseName = typeMeta.baseName;
    auto& members = typeMeta.members;

    std::vector<MemberInfo> memberInfos;
    memberInfos.reserve(members.size());
    for (auto& m : members) {
        memberInfos.push_back(interpretKotlinTypeDesc(m.typeDesc, typeMap));
    }

    bool isBase = (className == "Base");
    ss << "open class " << className;
    if (!isBase) {
        ss << " : " << baseName << "()";
    }
    ss << " {\n";
    ss << "    companion object {\n";
    ss << "        const val typeID = E_StreamPunkType." << className << "\n";
    ss << "    }\n\n";

    for (size_t i = 0; i < members.size(); ++i) {
        auto& info = memberInfos[i];
        ss << "    var " << members[i].name << ": " << info.tname << " = " << info.ivalue << "\n";
    }
    ss << "\n";

    ss << "    override fun from_(i: I): " << className << " {\n";
    if (baseName != "Base" || !isBase) {
        ss << "        super.from_(i)\n";
    }
    for (size_t i = 0; i < members.size(); ++i) {
        auto& info = memberInfos[i];
        if (info.arraySize > 0) {
            ss << "        for (_i in 0 until " << info.arraySize << ") this." << members[i].name << "[_i] = " << info.deserializeCode << "\n";
        } else {
            ss << "        this." << members[i].name << " = " << info.deserializeCode << "\n";
        }
    }
    ss << "        return this\n";
    ss << "    }\n\n";

    ss << "    override fun to(o: O) {\n";
    if (baseName != "Base" || !isBase) {
        ss << "        super.to(o)\n";
    }
    for (size_t i = 0; i < members.size(); ++i) {
        auto& info = memberInfos[i];
        if (!info.serializeCode.empty()) {
            auto code = info.serializeCode;
            replaceAll(code, VAL, std::string("this.") + members[i].name);
            ss << "        " << code << "\n";
        }
    }
    ss << "    }\n";
    ss << "}\n\n";

    return ss.str();
}