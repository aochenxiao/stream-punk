// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT

#pragma once
// ============================================================
// MetaData.hpp — 元数据提取与二进制格式
//
// 用法（提取元数据）:
//   #include <stream-punk/MetaData.hpp>
//   auto bin = getMetaData<MousePosition, MQTT, Test>();  // 返回 vector<uint8_t>
//
// 用法（读取元数据文件）:
//   auto meta = sp_meta::readMetaFile("temp/stream-punk-meta.bin");
//   for (auto& t : meta.types) { ... }
//
// 依赖: StreamPunk.hpp (TypeDesc / getMemberNames / typeID / SpToken)
//       Data.hpp
// ============================================================

#include <sstream>
#include <array>
#include <tuple>
#include <utility>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include "StreamPunk.hpp"

namespace sp_meta {

using sp::SpToken;
using sp::SpDataError;
using sp::TypeDesc;

// ============== 二进制格式常量 ==============

constexpr uint32_t META_MAGIC   = 0x53504D44;  // "SPMD"
constexpr uint32_t META_VERSION = 1;

// ============== 二进制格式数据结构 ==============

struct MemberMeta {
    std::string name;
    std::vector<SpToken> typeDesc;
};

struct TypeMeta {
    uint32_t typeID = 0;
    std::string className;
    std::string baseName;
    std::vector<MemberMeta> members;
};

struct MetaFile {
    std::vector<TypeMeta> types;
};

// ============== 写入 ==============

inline void writeU32(std::ostream& os, uint32_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

inline void writeU16(std::ostream& os, uint16_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

inline void writeStr(std::ostream& os, const std::string& s) {
    writeU16(os, static_cast<uint16_t>(s.size()));
    if (!s.empty()) {
        os.write(s.data(), s.size());
    }
}

inline void writeMetaFile(std::ostream& os, const MetaFile& meta) {
    writeU32(os, META_MAGIC);
    writeU32(os, META_VERSION);
    writeU32(os, static_cast<uint32_t>(meta.types.size()));

    for (auto& t : meta.types) {
        writeU32(os, t.typeID);
        writeStr(os, t.className);
        writeStr(os, t.baseName);
        writeU16(os, static_cast<uint16_t>(t.members.size()));

        for (auto& m : t.members) {
            writeStr(os, m.name);
            writeU16(os, static_cast<uint16_t>(m.typeDesc.size()));
            if (!m.typeDesc.empty()) {
                os.write(reinterpret_cast<const char*>(m.typeDesc.data()),
                         m.typeDesc.size() * sizeof(SpToken));
            }
        }
    }
}

// ============== 读取 ==============

inline uint32_t readU32(std::istream& is) {
    uint32_t v;
    is.read(reinterpret_cast<char*>(&v), sizeof(v));
    if (!is) throw SpDataError("Failed to read u32 from metadata");
    return v;
}

inline uint16_t readU16(std::istream& is) {
    uint16_t v;
    is.read(reinterpret_cast<char*>(&v), sizeof(v));
    if (!is) throw SpDataError("Failed to read u16 from metadata");
    return v;
}

inline std::string readStr(std::istream& is) {
    uint16_t len = readU16(is);
    if (len == 0) return {};
    std::string s(len, '\0');
    is.read(s.data(), len);
    if (!is) throw SpDataError("Failed to read string from metadata");
    return s;
}

inline MetaFile readMetaFile(std::istream& is) {
    uint32_t magic = readU32(is);
    if (magic != META_MAGIC) {
        throw SpDataError("Invalid metadata magic number");
    }
    uint32_t version = readU32(is);
    if (version != META_VERSION) {
        throw SpDataError("Unsupported metadata version: " + std::to_string(version));
    }

    MetaFile meta;
    uint32_t typeCount = readU32(is);
    meta.types.reserve(typeCount);

    for (uint32_t ti = 0; ti < typeCount; ++ti) {
        TypeMeta t;
        t.typeID = readU32(is);
        t.className = readStr(is);
        t.baseName = readStr(is);
        uint16_t memberCount = readU16(is);
        t.members.reserve(memberCount);

        for (uint16_t mi = 0; mi < memberCount; ++mi) {
            MemberMeta m;
            m.name = readStr(is);
            uint16_t descLen = readU16(is);
            if (descLen > 0) {
                m.typeDesc.resize(descLen);
                is.read(reinterpret_cast<char*>(m.typeDesc.data()),
                        descLen * sizeof(SpToken));
                if (!is) throw SpDataError("Failed to read typeDesc from metadata");
            }
            t.members.push_back(std::move(m));
        }
        meta.types.push_back(std::move(t));
    }
    return meta;
}

inline MetaFile readMetaFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        throw SpDataError("Cannot open metadata file: " + path);
    }
    return readMetaFile(ifs);
}

// ============== 类型提取辅助模板 ==============

// 从 TypeDesc<T>::v 获取 SpToken 向量
template<typename T>
std::vector<SpToken> get_type_desc() {
    auto& arr = TypeDesc<T>::v;
    return {arr.begin(), arr.end()};
}

// 从 tuple 类型中提取每个元素的 TypeDesc，返回数组
// 注意：M::TypeList 末尾有一个额外的 E（枚举类型），需要排除
template<typename Tuple, size_t... Is>
std::array<std::vector<SpToken>, sizeof...(Is)>
get_member_type_descs_impl(std::index_sequence<Is...>) {
    return {get_type_desc<std::tuple_element_t<Is, Tuple>>()...};
}

template<typename T>
auto get_member_type_descs() {
    constexpr size_t N = std::tuple_size_v<typename T::M::TypeList> - 1;
    return get_member_type_descs_impl<typename T::M::TypeList>(
        std::make_index_sequence<N>{}
    );
}

// ============== 公共 API ==============

// 添加一个类型的元数据到 MetaFile
template<typename T>
void add_type_meta(MetaFile& meta) {
    T obj;
    TypeMeta tm;
    tm.typeID    = static_cast<SpToken>(obj.typeID());
    tm.className = obj.getClassName();
    tm.baseName  = obj.getBaseName();

    auto memberNames = obj.getMemberNames();
    auto memberDescs = get_member_type_descs<T>();

    for (size_t i = 0; i < memberNames.size(); ++i) {
        tm.members.push_back({std::string(memberNames[i]), memberDescs[i]});
    }

    meta.types.push_back(std::move(tm));
}

// 从多个类型生成元数据，返回二进制数组
template<typename... Types>
std::vector<uint8_t> getMetaData() {
    MetaFile meta;
    (add_type_meta<Types>(meta), ...);
    std::ostringstream oss(std::ios::binary);
    writeMetaFile(oss, meta);
    auto str = std::move(oss).str();
    return {reinterpret_cast<const uint8_t*>(str.data()),
            reinterpret_cast<const uint8_t*>(str.data() + str.size())};
}

} // namespace sp_meta