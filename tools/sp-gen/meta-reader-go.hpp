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

// ============== 运行时 TypeDesc 解释器（Go 版本） ==============

inline std::map<Sz, MemberInfo> goBasicTypeMap = {
    {E_type::u8  , {"uint8"  , "0"    , "o.WriteU8({{_}})"  , "i.ReadU8()"  }},
    {E_type::u16 , {"uint16" , "0"    , "o.WriteU16({{_}})" , "i.ReadU16()" }},
    {E_type::u32 , {"uint32" , "0"    , "o.WriteU32({{_}})" , "i.ReadU32()" }},
    {E_type::u64 , {"uint64" , "0"    , "o.WriteU64({{_}})" , "i.ReadU64()" }},
    {E_type::i8  , {"int8"   , "0"    , "o.WriteI8({{_}})"  , "i.ReadI8()"  }},
    {E_type::i16 , {"int16"  , "0"    , "o.WriteI16({{_}})" , "i.ReadI16()" }},
    {E_type::i32 , {"int32"  , "0"    , "o.WriteI32({{_}})" , "i.ReadI32()" }},
    {E_type::i64 , {"int64"  , "0"    , "o.WriteI64({{_}})" , "i.ReadI64()" }},
    {E_type::f32 , {"float32", "0.0"  , "o.WriteF32({{_}})" , "i.ReadF32()" }},
    {E_type::f64 , {"float64", "0.0"  , "o.WriteF64({{_}})" , "i.ReadF64()" }},
    {E_type::bl  , {"bool"   , "false", "o.WriteBl({{_}})"  , "i.ReadBl()"  }},
    {E_type::ch  , {"byte"   , "0"    , "o.WriteCh({{_}})"  , "i.ReadCh()"  }},
    {E_type::ch8 , {"byte"   , "0"    , "o.WriteCh8({{_}})" , "i.ReadCh8()" }},
    {E_type::ch16, {"uint16" , "0"    , "o.WriteCh16({{_}})", "i.ReadCh16()"}},
    {E_type::ch32, {"uint32" , "0"    , "o.WriteCh32({{_}})", "i.ReadCh32()"}},
};

inline MemberInfo getGoBasicTypeInfo(Sz typeId) {
    auto it = goBasicTypeMap.find(typeId);
    if (it != goBasicTypeMap.end()) return it->second;
    return nullInfo;
}

// 计算 TypeDesc 的 token 数量（不生成代码，仅用于确定子描述符的边界）
inline size_t getGoTypeDescLength(std::span<const sp_meta::SpToken> desc) {
    return getTypeDescLength(desc);
}

inline MemberInfo interpretGoTypeDescInner(std::span<const sp_meta::SpToken> desc,
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
            r.tname = "*" + cn;
            r.ivalue = "&" + cn + "{}";
            r.deserializeCode = "&" + cn + "{}; {{_}}.From_(i)";
            r.serializeCode = "WriteObj(o, {{_}})";
            return r;
        }
        return nullInfo;
    }

    // Base 类型
    if (first == E_type::Base) {
        MemberInfo r;
        r.tname = "SpBase";
        r.ivalue = "nil";
        r.deserializeCode = "ReadObj(i)";
        r.serializeCode = "WriteObj(o, {{_}})";
        return r;
    }

    // 指针类型
    if (first == E_type::ptr || first == E_type::sptr || first == E_type::wptr || first == E_type::uptr) {
        auto subDesc = desc.subspan(1);
        auto vInfo = interpretGoTypeDescInner(subDesc, typeMap);
        bool isCustomPtr = (!subDesc.empty() && (subDesc[0] >= static_cast<Sz>(E_type::Base) + 1 && subDesc[0] < static_cast<Sz>(E_type::e_customType)))
                         || (!subDesc.empty() && subDesc[0] == E_type::Base);
        MemberInfo r;
        r.tname = "SpRef";
        r.ivalue = "NewSpRef()";
        if (isCustomPtr) {
            r.deserializeCode = "i.ReadPtrWithTypeID()";
            r.serializeCode = "o.WritePtrWithTypeID({{_}}.Value)";
        } else {
            r.deserializeCode = "i.ReadPtr(func() interface{} { return " + vInfo.deserializeCode + " })";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "v");
            r.serializeCode = "o.WritePtr({{_}}.Value, {{_}}.Address, func(v interface{}) { _v := v.(" + vInfo.tname + "); " + ser + " })";
        }
        return r;
    }

    // 容器类型 - list
    if (first == E_type::vector || first == E_type::deque || first == E_type::list || first == E_type::flist) {
        if (desc.size() >= 2) {
            auto vInfo = interpretGoTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            auto retType = "[]" + vInfo.tname;
            r.tname = retType;
            r.ivalue = "nil";
            auto d = vInfo.deserializeCode;
            replaceAll(d, VAL, "_tmp[_j]");
            r.deserializeCode = "func() " + retType + " { _len := i.ReadSz(); _tmp := make(" + retType + ", _len); for _j := 0; _j < _len; _j++ { " + d + " }; return _tmp }()";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "_v");
            r.serializeCode = "o.WriteSz(len({{_}})); for _, _v := range {{_}} { " + ser + " }";
            return r;
        }
    }

    // 容器类型 - set
    if (first == E_type::set || first == E_type::uset) {
        if (desc.size() >= 2) {
            auto vInfo = interpretGoTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            auto retType = "map[" + vInfo.tname + "]struct{}";
            r.tname = retType;
            r.ivalue = "nil";
            auto d = vInfo.deserializeCode;
            replaceAll(d, VAL, "_k");
            r.deserializeCode = "func() " + retType + " { _len := i.ReadSz(); _tmp := make(" + retType + "); for _j := 0; _j < _len; _j++ { _k := " + d + "; _tmp[_k] = struct{}{} }; return _tmp }()";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "k");
            r.serializeCode = "o.WriteSz(len({{_}})); for k := range {{_}} { " + ser + " }";
            return r;
        }
    }

    // 容器类型 - map
    if (first == E_type::map || first == E_type::umap) {
        if (desc.size() >= 3) {
            size_t keyLen = getGoTypeDescLength(desc.subspan(1));
            auto kDesc = desc.subspan(1, keyLen);
            auto vDesc = desc.subspan(1 + keyLen);
            auto kInfo = interpretGoTypeDescInner(kDesc, typeMap);
            auto vInfo = interpretGoTypeDescInner(vDesc, typeMap);
            MemberInfo r;
            auto retType = "map[" + kInfo.tname + "]" + vInfo.tname;
            r.tname = retType;
            r.ivalue = "nil";
            auto kd = kInfo.deserializeCode;
            auto vd = vInfo.deserializeCode;
            replaceAll(kd, VAL, "k");
            replaceAll(vd, VAL, "v");
            r.deserializeCode = "func() " + retType + " { _len := i.ReadSz(); _tmp := make(" + retType + "); for _j := 0; _j < _len; _j++ { k := " + kd + "; v := " + vd + "; _tmp[k] = v }; return _tmp }()";
            auto ks = kInfo.serializeCode;
            auto vs = vInfo.serializeCode;
            replaceAll(ks, VAL, "k");
            replaceAll(vs, VAL, "v");
            r.serializeCode = "o.WriteSz(len({{_}})); for k, v := range {{_}} { " + ks + "; " + vs + " }";
            return r;
        }
    }

    // optional
    if (first == E_type::opt) {
        if (desc.size() >= 2) {
            auto vInfo = interpretGoTypeDescInner(desc.subspan(1), typeMap);
            MemberInfo r;
            r.tname = "*" + vInfo.tname;
            r.ivalue = "nil";
            auto d = vInfo.deserializeCode;
            replaceAll(d, VAL, "_opt");
            r.deserializeCode = "func() *" + vInfo.tname + " { if i.ReadBl() { _opt := " + d + "; return &_opt } else { return nil } }()";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "(*_v)");
            r.serializeCode = "if {{_}} != nil { o.WriteBl(true); _v := {{_}}; " + ser + " } else { o.WriteBl(false) }";
            return r;
        }
    }

    // bitset
    if (first == E_type::bitset) {
        MemberInfo r;
        r.tname = "[]bool";
        r.ivalue = "nil";
        r.deserializeCode = "func() []bool { _len := i.ReadSz(); _tmp := make([]bool, _len); for _j := 0; _j < _len; _j++ { _tmp[_j] = i.ReadBl() }; return _tmp }()";
        r.serializeCode = "o.WriteSz(len({{_}})); for _, _v := range {{_}} { o.WriteBl(_v) }";
        return r;
    }

    // atomic
    if (first == E_type::atomic) {
        if (desc.size() >= 2) {
            return interpretGoTypeDescInner(desc.subspan(1), typeMap);
        }
    }

    // array
    if (first == E_type::array) {
        if (desc.size() >= 3) {
            auto sz = desc[1];
            auto vInfo = interpretGoTypeDescInner(desc.subspan(2), typeMap);
            MemberInfo r;
            auto szStr = std::to_string(sz);
            auto retType = "[" + szStr + "]" + vInfo.tname;
            r.tname = retType;
            r.ivalue = "[" + szStr + "]" + vInfo.tname + "{}";
            r.arraySize = static_cast<int>(sz);
            r.arrayElemType = vInfo.tname;
            auto d = vInfo.deserializeCode;
            replaceAll(d, VAL, "_tmp[_j]");
            r.deserializeCode = "func() " + retType + " { var _tmp " + retType + "; for _j := 0; _j < " + szStr + "; _j++ { " + d + " }; return _tmp }()";
            auto ser = vInfo.serializeCode;
            replaceAll(ser, VAL, "_v");
            r.serializeCode = "for _, _v := range {{_}} { " + ser + " }";
            return r;
        }
    }

    // tuple：[tuple, elem1, elem2, ..., ed]
    if (first == E_type::tuple) {
        std::vector<MemberInfo> elemInfos;
        size_t pos = 1;
        while (pos < desc.size() && desc[pos] != E_type::ed) {
            size_t elemLen = getGoTypeDescLength(desc.subspan(pos));
            elemInfos.push_back(interpretGoTypeDescInner(desc.subspan(pos, elemLen), typeMap));
            pos += elemLen;
        }
        std::string typeNames, deserElems, serStmts;
        for (size_t i = 0; i < elemInfos.size(); ++i) {
            if (i > 0) { typeNames += ", "; deserElems += ", "; }
            typeNames += elemInfos[i].tname;
            deserElems += elemInfos[i].deserializeCode;
            auto ser = elemInfos[i].serializeCode;
            if (!ser.empty()) {
                replaceAll(ser, VAL, "{{_}}[" + std::to_string(i) + "]");
                serStmts += ser + "; ";
            }
        }
        MemberInfo r;
        r.tname = "[]interface{}";
        r.ivalue = "nil";
        r.deserializeCode = "[]interface{}{" + deserElems + "}";
        r.serializeCode = serStmts;
        return r;
    }

    // variant：[variant, elem1, elem2, ..., ed]
    if (first == E_type::variant) {
        MemberInfo r;
        r.tname = "interface{}";
        r.ivalue = "nil";
        r.deserializeCode = "nil";
        r.serializeCode = "";
        return r;
    }

    // 基础类型
    if (first == E_type::string) {
        MemberInfo r;
        r.tname = "string";
        r.ivalue = "\"\"";
        r.deserializeCode = "i.ReadString()";
        r.serializeCode = "o.WriteString({{_}})";
        return r;
    }

    if (first == E_type::u8 || first == E_type::u16 || first == E_type::u32 ||
        first == E_type::u64 || first == E_type::i8 || first == E_type::i16 ||
        first == E_type::i32 || first == E_type::i64 || first == E_type::f32 ||
        first == E_type::f64 || first == E_type::bl ||
        first == E_type::ch || first == E_type::ch8 || first == E_type::ch16 || first == E_type::ch32) {
        return getGoBasicTypeInfo(static_cast<Sz>(first));
    }

    if (first == E_type::path) {
        MemberInfo r;
        r.tname = "string";
        r.ivalue = "\"\"";
        r.deserializeCode = "i.ReadString()";
        r.serializeCode = "o.WriteString({{_}})";
        return r;
    }

    if (first == E_type::dur || first == E_type::timepoint) {
        MemberInfo r;
        r.tname = "int64";
        r.ivalue = "0";
        r.deserializeCode = "i.ReadI64()";
        r.serializeCode = "o.WriteI64({{_}})";
        return r;
    }

    return nullInfo;
}

inline MemberInfo interpretGoTypeDesc(std::span<const sp_meta::SpToken> desc,
                                       const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    return interpretGoTypeDescInner(desc, typeMap);
}

// ============== 基于元数据生成 Go 类代码 ==============

inline std::string genGoClassCodeFromMeta(const sp_meta::TypeMeta& typeMeta,
                                           const std::map<uint32_t, const sp_meta::TypeMeta*>& typeMap) {
    std::stringstream ss;
    auto& className = typeMeta.className;
    auto& members = typeMeta.members;

    std::vector<MemberInfo> memberInfos;
    memberInfos.reserve(members.size());
    for (auto& m : members) {
        memberInfos.push_back(interpretGoTypeDesc(m.typeDesc, typeMap));
    }

    ss << "type " << className << " struct {\n";
    for (size_t i = 0; i < members.size(); ++i) {
        ss << "    " << members[i].name << " " << memberInfos[i].tname << "\n";
    }
    ss << "}\n\n";

    ss << "func (x *" << className << ") TypeID() uint32 {\n";
    ss << "    return E_StreamPunkType_" << className << "\n";
    ss << "}\n\n";

    ss << "func (x *" << className << ") From_(i *I) {\n";
    for (size_t i = 0; i < members.size(); ++i) {
        auto& info = memberInfos[i];
        auto code = info.deserializeCode;
        replaceAll(code, VAL, std::string("x.") + members[i].name);
        ss << "    x." << members[i].name << " = " << code << "\n";
    }
    ss << "}\n\n";

    ss << "func (x *" << className << ") To(o *O) {\n";
    for (size_t i = 0; i < members.size(); ++i) {
        auto& info = memberInfos[i];
        if (!info.serializeCode.empty()) {
            auto code = info.serializeCode;
            replaceAll(code, VAL, std::string("x.") + members[i].name);
            ss << "    " << code << "\n";
        }
    }
    ss << "}\n\n";

    return ss.str();
}