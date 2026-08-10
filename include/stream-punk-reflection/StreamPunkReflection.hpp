// stream-punk_for_cpp26 - C++26 反射版序列化/反序列化
// 编译: g++-16 -std=c++26 -freflection
//
// 设计目标:
//   1. 零宏 - 不需要 UseData / Xt_* / DH 等宏
//   2. 零侵入 - 不需要继承 Base
//   3. 二进制兼容 - 与现有 StreamPunk 产出完全相同的二进制数据
//
// 当前限制:
//   - GCC 16.0.1 的 nonstatic_data_members_of 不可用 (constexpr vector 分配问题)
//   - 因此成员列表暂时用 SP_REFLECT(...) 手动声明（等 GCC 更新后可移除）
//   - 反射仍用于: 类型名、成员名、[:] splice 访问

#pragma once

#include <string_view>
#include <string>
#include <ostream>
#include <istream>
#include <sstream>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <array>
#include <bitset>
#include <optional>
#include <tuple>
#include <variant>
#include <memory>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <meta>

namespace sp26 {

// ============================== 类型别名（与 sp 保持一致） ==============================

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using Sz  = u32;  // 长度类型

// ============================== 流封装 ==============================

struct OStream {
    std::ostream* os = nullptr;
    explicit OStream(std::ostream& s) : os(&s) {}
};

struct IStream {
    std::istream* is = nullptr;
    explicit IStream(std::istream& s) : is(&s) {}
};

struct SpDataError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ============================== 基础类型序列化（与 sp 格式完全一致） ==============================

inline void write(OStream& o, u8  v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, u16 v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, u32 v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, u64 v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, i8  v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, i16 v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, i32 v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, i64 v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, f32 v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, f64 v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, bool v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, char v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, char8_t v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, char16_t v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, char32_t v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }
inline void write(OStream& o, wchar_t v) { o.os->write(reinterpret_cast<const char*>(&v), sizeof(v)); }

inline void read(IStream& i, u8&  v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read u8 failed"); }
inline void read(IStream& i, u16& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read u16 failed"); }
inline void read(IStream& i, u32& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read u32 failed"); }
inline void read(IStream& i, u64& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read u64 failed"); }
inline void read(IStream& i, i8&  v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read i8 failed"); }
inline void read(IStream& i, i16& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read i16 failed"); }
inline void read(IStream& i, i32& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read i32 failed"); }
inline void read(IStream& i, i64& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read i64 failed"); }
inline void read(IStream& i, f32& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read f32 failed"); }
inline void read(IStream& i, f64& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read f64 failed"); }
inline void read(IStream& i, bool& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read bool failed"); }
inline void read(IStream& i, char& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read char failed"); }
inline void read(IStream& i, char8_t& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read char8_t failed"); }
inline void read(IStream& i, char16_t& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read char16_t failed"); }
inline void read(IStream& i, char32_t& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read char32_t failed"); }
inline void read(IStream& i, wchar_t& v) { i.is->read(reinterpret_cast<char*>(&v), sizeof(v)); if (i.is->fail()) throw SpDataError("read wchar_t failed"); }

// 枚举类型（与 sp 一致：转为底层类型序列化）
template<typename T> requires std::is_enum_v<T>
inline void write(OStream& o, T const& v) {
    write(o, static_cast<std::underlying_type_t<T>>(v));
}
template<typename T> requires std::is_enum_v<T>
inline void read(IStream& i, T& v) {
    std::underlying_type_t<T> tmp;
    read(i, tmp);
    v = static_cast<T>(tmp);
}

// ============================== 容器序列化（与 sp 格式一致：Sz 长度 + 数据） ==============================

// string: Sz(len) + raw chars
inline void write(OStream& o, std::string const& v) {
    write(o, static_cast<Sz>(v.size()));
    o.os->write(v.data(), v.size());
}
inline void read(IStream& i, std::string& v) {
    Sz sz;
    read(i, sz);
    v.resize(sz);
    if (sz > 0) i.is->read(v.data(), sz);
    if (i.is->fail()) throw SpDataError("read string failed");
}

// vector: Sz(len) + elements
template<typename T>
inline void write(OStream& o, std::vector<T> const& v) {
    write(o, static_cast<Sz>(v.size()));
    if constexpr (std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>) {
        o.os->write(reinterpret_cast<const char*>(v.data()), sizeof(T) * v.size());
    } else {
        for (auto const& x : v) write(o, x);
    }
}
template<typename T>
inline void read(IStream& i, std::vector<T>& v) {
    Sz sz;
    read(i, sz);
    v.resize(sz);
    if constexpr (std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>) {
        if (sz > 0) i.is->read(reinterpret_cast<char*>(v.data()), sizeof(T) * sz);
    } else {
        for (auto& x : v) read(i, x);
    }
    if (i.is->fail()) throw SpDataError("read vector failed");
}

// u8string: Sz(len) + raw chars (UTF-8 字节)
inline void write(OStream& o, std::u8string const& v) {
    write(o, static_cast<Sz>(v.size()));
    o.os->write(reinterpret_cast<const char*>(v.data()), v.size());
}
inline void read(IStream& i, std::u8string& v) {
    Sz sz;
    read(i, sz);
    v.resize(sz);
    if (sz > 0) i.is->read(reinterpret_cast<char*>(v.data()), sz);
    if (i.is->fail()) throw SpDataError("read u8string failed");
}

// ============================== 通用序列化容器 ==============================
// 用于 deque, list, forward_list, set, unordered_set, map, unordered_map

// Sz(count) + elements (序列容器)
#define SP26_SEQ_CONTAINER_WRITE(CT, T) \
    write(o, static_cast<Sz>(v.size())); \
    for (auto const& x : v) write(o, x);

#define SP26_SEQ_CONTAINER_READ(CT, T) \
    { Sz sz; read(i, sz); \
      v.clear(); \
      for (Sz n = 0; n < sz; ++n) { \
        T val{}; read(i, val); \
        v.insert(v.end(), std::move(val)); \
      } \
    }

// Sz(count) + elements (关联容器: set-like)
#define SP26_SET_CONTAINER_WRITE(CT, T) \
    write(o, static_cast<Sz>(v.size())); \
    for (auto const& x : v) write(o, x);

#define SP26_SET_CONTAINER_READ(CT, T) \
    { Sz sz; read(i, sz); \
      v.clear(); \
      for (Sz n = 0; n < sz; ++n) { \
        T val{}; read(i, val); \
        v.insert(std::move(val)); \
      } \
    }

// Sz(count) + (key, value) pairs (关联容器: map-like)
#define SP26_MAP_CONTAINER_WRITE(CT, K, V) \
    write(o, static_cast<Sz>(v.size())); \
    for (auto const& [k, val] : v) { write(o, k); write(o, val); }

#define SP26_MAP_CONTAINER_READ(CT, K, V) \
    { Sz sz; read(i, sz); \
      v.clear(); \
      for (Sz n = 0; n < sz; ++n) { \
        K key{}; V val{}; \
        read(i, key); read(i, val); \
        v.emplace(std::move(key), std::move(val)); \
      } \
    }

// ---- deque ----
template<typename T>
inline void write(OStream& o, std::deque<T> const& v) { SP26_SEQ_CONTAINER_WRITE(std::deque<T>, T) }
template<typename T>
inline void read(IStream& i, std::deque<T>& v) { SP26_SEQ_CONTAINER_READ(std::deque<T>, T) }

// ---- list ----
template<typename T>
inline void write(OStream& o, std::list<T> const& v) { SP26_SEQ_CONTAINER_WRITE(std::list<T>, T) }
template<typename T>
inline void read(IStream& i, std::list<T>& v) { SP26_SEQ_CONTAINER_READ(std::list<T>, T) }

// ---- forward_list ----
template<typename T>
inline void write(OStream& o, std::forward_list<T> const& v) {
    Sz cnt = 0;
    for (auto const& x : v) ++cnt;
    write(o, cnt);
    for (auto const& x : v) write(o, x);
}
template<typename T>
inline void read(IStream& i, std::forward_list<T>& v) {
    Sz sz; read(i, sz);
    v.clear();
    auto it = v.before_begin();
    for (Sz n = 0; n < sz; ++n) {
        T val{}; read(i, val);
        it = v.insert_after(it, std::move(val));
    }
}

// ---- set ----
template<typename T>
inline void write(OStream& o, std::set<T> const& v) { SP26_SET_CONTAINER_WRITE(std::set<T>, T) }
template<typename T>
inline void read(IStream& i, std::set<T>& v) { SP26_SET_CONTAINER_READ(std::set<T>, T) }

// ---- unordered_set ----
template<typename T>
inline void write(OStream& o, std::unordered_set<T> const& v) { SP26_SET_CONTAINER_WRITE(std::unordered_set<T>, T) }
template<typename T>
inline void read(IStream& i, std::unordered_set<T>& v) { SP26_SET_CONTAINER_READ(std::unordered_set<T>, T) }

// ---- map ----
template<typename K, typename V>
inline void write(OStream& o, std::map<K,V> const& v) {
    write(o, static_cast<Sz>(v.size()));
    for (auto const& [k, val] : v) { write(o, k); write(o, val); }
}
template<typename K, typename V>
inline void read(IStream& i, std::map<K,V>& v) {
    Sz sz; read(i, sz);
    v.clear();
    for (Sz n = 0; n < sz; ++n) {
        K key{}; V val{};
        read(i, key); read(i, val);
        v.emplace(std::move(key), std::move(val));
    }
}

// ---- unordered_map ----
template<typename K, typename V>
inline void write(OStream& o, std::unordered_map<K,V> const& v) {
    write(o, static_cast<Sz>(v.size()));
    for (auto const& [k, val] : v) { write(o, k); write(o, val); }
}
template<typename K, typename V>
inline void read(IStream& i, std::unordered_map<K,V>& v) {
    Sz sz; read(i, sz);
    v.clear();
    for (Sz n = 0; n < sz; ++n) {
        K key{}; V val{};
        read(i, key); read(i, val);
        v.emplace(std::move(key), std::move(val));
    }
}

// ---- array<T, N> ----
template<typename T, size_t N>
inline void write(OStream& o, std::array<T, N> const& v) {
    if constexpr (std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>) {
        o.os->write(reinterpret_cast<const char*>(v.data()), sizeof(T) * N);
    } else {
        for (auto const& x : v) write(o, x);
    }
}
template<typename T, size_t N>
inline void read(IStream& i, std::array<T, N>& v) {
    if constexpr (std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>) {
        i.is->read(reinterpret_cast<char*>(v.data()), sizeof(T) * N);
    } else {
        for (auto& x : v) read(i, x);
    }
    if (i.is->fail()) throw SpDataError("read array failed");
}

// ---- bitset<N> ----
template<size_t N>
inline void write(OStream& o, std::bitset<N> const& v) {
    constexpr size_t bytes = (N + 7) / 8;
    char buf[bytes] = {};
    for (size_t i = 0; i < N; ++i) {
        if (v[i]) buf[i / 8] |= (1 << (i % 8));
    }
    o.os->write(buf, bytes);
}
template<size_t N>
inline void read(IStream& i, std::bitset<N>& v) {
    constexpr size_t bytes = (N + 7) / 8;
    char buf[bytes] = {};
    i.is->read(buf, bytes);
    v.reset();
    for (size_t j = 0; j < N; ++j) {
        if (buf[j / 8] & (1 << (j % 8))) v.set(j);
    }
    if (i.is->fail()) throw SpDataError("read bitset failed");
}

// ---- optional<T> ----
template<typename T>
inline void write(OStream& o, std::optional<T> const& v) {
    write(o, v.has_value());
    if (v.has_value()) write(o, *v);
}
template<typename T>
inline void read(IStream& i, std::optional<T>& v) {
    bool has;
    read(i, has);
    if (has) {
        T val{};
        read(i, val);
        v = std::move(val);
    } else {
        v = std::nullopt;
    }
}

// ---- tuple<T...> ----
template<typename... Ts>
inline void write(OStream& o, std::tuple<Ts...> const& v) {
    std::apply([&](auto const&... args) {
        ((write(o, args)), ...);
    }, v);
}
template<typename... Ts>
inline void read(IStream& i, std::tuple<Ts...>& v) {
    std::apply([&](auto&... args) {
        ((read(i, args)), ...);
    }, v);
}

// ---- variant<T...> ----
template<typename... Ts>
inline void write(OStream& o, std::variant<Ts...> const& v) {
    write(o, static_cast<Sz>(v.index()));
    std::visit([&](auto const& val) { write(o, val); }, v);
}
template<typename... Ts>
inline void read(IStream& i, std::variant<Ts...>& v) {
    Sz idx;
    read(i, idx);
    read_variant_impl<0, Ts...>(i, v, idx);
}
template<size_t I, typename... Ts>
inline void read_variant_impl(IStream& i, std::variant<Ts...>& v, Sz idx) {
    if constexpr (I < sizeof...(Ts)) {
        if (I == idx) {
            using T = std::variant_alternative_t<I, std::variant<Ts...>>;
            T val{};
            read(i, val);
            v = std::move(val);
        } else {
            read_variant_impl<I+1, Ts...>(i, v, idx);
        }
    } else {
        throw SpDataError("read variant: invalid index");
    }
}

// ---- chrono ----
// 通用 duration 序列化（支持 seconds, milliseconds, microseconds, nanoseconds 等）
template<typename Rep, typename Period>
inline void write(OStream& o, std::chrono::duration<Rep, Period> const& v) {
    write(o, static_cast<i64>(v.count()));
}
template<typename Rep, typename Period>
inline void read(IStream& i, std::chrono::duration<Rep, Period>& v) {
    i64 cnt;
    read(i, cnt);
    v = std::chrono::duration<Rep, Period>(cnt);
}

inline void write(OStream& o, std::chrono::system_clock::time_point const& v) {
    auto dur = v.time_since_epoch();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(dur);
    write(o, static_cast<i64>(ns.count()));
}
inline void read(IStream& i, std::chrono::system_clock::time_point& v) {
    i64 ns;
    read(i, ns);
    v = std::chrono::system_clock::time_point(std::chrono::nanoseconds(ns));
}

// ============================== 指针序列化 ==============================

// raw pointer T*
template<typename T>
inline void write(OStream& o, T* const& v) {
    write(o, v != nullptr);
    if (v) write(o, *v);
}
template<typename T>
inline void read(IStream& i, T*& v) {
    bool has;
    read(i, has);
    if (has) {
        v = new T{};
        read(i, *v);
    } else {
        v = nullptr;
    }
}

// shared_ptr<T>
template<typename T>
inline void write(OStream& o, std::shared_ptr<T> const& v) {
    write(o, v != nullptr);
    if (v) write(o, *v);
}
template<typename T>
inline void read(IStream& i, std::shared_ptr<T>& v) {
    bool has;
    read(i, has);
    if (has) {
        v = std::make_shared<T>();
        read(i, *v);
    } else {
        v = nullptr;
    }
}

// unique_ptr<T>
template<typename T>
inline void write(OStream& o, std::unique_ptr<T> const& v) {
    write(o, v != nullptr);
    if (v) write(o, *v);
}
template<typename T>
inline void read(IStream& i, std::unique_ptr<T>& v) {
    bool has;
    read(i, has);
    if (has) {
        v = std::make_unique<T>();
        read(i, *v);
    } else {
        v = nullptr;
    }
}

// weak_ptr<T> — 序列化时 lock() 为 shared_ptr 再写入
template<typename T>
inline void write(OStream& o, std::weak_ptr<T> const& v) {
    auto sp = v.lock();
    write(o, sp);
}
template<typename T>
inline void read(IStream& i, std::weak_ptr<T>& v) {
    std::shared_ptr<T> sp;
    read(i, sp);
    v = sp;
}

// 通用回退：对自定义结构体类型调用 serialize/deserialize
template<typename T>
inline void write(OStream& o, T const& v) { serialize(o, v); }
template<typename T>
inline void read(IStream& i, T& v) { deserialize(i, v); }

// 获取类型名（使用反射）
template<typename T>
consteval std::string_view type_name() {
    return std::meta::identifier_of(^^T);
}

// ============================== 反射序列化/反序列化 ==============================

// 前向声明
template<typename T> void serialize(OStream& o, T const& v);
template<typename T> void deserialize(IStream& i, T& v);

// 基础类型
template<typename T> requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
inline void serialize(OStream& o, T const& v) { write(o, v); }
template<typename T> requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
inline void deserialize(IStream& i, T& v) { read(i, v); }

// string
inline void serialize(OStream& o, std::string const& v) { write(o, v); }
inline void deserialize(IStream& i, std::string& v) { read(i, v); }

// vector
template<typename T>
inline void serialize(OStream& o, std::vector<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::vector<T>& v) { read(i, v); }

// u8string
inline void serialize(OStream& o, std::u8string const& v) { write(o, v); }
inline void deserialize(IStream& i, std::u8string& v) { read(i, v); }

// deque
template<typename T>
inline void serialize(OStream& o, std::deque<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::deque<T>& v) { read(i, v); }

// list
template<typename T>
inline void serialize(OStream& o, std::list<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::list<T>& v) { read(i, v); }

// forward_list
template<typename T>
inline void serialize(OStream& o, std::forward_list<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::forward_list<T>& v) { read(i, v); }

// set
template<typename T>
inline void serialize(OStream& o, std::set<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::set<T>& v) { read(i, v); }

// unordered_set
template<typename T>
inline void serialize(OStream& o, std::unordered_set<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::unordered_set<T>& v) { read(i, v); }

// map
template<typename K, typename V>
inline void serialize(OStream& o, std::map<K,V> const& v) { write(o, v); }
template<typename K, typename V>
inline void deserialize(IStream& i, std::map<K,V>& v) { read(i, v); }

// unordered_map
template<typename K, typename V>
inline void serialize(OStream& o, std::unordered_map<K,V> const& v) { write(o, v); }
template<typename K, typename V>
inline void deserialize(IStream& i, std::unordered_map<K,V>& v) { read(i, v); }

// array
template<typename T, size_t N>
inline void serialize(OStream& o, std::array<T,N> const& v) { write(o, v); }
template<typename T, size_t N>
inline void deserialize(IStream& i, std::array<T,N>& v) { read(i, v); }

// bitset
template<size_t N>
inline void serialize(OStream& o, std::bitset<N> const& v) { write(o, v); }
template<size_t N>
inline void deserialize(IStream& i, std::bitset<N>& v) { read(i, v); }

// optional
template<typename T>
inline void serialize(OStream& o, std::optional<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::optional<T>& v) { read(i, v); }

// tuple
template<typename... Ts>
inline void serialize(OStream& o, std::tuple<Ts...> const& v) { write(o, v); }
template<typename... Ts>
inline void deserialize(IStream& i, std::tuple<Ts...>& v) { read(i, v); }

// variant
template<typename... Ts>
inline void serialize(OStream& o, std::variant<Ts...> const& v) { write(o, v); }
template<typename... Ts>
inline void deserialize(IStream& i, std::variant<Ts...>& v) { read(i, v); }

// chrono — 通用 duration 序列化
template<typename Rep, typename Period>
inline void serialize(OStream& o, std::chrono::duration<Rep, Period> const& v) { write(o, v); }
template<typename Rep, typename Period>
inline void deserialize(IStream& i, std::chrono::duration<Rep, Period>& v) { read(i, v); }
inline void serialize(OStream& o, std::chrono::system_clock::time_point const& v) { write(o, v); }
inline void deserialize(IStream& i, std::chrono::system_clock::time_point& v) { read(i, v); }

// pointers
template<typename T>
inline void serialize(OStream& o, std::shared_ptr<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::shared_ptr<T>& v) { read(i, v); }
template<typename T>
inline void serialize(OStream& o, std::unique_ptr<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::unique_ptr<T>& v) { read(i, v); }
template<typename T>
inline void serialize(OStream& o, std::weak_ptr<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, std::weak_ptr<T>& v) { read(i, v); }
template<typename T>
inline void serialize(OStream& o, T* const& v) { write(o, v); }
template<typename T>
inline void deserialize(IStream& i, T*& v) { read(i, v); }

// ============================== 便捷函数：序列化到 string / 从 string 反序列化 ==============================

template<typename T>
std::string to_binary(T const& v) {
    std::stringstream ss;
    OStream o(ss);
    serialize(o, v);
    return ss.str();
}

template<typename T>
T from_binary(std::string const& data) {
    std::stringstream ss(data);
    IStream i(ss);
    T v{};
    deserialize(i, v);
    return v;
}

} // namespace sp26

// ============================== SP_REFLECT 宏 ==============================
//
// 用法:
//   SP_REFLECT(Player, id, name, health, pos);
//
// 等 GCC 的 nonstatic_data_members_of 修复后，这个宏可以变成空定义，
// 成员遍历由编译器自动完成。
//
// 当前：使用宏展开为每个成员调用 serialize/deserialize，
// 使用 ^^Type::member 反射获取成员信息，使用 [:info:] splice 访问成员值。

#define SP_REFLECT_SER_ONE(TYPE, MEMBER) \
    sp26::serialize(o, v.[:^^TYPE::MEMBER:]);

#define SP_REFLECT_DESER_ONE(TYPE, MEMBER) \
    sp26::deserialize(i, v.[:^^TYPE::MEMBER:]);

// 生成序列化特化（TYPE 不带命名空间前缀，用户在全局或自定义命名空间定义）
#define SP_REFLECT_SER(TYPE, MEMBERS) \
    template<> inline void sp26::serialize<TYPE>(sp26::OStream& o, TYPE const& v) { \
        MEMBERS \
    }

// 生成反序列化特化
#define SP_REFLECT_DESER(TYPE, MEMBERS) \
    template<> inline void sp26::deserialize<TYPE>(sp26::IStream& i, TYPE& v) { \
        MEMBERS \
    }

// 主宏
#define SP_REFLECT(TypeName, ...) \
    SP_REFLECT_SER(TypeName, SP_REFLECT_FOR_EACH_SER(TypeName, __VA_ARGS__)) \
    SP_REFLECT_DESER(TypeName, SP_REFLECT_FOR_EACH_DESER(TypeName, __VA_ARGS__))

// 对每个成员生成序列化代码
#define SP_REFLECT_FOR_EACH_SER(TYPE, ...) \
    SP_REFLECT_FOR_EACH_IMPL(SP_REFLECT_SER_ONE, TYPE, __VA_ARGS__)

#define SP_REFLECT_FOR_EACH_DESER(TYPE, ...) \
    SP_REFLECT_FOR_EACH_IMPL(SP_REFLECT_DESER_ONE, TYPE, __VA_ARGS__)

// 变参展开辅助
#define SP_REFLECT_FOR_EACH_IMPL(MACRO, TYPE, ...) \
    SP_REFLECT_EXPAND(SP_REFLECT_FOR_EACH_(MACRO, TYPE, __VA_ARGS__))

#define SP_REFLECT_FOR_EACH_(MACRO, TYPE, ...) \
    SP_REFLECT_FOR_EACH_N(SP_REFLECT_NARG(__VA_ARGS__), MACRO, TYPE, __VA_ARGS__)

#define SP_REFLECT_FOR_EACH_N(N, MACRO, TYPE, ...) \
    SP_REFLECT_CONCAT(SP_REFLECT_FOR_EACH_, N)(MACRO, TYPE, __VA_ARGS__)

#define SP_REFLECT_FOR_EACH_1(M, T, A)     M(T, A)
#define SP_REFLECT_FOR_EACH_2(M, T, A, B)  M(T, A) M(T, B)
#define SP_REFLECT_FOR_EACH_3(M, T, A, B, C) M(T, A) M(T, B) M(T, C)
#define SP_REFLECT_FOR_EACH_4(M, T, A, B, C, D) M(T, A) M(T, B) M(T, C) M(T, D)
#define SP_REFLECT_FOR_EACH_5(M, T, A, B, C, D, E) M(T, A) M(T, B) M(T, C) M(T, D) M(T, E)
#define SP_REFLECT_FOR_EACH_6(M, T, A, B, C, D, E, F) M(T, A) M(T, B) M(T, C) M(T, D) M(T, E) M(T, F)
#define SP_REFLECT_FOR_EACH_7(M, T, A, B, C, D, E, F, G) M(T, A) M(T, B) M(T, C) M(T, D) M(T, E) M(T, F) M(T, G)
#define SP_REFLECT_FOR_EACH_8(M, T, A, B, C, D, E, F, G, H) M(T, A) M(T, B) M(T, C) M(T, D) M(T, E) M(T, F) M(T, G) M(T, H)
#define SP_REFLECT_FOR_EACH_9(M, T, A, B, C, D, E, F, G, H, I) \
    SP_REFLECT_FOR_EACH_8(M, T, A, B, C, D, E, F, G, H) M(T, I)
#define SP_REFLECT_FOR_EACH_10(M, T, A, B, C, D, E, F, G, H, I, J) \
    SP_REFLECT_FOR_EACH_9(M, T, A, B, C, D, E, F, G, H, I) M(T, J)
#define SP_REFLECT_FOR_EACH_11(M, T, A, B, C, D, E, F, G, H, I, J, K) \
    SP_REFLECT_FOR_EACH_10(M, T, A, B, C, D, E, F, G, H, I, J) M(T, K)
#define SP_REFLECT_FOR_EACH_12(M, T, A, B, C, D, E, F, G, H, I, J, K, L) \
    SP_REFLECT_FOR_EACH_11(M, T, A, B, C, D, E, F, G, H, I, J, K) M(T, L)
#define SP_REFLECT_FOR_EACH_13(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_) \
    SP_REFLECT_FOR_EACH_12(M, T, A, B, C, D, E, F, G, H, I, J, K, L) M(T, M_)
#define SP_REFLECT_FOR_EACH_14(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_) \
    SP_REFLECT_FOR_EACH_13(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_) M(T, N_)
#define SP_REFLECT_FOR_EACH_15(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_) \
    SP_REFLECT_FOR_EACH_14(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_) M(T, O_)
#define SP_REFLECT_FOR_EACH_16(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_, P_) \
    SP_REFLECT_FOR_EACH_15(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_) M(T, P_)
#define SP_REFLECT_FOR_EACH_17(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_, P_, Q_) \
    SP_REFLECT_FOR_EACH_16(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_, P_) M(T, Q_)
#define SP_REFLECT_FOR_EACH_18(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_, P_, Q_, R_) \
    SP_REFLECT_FOR_EACH_17(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_, P_, Q_) M(T, R_)
#define SP_REFLECT_FOR_EACH_19(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_, P_, Q_, R_, S_) \
    SP_REFLECT_FOR_EACH_18(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_, P_, Q_, R_) M(T, S_)
#define SP_REFLECT_FOR_EACH_20(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_, P_, Q_, R_, S_, T_) \
    SP_REFLECT_FOR_EACH_19(M, T, A, B, C, D, E, F, G, H, I, J, K, L, M_, N_, O_, P_, Q_, R_, S_) M(T, T_)

// 参数计数 (支持 1-20)
#define SP_REFLECT_NARG(...) SP_REFLECT_NARG_(__VA_ARGS__, SP_REFLECT_RSEQ_N())
#define SP_REFLECT_NARG_(...) SP_REFLECT_ARG_N(__VA_ARGS__)
#define SP_REFLECT_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, N, ...) N
#define SP_REFLECT_RSEQ_N() 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1

// 辅助宏
#define SP_REFLECT_CONCAT_(a, b) a##b
#define SP_REFLECT_CONCAT(a, b) SP_REFLECT_CONCAT_(a, b)
#define SP_REFLECT_EXPAND(x) x