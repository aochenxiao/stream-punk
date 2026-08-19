// stream-punk_for_cpp26 - C++26 反射版序列化/反序列化
// 编译: g++-16 -std=c++26 -freflection
//
// 设计目标:
//   1. 零宏 - 不需要 UseData / Xt_* / DH 等宏
//   2. 零侵入 - 不需要继承 Base
//   3. 序列化格式兼容 - 与现有 StreamPunk 产出相同的序列化格式
//
// 当前限制:
//   - GCC 16.0.1 的 nonstatic_data_members_of 不可用 (constexpr vector 分配问题)
//   - 因此成员列表暂时用 SP_REFLECT(...) 手动声明（等 GCC 更新后可移除）
//   - 反射仍用于: 类型名、成员名、[:] splice 访问、attribute 读取

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
#include <atomic>
#include <filesystem>
#include <span>
#include <cstring>
#include <stdexcept>
#include <functional>
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

template<typename T> using Sptr = std::shared_ptr<T>;
template<typename T> using Wptr = std::weak_ptr<T>;
template<typename T> using Uptr = std::unique_ptr<T>;

using Imax = i64;
using Umax = u64;
using SpToken = Sz;
template<size_t N> using SpTokenArr = std::array<SpToken, N>;

// 用来放入流当中的指针类型，长度统一为64位
using PtrValue = u64;

// ============================== 异常类型 ==============================

struct StreamPunkError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct SpVersionError : StreamPunkError { using StreamPunkError::StreamPunkError; };
struct SpTypeError : StreamPunkError { using StreamPunkError::StreamPunkError; };
struct SpDataError : StreamPunkError { using StreamPunkError::StreamPunkError; };
struct SpUninitializedError : StreamPunkError { using StreamPunkError::StreamPunkError; };

// ============================== 版本号 ==============================

inline constexpr u32 makeVersion(u8 major, u8 minor = 0, u8 patch = 0, u8 custom = 0) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return (static_cast<u32>(major) << 24) | (static_cast<u32>(minor) << 16)
             | (static_cast<u32>(patch) << 8)  | static_cast<u32>(custom);
    } else {
        return (static_cast<u32>(custom) << 24) | (static_cast<u32>(patch) << 16)
             | (static_cast<u32>(minor) << 8)   | static_cast<u32>(major);
    }
}
inline constexpr u8 getVerMajor(u32 v) noexcept {
    if constexpr (std::endian::native == std::endian::little) return static_cast<u8>(v >> 24);
    else return static_cast<u8>(v & 0xFF);
}
inline constexpr u8 getVerMinor(u32 v) noexcept {
    if constexpr (std::endian::native == std::endian::little) return static_cast<u8>(v >> 16);
    else return static_cast<u8>((v >> 8) & 0xFF);
}
inline constexpr u8 getVerPatch(u32 v) noexcept {
    if constexpr (std::endian::native == std::endian::little) return static_cast<u8>(v >> 8);
    else return static_cast<u8>((v >> 16) & 0xFF);
}
inline constexpr u8 getVerCustom(u32 v) noexcept {
    if constexpr (std::endian::native == std::endian::little) return static_cast<u8>(v & 0xFF);
    else return static_cast<u8>(v >> 24);
}
inline constexpr u32 StreamPunkVer = makeVersion(0, 9, 0);

// ============================== StreamPunkTime（128位时间精度） ==============================

struct StreamPunkTime {
    using AttoSec = std::chrono::duration<i64, std::atto>;

    i64 sec      = 0;
    i64 attoSec  = 0;

    // 将任意的 chrono::duration 拆成「整数秒 + 余下 atto 秒」两部分，
    // 与参考版 StreamPunk.hpp 的 StreamPunkTime::set 保持一致（128 位精度）。
    template<typename Rep, typename Period>
    void set(std::chrono::duration<Rep, Period> const& v) {
        using namespace std::chrono;
        auto sec_part = duration_cast<seconds>(v);
        sec = sec_part.count();
        auto rem = v - sec_part;
        attoSec = duration_cast<AttoSec>(rem).count();
    }

    // 从「整数秒 + atto 秒」还原为任意的 chrono::duration。
    template<typename Rep, typename Period>
    void get(std::chrono::duration<Rep, Period>& v) const {
        using TargetDuration = std::chrono::duration<Rep, Period>;
        auto secs_as_target = std::chrono::duration_cast<TargetDuration>(std::chrono::seconds(sec));
        v = secs_as_target;
        // 整数 Rep 且目标单位比秒还粗：无需再算 atto 部分
        if constexpr (std::is_integral_v<Rep> && std::ratio_greater<Period, std::ratio<1>>::value) {
            return;
        }
        // 整数 Rep 且目标单位细于秒：截断还原
        else if constexpr (std::is_integral_v<Rep>) {
            auto attos_as_target = std::chrono::duration_cast<TargetDuration>(AttoSec(attoSec));
            v += attos_as_target;
        }
        // 浮点 Rep：按 atto 精度四舍五入还原
        else {
            constexpr double target_period = static_cast<double>(Period::num) / Period::den;
            constexpr double ratio = target_period > 1e-18 ? (1e-18 / target_period) : 0;
            double value_in_target_units = static_cast<double>(attoSec) * ratio;
            Rep rounded_value = static_cast<Rep>(value_in_target_units);
            if (rounded_value == 0 && attoSec > 0 && value_in_target_units > 0) {
                rounded_value = 1;
            } else if (rounded_value == 0 && attoSec < 0 && value_in_target_units < 0) {
                rounded_value = -1;
            }
            v += TargetDuration(rounded_value);
        }
    }
};

// ============================== 深拷贝基础设施 ==============================

struct PtrRefInfo {
    Sz refCount = 0;
    enum Kind { e_raw, e_uptr, e_sptr } kind = e_raw;
    void* rawPtr = nullptr;
    std::shared_ptr<void> sptr;
};
using DeepCopier = std::unordered_map<void*, PtrRefInfo>;

// ============================== 抽象 IO 接口 ==============================

struct SpWriter {
    virtual void write(const void* data, size_t size) = 0;
    virtual ~SpWriter() = default;
};

struct SpReader {
    virtual void read(void* data, size_t size) = 0;
    virtual ~SpReader() = default;
};

struct SpStreamWriter final : SpWriter {
    std::ostream& os;
    explicit SpStreamWriter(std::ostream& s) : os(s) {}
    void write(const void* data, size_t size) override {
        os.write(static_cast<const char*>(data), size);
    }
};

struct SpStreamReader final : SpReader {
    std::istream& is;
    explicit SpStreamReader(std::istream& s) : is(s) {}
    void read(void* data, size_t size) override {
        is.read(static_cast<char*>(data), size);
        if (!is.good()) {
            throw SpDataError("read failed: unexpected EOF or stream error");
        }
    }
};

// ============================== O / I 流对象（含指针跟踪上下文） ==============================

struct O {
    std::unique_ptr<SpStreamWriter> _ownedWriter;
    SpWriter* w = nullptr;
    std::unordered_set<PtrValue> ptrSet;  // 已序列化的指针地址，避免重复序列化

    explicit O(std::ostream& os)
        : _ownedWriter(std::make_unique<SpStreamWriter>(os))
        , w(_ownedWriter.get()) {}
    explicit O(SpWriter& writer) : w(&writer) {}

    void clear() { ptrSet.clear(); }
};

struct I {
    std::unique_ptr<SpStreamReader> _ownedReader;
    SpReader* r = nullptr;
    std::unordered_map<PtrValue, Sptr<void>> sptrSet;  // shared_ptr 反序列化复用
    std::unordered_map<PtrValue, void*>  ptrSet;       // raw/unique_ptr 反序列化复用

    explicit I(std::istream& is)
        : _ownedReader(std::make_unique<SpStreamReader>(is))
        , r(_ownedReader.get()) {}
    explicit I(SpReader& reader) : r(&reader) {}

    void clear() {
        sptrSet.clear();
        ptrSet.clear();
    }
};

// ============================== 基础类型 write/read ==============================

#define SP26_WRITE_PRIMITIVE(T) \
    inline void write(O& o, T v) { o.w->write(&v, sizeof(v)); }
#define SP26_READ_PRIMITIVE(T) \
    inline void read(I& i, T& v) { i.r->read(&v, sizeof(v)); }

SP26_WRITE_PRIMITIVE(u8)
SP26_WRITE_PRIMITIVE(u16)
SP26_WRITE_PRIMITIVE(u32)
SP26_WRITE_PRIMITIVE(u64)
SP26_WRITE_PRIMITIVE(i8)
SP26_WRITE_PRIMITIVE(i16)
SP26_WRITE_PRIMITIVE(i32)
SP26_WRITE_PRIMITIVE(i64)
SP26_WRITE_PRIMITIVE(f32)
SP26_WRITE_PRIMITIVE(f64)
SP26_WRITE_PRIMITIVE(bool)
SP26_WRITE_PRIMITIVE(char)
SP26_WRITE_PRIMITIVE(char8_t)
SP26_WRITE_PRIMITIVE(char16_t)
SP26_WRITE_PRIMITIVE(char32_t)
SP26_WRITE_PRIMITIVE(wchar_t)

SP26_READ_PRIMITIVE(u8)
SP26_READ_PRIMITIVE(u16)
SP26_READ_PRIMITIVE(u32)
SP26_READ_PRIMITIVE(u64)
SP26_READ_PRIMITIVE(i8)
SP26_READ_PRIMITIVE(i16)
SP26_READ_PRIMITIVE(i32)
SP26_READ_PRIMITIVE(i64)
SP26_READ_PRIMITIVE(f32)
SP26_READ_PRIMITIVE(f64)
SP26_READ_PRIMITIVE(bool)
SP26_READ_PRIMITIVE(char)
SP26_READ_PRIMITIVE(char8_t)
SP26_READ_PRIMITIVE(char16_t)
SP26_READ_PRIMITIVE(char32_t)
SP26_READ_PRIMITIVE(wchar_t)

#undef SP26_WRITE_PRIMITIVE
#undef SP26_READ_PRIMITIVE

// StreamPunkTime（依赖基础类型 write/read）
inline void write(O& o, StreamPunkTime const& v) { write(o, v.sec); write(o, v.attoSec); }
inline void read(I& i, StreamPunkTime& v) { read(i, v.sec); read(i, v.attoSec); }

// 枚举类型（转为底层类型序列化）
template<typename T> requires std::is_enum_v<T>
inline void write(O& o, T const& v) {
    write(o, static_cast<std::underlying_type_t<T>>(v));
}
template<typename T> requires std::is_enum_v<T>
inline void read(I& i, T& v) {
    std::underlying_type_t<T> tmp;
    read(i, tmp);
    v = static_cast<T>(tmp);
}

// ============================== 容器序列化（Sz 长度 + 数据） ==============================

// string: Sz(len) + raw chars
inline void write(O& o, std::string const& v) {
    write(o, static_cast<Sz>(v.size()));
    o.w->write(v.data(), v.size());
}
inline void read(I& i, std::string& v) {
    Sz sz;
    read(i, sz);
    v.resize(sz);
    if (sz > 0) i.r->read(v.data(), sz);
}

// u8string: Sz(len) + raw chars
inline void write(O& o, std::u8string const& v) {
    write(o, static_cast<Sz>(v.size()));
    o.w->write(reinterpret_cast<const char*>(v.data()), v.size());
}
inline void read(I& i, std::u8string& v) {
    Sz sz;
    read(i, sz);
    v.resize(sz);
    if (sz > 0) i.r->read(reinterpret_cast<char*>(v.data()), sz);
}

// wstring: Sz(len in chars) + raw bytes
inline void write(O& o, std::wstring const& v) {
    write(o, static_cast<Sz>(v.size()));
    o.w->write(reinterpret_cast<const char*>(v.data()), v.size() * sizeof(wchar_t));
}
inline void read(I& i, std::wstring& v) {
    Sz sz;
    read(i, sz);
    v.resize(sz);
    if (sz > 0) i.r->read(reinterpret_cast<char*>(v.data()), sz * sizeof(wchar_t));
}

// u16string
inline void write(O& o, std::u16string const& v) {
    write(o, static_cast<Sz>(v.size()));
    o.w->write(reinterpret_cast<const char*>(v.data()), v.size() * sizeof(char16_t));
}
inline void read(I& i, std::u16string& v) {
    Sz sz;
    read(i, sz);
    v.resize(sz);
    if (sz > 0) i.r->read(reinterpret_cast<char*>(v.data()), sz * sizeof(char16_t));
}

// u32string
inline void write(O& o, std::u32string const& v) {
    write(o, static_cast<Sz>(v.size()));
    o.w->write(reinterpret_cast<const char*>(v.data()), v.size() * sizeof(char32_t));
}
inline void read(I& i, std::u32string& v) {
    Sz sz;
    read(i, sz);
    v.resize(sz);
    if (sz > 0) i.r->read(reinterpret_cast<char*>(v.data()), sz * sizeof(char32_t));
}

// vector: Sz(len) + elements
template<typename T, typename Alloc>
inline void write(O& o, std::vector<T, Alloc> const& v) {
    write(o, static_cast<Sz>(v.size()));
    if constexpr (std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>) {
        o.w->write(reinterpret_cast<const char*>(v.data()), sizeof(T) * v.size());
    } else {
        for (auto const& x : v) serialize(o, x);
    }
}
template<typename T, typename Alloc>
inline void read(I& i, std::vector<T, Alloc>& v) {
    Sz sz;
    read(i, sz);
    v.resize(sz);
    if constexpr (std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>) {
        if (sz > 0) i.r->read(reinterpret_cast<char*>(v.data()), sizeof(T) * sz);
    } else {
        for (auto& x : v) deserialize(i, x);
    }
}

// vector<bool> 特化：其代理迭代器 std::_Bit_reference 无法绑定到 T&，
// 主模板的 deserialize(i, x) 循环会编译失败，这里逐位读写绕过代理。
template<typename Alloc>
inline void write(O& o, std::vector<bool, Alloc> const& v) {
    write(o, static_cast<Sz>(v.size()));
    for (bool b : v) write(o, b);
}
template<typename Alloc>
inline void read(I& i, std::vector<bool, Alloc>& v) {
    Sz sz;
    read(i, sz);
    v.assign(sz, false);
    for (Sz n = 0; n < sz; ++n) {
        bool b = false;
        read(i, b);
        v[n] = b;
    }
}

// ---- deque ----
template<typename T>
inline void write(O& o, std::deque<T> const& v) {
    write(o, static_cast<Sz>(v.size()));
    for (auto const& x : v) serialize(o, x);
}
template<typename T>
inline void read(I& i, std::deque<T>& v) {
    Sz sz; read(i, sz);
    v.clear();
    for (Sz n = 0; n < sz; ++n) {
        T val{}; deserialize(i, val);
        v.insert(v.end(), std::move(val));
    }
}

// ---- list ----
template<typename T>
inline void write(O& o, std::list<T> const& v) {
    write(o, static_cast<Sz>(v.size()));
    for (auto const& x : v) serialize(o, x);
}
template<typename T>
inline void read(I& i, std::list<T>& v) {
    Sz sz; read(i, sz);
    v.clear();
    for (Sz n = 0; n < sz; ++n) {
        T val{}; deserialize(i, val);
        v.insert(v.end(), std::move(val));
    }
}

// ---- forward_list ----
template<typename T>
inline void write(O& o, std::forward_list<T> const& v) {
    Sz cnt = 0;
    for (auto const& x : v) ++cnt;
    write(o, cnt);
    for (auto const& x : v) serialize(o, x);
}
template<typename T>
inline void read(I& i, std::forward_list<T>& v) {
    Sz sz; read(i, sz);
    v.clear();
    auto it = v.before_begin();
    for (Sz n = 0; n < sz; ++n) {
        T val{}; deserialize(i, val);
        it = v.insert_after(it, std::move(val));
    }
}

// ---- set ----
template<typename T>
inline void write(O& o, std::set<T> const& v) {
    write(o, static_cast<Sz>(v.size()));
    for (auto const& x : v) serialize(o, x);
}
template<typename T>
inline void read(I& i, std::set<T>& v) {
    Sz sz; read(i, sz);
    v.clear();
    for (Sz n = 0; n < sz; ++n) {
        T val{}; deserialize(i, val);
        v.insert(std::move(val));
    }
}

// ---- unordered_set ----
template<typename T>
inline void write(O& o, std::unordered_set<T> const& v) {
    write(o, static_cast<Sz>(v.size()));
    for (auto const& x : v) serialize(o, x);
}
template<typename T>
inline void read(I& i, std::unordered_set<T>& v) {
    Sz sz; read(i, sz);
    v.clear();
    for (Sz n = 0; n < sz; ++n) {
        T val{}; deserialize(i, val);
        v.insert(std::move(val));
    }
}

// ---- map ----
template<typename K, typename V>
inline void write(O& o, std::map<K,V> const& v) {
    write(o, static_cast<Sz>(v.size()));
    for (auto const& [k, val] : v) { serialize(o, k); serialize(o, val); }
}
template<typename K, typename V>
inline void read(I& i, std::map<K,V>& v) {
    Sz sz; read(i, sz);
    v.clear();
    for (Sz n = 0; n < sz; ++n) {
        K key{}; V val{};
        deserialize(i, key); deserialize(i, val);
        v.emplace(std::move(key), std::move(val));
    }
}

// ---- unordered_map ----
template<typename K, typename V>
inline void write(O& o, std::unordered_map<K,V> const& v) {
    write(o, static_cast<Sz>(v.size()));
    for (auto const& [k, val] : v) { serialize(o, k); serialize(o, val); }
}
template<typename K, typename V>
inline void read(I& i, std::unordered_map<K,V>& v) {
    Sz sz; read(i, sz);
    v.clear();
    for (Sz n = 0; n < sz; ++n) {
        K key{}; V val{};
        deserialize(i, key); deserialize(i, val);
        v.emplace(std::move(key), std::move(val));
    }
}

// ---- array<T, N> ----
template<typename T, size_t N>
inline void write(O& o, std::array<T, N> const& v) {
    if constexpr (std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>) {
        o.w->write(reinterpret_cast<const char*>(v.data()), sizeof(T) * N);
    } else {
        for (auto const& x : v) serialize(o, x);
    }
}
template<typename T, size_t N>
inline void read(I& i, std::array<T, N>& v) {
    if constexpr (std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>) {
        i.r->read(reinterpret_cast<char*>(v.data()), sizeof(T) * N);
    } else {
        for (auto& x : v) deserialize(i, x);
    }
}

// ---- bitset<N> ----
template<size_t N>
inline void write(O& o, std::bitset<N> const& v) {
    constexpr size_t bytes = (N + 7) / 8;
    char buf[bytes] = {};
    for (size_t i = 0; i < N; ++i) {
        if (v[i]) buf[i / 8] |= (1 << (i % 8));
    }
    o.w->write(buf, bytes);
}
template<size_t N>
inline void read(I& i, std::bitset<N>& v) {
    constexpr size_t bytes = (N + 7) / 8;
    char buf[bytes] = {};
    i.r->read(buf, bytes);
    v.reset();
    for (size_t j = 0; j < N; ++j) {
        if (buf[j / 8] & (1 << (j % 8))) v.set(j);
    }
}

// ---- optional<T> ----
template<typename T>
inline void write(O& o, std::optional<T> const& v) {
    write(o, v.has_value());
    if (v.has_value()) serialize(o, *v);
}
template<typename T>
inline void read(I& i, std::optional<T>& v) {
    bool has;
    read(i, has);
    if (has) {
        T val{};
        deserialize(i, val);
        v = std::move(val);
    } else {
        v = std::nullopt;
    }
}

// ---- tuple<T...> ----
template<typename... Ts>
inline void write(O& o, std::tuple<Ts...> const& v) {
    std::apply([&](auto const&... args) { ((serialize(o, args)), ...); }, v);
}
template<typename... Ts>
inline void read(I& i, std::tuple<Ts...>& v) {
    std::apply([&](auto&... args) { ((deserialize(i, args)), ...); }, v);
}

// ---- variant<T...> ----
template<typename... Ts>
inline void write(O& o, std::variant<Ts...> const& v) {
    write(o, static_cast<Sz>(v.index()));
    std::visit([&](auto const& val) { serialize(o, val); }, v);
}
namespace detail {
template<size_t Idx, typename... Ts>
inline void read_variant(I& i, std::variant<Ts...>& v, Sz idx) {
    if constexpr (Idx < sizeof...(Ts)) {
        if (Idx == idx) {
            using T = std::variant_alternative_t<Idx, std::variant<Ts...>>;
            T val{};
            deserialize(i, val);
            v = std::move(val);
        } else {
            read_variant<Idx+1, Ts...>(i, v, idx);
        }
    } else {
        throw SpDataError("read variant: invalid index");
    }
}
} // namespace detail
template<typename... Ts>
inline void read(I& i, std::variant<Ts...>& v) {
    Sz idx;
    read(i, idx);
    detail::read_variant<0, Ts...>(i, v, idx);
}

// ---- std::monostate（variant 的默认空替代类型，无任何数据） ----
inline void write(O& o, std::monostate const&) {}
inline void read(I& i, std::monostate&) {}

// ---- chrono: duration（128 位 StreamPunkTime，与参考版二进制格式一致） ----
template<typename Rep, typename Period>
inline void write(O& o, std::chrono::duration<Rep, Period> const& v) {
    StreamPunkTime t;
    t.set(v);
    write(o, t);
}
template<typename Rep, typename Period>
inline void read(I& i, std::chrono::duration<Rep, Period>& v) {
    StreamPunkTime t;
    read(i, t);
    t.get(v);
}

// ---- chrono: time_point（泛化到任意 Clock，不再只支持 system_clock） ----
template<typename Clock, typename Duration>
inline void write(O& o, std::chrono::time_point<Clock, Duration> const& v) {
    write(o, v.time_since_epoch());
}
template<typename Clock, typename Duration>
inline void read(I& i, std::chrono::time_point<Clock, Duration>& v) {
    Duration dur{};
    read(i, dur);
    v = std::chrono::time_point<Clock, Duration>(dur);
}

// ---- atomic<T> ----
template<typename T>
inline void write(O& o, std::atomic<T> const& v) {
    write(o, v.load(std::memory_order_acquire));
}
template<typename T>
inline void read(I& i, std::atomic<T>& v) {
    T tmp{};
    read(i, tmp);
    v.store(tmp, std::memory_order_release);
}

// ---- atomic_ref<T> ----
template<typename T>
inline void write(O& o, std::atomic_ref<T> const& v) {
    write(o, v.load(std::memory_order_acquire));
}
template<typename T>
inline void read(I& i, std::atomic_ref<T>& v) {
    T tmp{};
    read(i, tmp);
    v.store(tmp, std::memory_order_release);
}

// ---- filesystem::path ----
inline void write(O& o, std::filesystem::path const& v) {
    write(o, v.u8string());
}
inline void read(I& i, std::filesystem::path& v) {
    std::u8string u8str;
    read(i, u8str);
    v = u8str;
}

// ---- string_view (仅输出) ----
inline void write(O& o, std::string_view const& v) {
    write(o, static_cast<Sz>(v.size()));
    o.w->write(v.data(), v.size());
}

// ---- span<T> (仅输出) ----
template<typename T>
inline void write(O& o, std::span<T> const& v) {
    write(o, static_cast<Sz>(v.size()));
    if constexpr (std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>) {
        o.w->write(reinterpret_cast<const char*>(v.data()), sizeof(T) * v.size());
    } else {
        for (auto const& x : v) write(o, x);
    }
}

// ---- writeSpan / readSpan 泛型辅助 ----
namespace detail {
    template<typename ValueType, typename T>
    inline void writeSpan(O& o, T const& v) {
        write(o, static_cast<Sz>(std::size(v)));
        if constexpr (std::is_trivially_copyable_v<ValueType> && !std::is_same_v<ValueType, bool>) {
            o.w->write(reinterpret_cast<const char*>(std::data(v)), sizeof(ValueType) * std::size(v));
        } else {
            for (auto const& x : v) write(o, x);
        }
    }
    template<typename ValueType, typename T>
    inline void readSpan(I& i, T& v) {
        Sz sz; read(i, sz);
        v.clear();
        if constexpr (std::is_trivially_copyable_v<ValueType> && !std::is_same_v<ValueType, bool>) {
            v.resize(sz);
            if (sz > 0) i.r->read(reinterpret_cast<char*>(std::data(v)), sizeof(ValueType) * sz);
        } else {
            for (Sz n = 0; n < sz; ++n) {
                ValueType val{}; read(i, val);
                v.insert(v.end(), std::move(val));
            }
        }
    }
} // namespace detail

// ---- initializer_list<T> (仅输出) ----
template<typename T>
inline void write(O& o, std::initializer_list<T> const& v) {
    detail::writeSpan<T>(o, v);
}

// ============================== 类型特征 ==============================

template<typename T> struct is_vector_impl : std::false_type {};
template<typename T, typename A> struct is_vector_impl<std::vector<T, A>> : std::true_type {};
template<typename T> inline constexpr bool is_vector_v = is_vector_impl<T>::value;

template<typename T> struct is_deque_impl : std::false_type {};
template<typename T, typename A> struct is_deque_impl<std::deque<T, A>> : std::true_type {};
template<typename T> inline constexpr bool is_deque_v = is_deque_impl<T>::value;

template<typename T> struct is_list_impl : std::false_type {};
template<typename T, typename A> struct is_list_impl<std::list<T, A>> : std::true_type {};
template<typename T> inline constexpr bool is_list_v = is_list_impl<T>::value;

template<typename T> struct is_flist_impl : std::false_type {};
template<typename T, typename A> struct is_flist_impl<std::forward_list<T, A>> : std::true_type {};
template<typename T> inline constexpr bool is_flist_v = is_flist_impl<T>::value;

template<typename T> struct is_set_impl : std::false_type {};
template<typename T, typename C, typename A> struct is_set_impl<std::set<T, C, A>> : std::true_type {};
template<typename T> inline constexpr bool is_set_v = is_set_impl<T>::value;

template<typename T> struct is_uset_impl : std::false_type {};
template<typename T, typename H, typename E, typename A> struct is_uset_impl<std::unordered_set<T, H, E, A>> : std::true_type {};
template<typename T> inline constexpr bool is_uset_v = is_uset_impl<T>::value;

template<typename T> struct is_map_impl : std::false_type {};
template<typename K, typename V, typename C, typename A> struct is_map_impl<std::map<K, V, C, A>> : std::true_type {};
template<typename T> inline constexpr bool is_map_v = is_map_impl<T>::value;

template<typename T> struct is_umap_impl : std::false_type {};
template<typename K, typename V, typename H, typename E, typename A> struct is_umap_impl<std::unordered_map<K, V, H, E, A>> : std::true_type {};
template<typename T> inline constexpr bool is_umap_v = is_umap_impl<T>::value;

template<typename T> struct is_optional_impl : std::false_type {};
template<typename T> struct is_optional_impl<std::optional<T>> : std::true_type {};
template<typename T> inline constexpr bool is_optional_v = is_optional_impl<std::decay_t<T>>::value;

template<typename T> struct is_sptr_impl : std::false_type {};
template<typename T> struct is_sptr_impl<std::shared_ptr<T>> : std::true_type {};
template<typename T> inline constexpr bool is_sptr_v = is_sptr_impl<T>::value;

template<typename T> struct is_uptr_impl : std::false_type {};
template<typename T> struct is_uptr_impl<std::unique_ptr<T>> : std::true_type {};
template<typename T> inline constexpr bool is_uptr_v = is_uptr_impl<T>::value;

template<typename T> struct is_string_impl : std::false_type {};
template<typename T> struct is_string_impl<std::basic_string<T>> : std::true_type {};
template<typename T> inline constexpr bool is_string_v = is_string_impl<T>::value;

template<typename T> inline constexpr bool is_ordered_container_v = is_vector_v<T> || is_deque_v<T> || is_list_v<T>;
template<typename T> inline constexpr bool is_assoc_container_v = is_map_v<T> || is_umap_v<T>;
template<typename T> inline constexpr bool is_set_container_v = is_set_v<T> || is_uset_v<T>;

// ============================== 编译期 typeID 特质 ==============================
// 用于标记「自定义类型」及其 typeID。默认不是自定义类型（id = -1）。
// 自定义类型通过 SP_TYPE_ID(Type, N) 宏（未来的 [[sp::type_id(N)]] attribute，暂被 GCC 阻塞）登记。
template<typename T>
struct TypeID_t {
    static constexpr Sz id = static_cast<Sz>(-1);
    static constexpr bool is_custom = false;
};

// ============================== 指针序列化（含 PtrValue 地址跟踪、循环引用去重） ==============================

// 前向声明：多态类型工厂
namespace detail {
    using PFN_Creator = void* (*)();
    inline auto& creator_pfn_arr() {
        static std::vector<PFN_Creator> arr;
        return arr;
    }
    inline void* create_from_type_id(Sz typeID) {
        auto& arr = creator_pfn_arr();
        if (typeID >= arr.size() || arr[typeID] == nullptr) {
            throw SpTypeError("Invalid typeID for polymorphic deserialization");
        }
        return arr[typeID]();
    }

    // 创建对象：自定义类型读取 typeID 并走工厂；普通类型直接 new。
    template<typename T>
    inline void create_object(I& i, T*& v) {
        if constexpr (TypeID_t<T>::is_custom) {
            Sz typeID;
            read(i, typeID);
            v = static_cast<T*>(create_from_type_id(typeID));
        } else {
            v = new T{};
        }
    }
} // namespace detail

// ---- raw pointer T* ----
template<typename T>
inline void write(O& o, T* const& v) {
    auto const p = reinterpret_cast<PtrValue>(v);
    write(o, p);
    if (v == nullptr) return;
    if (o.ptrSet.find(p) == o.ptrSet.end()) {
        o.ptrSet.emplace(p);
        if constexpr (TypeID_t<T>::is_custom) {
            write(o, TypeID_t<T>::id);
        }
        serialize(o, *v);
    }
}
template<typename T>
inline void read(I& i, T*& v) {
    PtrValue p = 0;
    read(i, p);
    if (p == 0) {
        v = nullptr;
        return;
    }
    auto ptrIter = i.ptrSet.find(p);
    auto sptrIter = i.sptrSet.find(p);
    if (ptrIter != i.ptrSet.end()) {
        v = reinterpret_cast<T*>(ptrIter->second);
        return;
    }
    if (sptrIter != i.sptrSet.end()) {
        v = reinterpret_cast<T*>(sptrIter->second.get());
        return;
    }
    detail::create_object(i, v);
    deserialize(i, *v);
    i.ptrSet.emplace(p, v);
}

// ---- shared_ptr<T> ----
template<typename T>
inline void write(O& o, Sptr<T> const& v) {
    write(o, v.get());
}
template<typename T>
inline void read(I& i, Sptr<T>& v) {
    PtrValue p = 0;
    read(i, p);
    if (p == 0) {
        v.reset();
        return;
    }
    auto iter = i.sptrSet.find(p);
    if (iter != i.sptrSet.end()) {
        v = std::static_pointer_cast<T>(iter->second);
        return;
    }
    auto ptrIter = i.ptrSet.find(p);
    if (ptrIter != i.ptrSet.end()) {
        v.reset(reinterpret_cast<T*>(ptrIter->second));
        return;
    }
    T* ptr = nullptr;
    detail::create_object(i, ptr);
    v.reset(ptr);
    i.sptrSet.emplace(p, v);
    deserialize(i, *ptr);
}

// ---- weak_ptr<T> ----
template<typename T>
inline void write(O& o, Wptr<T> const& v) {
    write(o, v.lock().get());
}
template<typename T>
inline void read(I& i, Wptr<T>& v) {
    PtrValue p = 0;
    read(i, p);
    v.reset();
    if (p == 0) return;
    auto sIter = i.sptrSet.find(p);
    if (sIter != i.sptrSet.end()) {
        v = std::static_pointer_cast<T>(sIter->second);
        return;
    }
    auto ptrIter = i.ptrSet.find(p);
    if (ptrIter != i.ptrSet.end()) {
        Sptr<T> sptr(reinterpret_cast<T*>(ptrIter->second));
        i.sptrSet.emplace(p, sptr);
        v = sptr;
        return;
    }
    T* ptr = nullptr;
    detail::create_object(i, ptr);
    Sptr<T> sptr(ptr);
    deserialize(i, *sptr);
    v = sptr;
    i.sptrSet.emplace(p, std::move(sptr));
}

// ---- unique_ptr<T> ----
template<typename T>
inline void write(O& o, Uptr<T> const& v) {
    write(o, v.get());
}
template<typename T>
inline void read(I& i, Uptr<T>& v) {
    T* p = nullptr;
    read(i, p);
    v.reset(p);
}

// ============================== 通用序列化/反序列化转发 ==============================

template<typename T> void serialize(O& o, T const& v);
template<typename T> void deserialize(I& i, T& v);

// 基础类型 + 枚举
template<typename T> requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
inline void serialize(O& o, T const& v) { write(o, v); }
template<typename T> requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
inline void deserialize(I& i, T& v) { read(i, v); }

// string
inline void serialize(O& o, std::string const& v) { write(o, v); }
inline void deserialize(I& i, std::string& v) { read(i, v); }

// u8string
inline void serialize(O& o, std::u8string const& v) { write(o, v); }
inline void deserialize(I& i, std::u8string& v) { read(i, v); }

// wstring
inline void serialize(O& o, std::wstring const& v) { write(o, v); }
inline void deserialize(I& i, std::wstring& v) { read(i, v); }

// u16string
inline void serialize(O& o, std::u16string const& v) { write(o, v); }
inline void deserialize(I& i, std::u16string& v) { read(i, v); }

// u32string
inline void serialize(O& o, std::u32string const& v) { write(o, v); }
inline void deserialize(I& i, std::u32string& v) { read(i, v); }

// vector
template<typename T>
inline void serialize(O& o, std::vector<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, std::vector<T>& v) { read(i, v); }

// deque
template<typename T>
inline void serialize(O& o, std::deque<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, std::deque<T>& v) { read(i, v); }

// list
template<typename T>
inline void serialize(O& o, std::list<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, std::list<T>& v) { read(i, v); }

// forward_list
template<typename T>
inline void serialize(O& o, std::forward_list<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, std::forward_list<T>& v) { read(i, v); }

// set
template<typename T>
inline void serialize(O& o, std::set<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, std::set<T>& v) { read(i, v); }

// unordered_set
template<typename T>
inline void serialize(O& o, std::unordered_set<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, std::unordered_set<T>& v) { read(i, v); }

// map
template<typename K, typename V>
inline void serialize(O& o, std::map<K,V> const& v) { write(o, v); }
template<typename K, typename V>
inline void deserialize(I& i, std::map<K,V>& v) { read(i, v); }

// unordered_map
template<typename K, typename V>
inline void serialize(O& o, std::unordered_map<K,V> const& v) { write(o, v); }
template<typename K, typename V>
inline void deserialize(I& i, std::unordered_map<K,V>& v) { read(i, v); }

// array
template<typename T, size_t N>
inline void serialize(O& o, std::array<T,N> const& v) { write(o, v); }
template<typename T, size_t N>
inline void deserialize(I& i, std::array<T,N>& v) { read(i, v); }

// bitset
template<size_t N>
inline void serialize(O& o, std::bitset<N> const& v) { write(o, v); }
template<size_t N>
inline void deserialize(I& i, std::bitset<N>& v) { read(i, v); }

// optional
template<typename T>
inline void serialize(O& o, std::optional<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, std::optional<T>& v) { read(i, v); }

// tuple
template<typename... Ts>
inline void serialize(O& o, std::tuple<Ts...> const& v) { write(o, v); }
template<typename... Ts>
inline void deserialize(I& i, std::tuple<Ts...>& v) { read(i, v); }

// variant
template<typename... Ts>
inline void serialize(O& o, std::variant<Ts...> const& v) { write(o, v); }
template<typename... Ts>
inline void deserialize(I& i, std::variant<Ts...>& v) { read(i, v); }

// monostate
inline void serialize(O& o, std::monostate const& v) { write(o, v); }
inline void deserialize(I& i, std::monostate& v) { read(i, v); }

// chrono
template<typename Rep, typename Period>
inline void serialize(O& o, std::chrono::duration<Rep, Period> const& v) { write(o, v); }
template<typename Rep, typename Period>
inline void deserialize(I& i, std::chrono::duration<Rep, Period>& v) { read(i, v); }
template<typename Clock, typename Duration>
inline void serialize(O& o, std::chrono::time_point<Clock, Duration> const& v) { write(o, v); }
template<typename Clock, typename Duration>
inline void deserialize(I& i, std::chrono::time_point<Clock, Duration>& v) { read(i, v); }

// atomic
template<typename T>
inline void serialize(O& o, std::atomic<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, std::atomic<T>& v) { read(i, v); }

// atomic_ref
template<typename T>
inline void serialize(O& o, std::atomic_ref<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, std::atomic_ref<T>& v) { read(i, v); }

// path
inline void serialize(O& o, std::filesystem::path const& v) { write(o, v); }
inline void deserialize(I& i, std::filesystem::path& v) { read(i, v); }

// string_view (仅输出)
inline void serialize(O& o, std::string_view const& v) { write(o, v); }

// span (仅输出)
template<typename T>
inline void serialize(O& o, std::span<T> const& v) { write(o, v); }

// initializer_list (仅输出)
template<typename T>
inline void serialize(O& o, std::initializer_list<T> const& v) { write(o, v); }

// StreamPunkTime
inline void serialize(O& o, StreamPunkTime const& v) { write(o, v); }
inline void deserialize(I& i, StreamPunkTime& v) { read(i, v); }

// pointers
template<typename T>
inline void serialize(O& o, Sptr<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, Sptr<T>& v) { read(i, v); }
template<typename T>
inline void serialize(O& o, Uptr<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, Uptr<T>& v) { read(i, v); }
template<typename T>
inline void serialize(O& o, Wptr<T> const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, Wptr<T>& v) { read(i, v); }
template<typename T>
inline void serialize(O& o, T* const& v) { write(o, v); }
template<typename T>
inline void deserialize(I& i, T*& v) { read(i, v); }

// ============================== E_type 类型枚举 ==============================
// 在反射版中，自定义类型通过 TypeList 注册，不在 E_type 中占位
// E_type 仅覆盖基础类型和模板类型

namespace E_type {
    enum E : SpToken {
        e_unknowType = 0,
        e_op_position,
        e_op_select,
        e_op_deptr,
        e_op_ranges_insert_one,
        bg,
        ed,
        // 模板类型
        vector,
        array,
        string,
        bitset,
        deque,
        list,
        flist,
        set,
        uset,
        map,
        umap,
        sptr,
        wptr,
        uptr,
        opt,
        path,
        atomic,
        variant,
        tuple,
        // 基础类型
        u8_, u16_, u32_, u64_,
        i8_, i16_, i32_, i64_,
        f32_, f64_,
        ch_, ch8_, ch16_, ch32_, bl_,
        // 其他
        ptr,
        voidPtr,
        cst,
        dur,
        timepoint,
        Base,
        e_customType
    };
}

// ============================== TypeDesc 类型描述系统 ==============================

namespace detail {
    template<size_t... Ns>
    constexpr auto concat_arrays(const SpTokenArr<Ns>&... arrays) {
        constexpr size_t total_size = (Ns + ... + 0);
        SpTokenArr<total_size> result{};
        size_t index = 0;
        ((std::copy_n(arrays.begin(), Ns, result.begin() + index), index += Ns), ...);
        return result;
    }
} // namespace detail

template<typename... Args> struct TypeDesc;

// 基础类型
template<> struct TypeDesc<u8  > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::u8_  ) }; };
template<> struct TypeDesc<u16 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::u16_ ) }; };
template<> struct TypeDesc<u32 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::u32_ ) }; };
template<> struct TypeDesc<u64 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::u64_ ) }; };
template<> struct TypeDesc<i8  > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::i8_  ) }; };
template<> struct TypeDesc<i16 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::i16_ ) }; };
template<> struct TypeDesc<i32 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::i32_ ) }; };
template<> struct TypeDesc<i64 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::i64_ ) }; };
template<> struct TypeDesc<f32 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::f32_ ) }; };
template<> struct TypeDesc<f64 > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::f64_ ) }; };
template<> struct TypeDesc<char > { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::ch_ ) }; };
template<> struct TypeDesc<char8_t> { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::ch8_) }; };
template<> struct TypeDesc<char16_t> { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::ch16_) }; };
template<> struct TypeDesc<char32_t> { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::ch32_) }; };
template<> struct TypeDesc<bool> { static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::bl_ ) }; };

// chrono
template<typename Rep, typename Period>
struct TypeDesc<std::chrono::duration<Rep, Period>> {
    static inline constexpr auto v = detail::concat_arrays(
        SpTokenArr<1>{ static_cast<SpToken>(E_type::dur) },
        TypeDesc<Rep>::v,
        SpTokenArr<2>{ static_cast<SpToken>(Period::num), static_cast<SpToken>(Period::den) }
    );
};
template<typename Clock, typename Duration>
struct TypeDesc<std::chrono::time_point<Clock, Duration>> {
    static inline constexpr auto v = detail::concat_arrays(
        SpTokenArr<1>{ static_cast<SpToken>(E_type::timepoint) },
        TypeDesc<Duration>::v
    );
};

// 容器类型
template<typename T, typename... Args> struct TypeDesc<std::vector<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::vector)}, TypeDesc<T>::v);
};
template<typename T, typename... Args> struct TypeDesc<std::basic_string<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::string)}, TypeDesc<T>::v);
};
template<typename T, std::size_t N> struct TypeDesc<std::array<T, N>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<2>{static_cast<SpToken>(E_type::array), static_cast<Sz>(N)}, TypeDesc<T>::v);
};
template<std::size_t N> struct TypeDesc<std::bitset<N>> { static inline constexpr auto v = SpTokenArr<2>{ static_cast<SpToken>(E_type::bitset), static_cast<Sz>(N) }; };
template<typename T, typename... Args> struct TypeDesc<std::deque<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::deque)}, TypeDesc<T>::v);
};
template<typename T, typename... Args> struct TypeDesc<std::list<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::list)}, TypeDesc<T>::v);
};
template<typename T, typename... Args> struct TypeDesc<std::forward_list<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::flist)}, TypeDesc<T>::v);
};
template<typename T, typename... Args> struct TypeDesc<std::set<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::set)}, TypeDesc<T>::v);
};
template<typename T, typename... Args> struct TypeDesc<std::unordered_set<T, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::uset)}, TypeDesc<T>::v);
};
template<typename K, typename V, typename... Args> struct TypeDesc<std::map<K, V, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::map)}, TypeDesc<K>::v, TypeDesc<V>::v);
};
template<typename K, typename V, typename... Args> struct TypeDesc<std::unordered_map<K, V, Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::umap)}, TypeDesc<K>::v, TypeDesc<V>::v);
};

// 智能指针
template<typename T> struct TypeDesc<Sptr<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::sptr)}, TypeDesc<T>::v);
};
template<typename T> struct TypeDesc<Wptr<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::wptr)}, TypeDesc<T>::v);
};
template<typename T> struct TypeDesc<Uptr<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::uptr)}, TypeDesc<T>::v);
};

// 其他
template<typename T> struct TypeDesc<std::optional<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::opt)}, TypeDesc<T>::v);
};
template<> struct TypeDesc<std::filesystem::path> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::path) };
};
template<typename T> struct TypeDesc<std::atomic<T>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::atomic)}, TypeDesc<T>::v);
};
template<typename... Args> struct TypeDesc<std::variant<Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::variant)}, detail::concat_arrays(TypeDesc<Args>::v...), SpTokenArr<1>{static_cast<SpToken>(E_type::ed)});
};
template<typename... Args> struct TypeDesc<std::tuple<Args...>> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::tuple)}, detail::concat_arrays(TypeDesc<Args>::v...), SpTokenArr<1>{static_cast<SpToken>(E_type::ed)});
};

// 指针
template<typename T> struct TypeDesc<T*> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::ptr)}, TypeDesc<T>::v);
};
template<typename T> struct TypeDesc<T const> {
    static inline constexpr auto v = detail::concat_arrays(SpTokenArr<1>{static_cast<SpToken>(E_type::cst)}, TypeDesc<T>::v);
};
template<> struct TypeDesc<void*> {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::voidPtr) };
};

// enum 类型：映射为底层整数类型
template<typename T> struct TypeDesc<T, std::enable_if_t<std::is_enum_v<T>>> {
    static inline constexpr auto v = TypeDesc<std::underlying_type_t<T>>::v;
};

// StreamPunkTime
template<> struct TypeDesc<StreamPunkTime> {
    static inline constexpr auto v = detail::concat_arrays(
        SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) },
        TypeDesc<i64>::v,
        TypeDesc<i64>::v
    );
};

// 自定义类型：通过 SFINAE 检测 _desc 成员
template<typename T, typename = void>
struct TypeDescCustom {
    static inline constexpr auto v = SpTokenArr<1>{ static_cast<SpToken>(E_type::Base) };
};
template<typename T>
struct TypeDescCustom<T, std::void_t<decltype(T::_desc)>> {
    static inline constexpr auto v = T::_desc;
};

// 聚合类型描述：TypesDesc<Types...>
template<typename... Types>
struct TypesDesc {
    static inline constexpr auto v = detail::concat_arrays(TypeDesc<Types>::v...);
};

// ============================== DeepCopy 深拷贝系统 ==============================

// 基础类型
inline void deepCopy(DeepCopier&, u8& dst, u8 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, u16& dst, u16 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, u32& dst, u32 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, u64& dst, u64 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, i8& dst, i8 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, i16& dst, i16 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, i32& dst, i32 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, i64& dst, i64 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, f32& dst, f32 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, f64& dst, f64 const& src) { dst = src; }
inline void deepCopy(DeepCopier&, char& dst, char const& src) { dst = src; }
inline void deepCopy(DeepCopier&, char8_t& dst, char8_t const& src) { dst = src; }
inline void deepCopy(DeepCopier&, char16_t& dst, char16_t const& src) { dst = src; }
inline void deepCopy(DeepCopier&, char32_t& dst, char32_t const& src) { dst = src; }
inline void deepCopy(DeepCopier&, bool& dst, bool const& src) { dst = src; }
template<typename T> inline std::enable_if_t<std::is_enum_v<T>, void> deepCopy(DeepCopier&, T& dst, T const& src) { dst = src; }

// chrono
template<typename Rep, typename Period>
inline void deepCopy(DeepCopier&, std::chrono::duration<Rep, Period>& dst, std::chrono::duration<Rep, Period> const& src) { dst = src; }
template<typename Clock, typename Duration>
inline void deepCopy(DeepCopier&, std::chrono::time_point<Clock, Duration>& dst, std::chrono::time_point<Clock, Duration> const& src) { dst = src; }

// string / bitset
template<typename T, typename... Args>
inline void deepCopy(DeepCopier&, std::basic_string<T, Args...>& dst, std::basic_string<T, Args...> const& src) { dst = src; }
template<size_t N>
inline void deepCopy(DeepCopier&, std::bitset<N>& dst, std::bitset<N> const& src) { dst = src; }

// vector
template<typename T, typename... Args>
inline void deepCopy(DeepCopier& dc, std::vector<T, Args...>& dst, std::vector<T, Args...> const& src) {
    if (&dst == &src) return;
    dst.clear();
    dst.reserve(src.size());
    for (auto const& srcElement : src) {
        dst.emplace_back();
        deepCopy(dc, dst.back(), srcElement);
    }
}

// array
template<typename T, size_t N>
inline void deepCopy(DeepCopier& dc, std::array<T, N>& dst, std::array<T, N> const& src) {
    if (&dst == &src) return;
    for (size_t i = 0; i < N; ++i) {
        deepCopy(dc, dst[i], src[i]);
    }
}

// 容器辅助
namespace detail {
    template<typename T>
    inline void deepCopyContainer(DeepCopier& dc, T& dst, T const& src) {
        if (&dst == &src) return;
        dst.clear();
        for (auto const& srcElement : src) {
            typename T::value_type val{};
            deepCopy(dc, val, srcElement);
            dst.insert(dst.end(), std::move(val));
        }
    }
    template<typename T>
    inline void deepCopySet(DeepCopier& dc, T& dst, T const& src) {
        if (&dst == &src) return;
        dst.clear();
        for (auto const& srcElement : src) {
            typename T::value_type newElement{};
            deepCopy(dc, newElement, srcElement);
            dst.insert(std::move(newElement));
        }
    }
    template<typename T>
    inline void deepCopyMap(DeepCopier& dc, T& dst, T const& src) {
        if (&dst == &src) return;
        dst.clear();
        for (auto const& [srcKey, srcVal] : src) {
            typename T::key_type newKey{};
            typename T::mapped_type newVal{};
            deepCopy(dc, newKey, srcKey);
            deepCopy(dc, newVal, srcVal);
            dst.emplace(std::move(newKey), std::move(newVal));
        }
    }
} // namespace detail

template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::deque<T, Args...>& dst, std::deque<T, Args...> const& src) { detail::deepCopyContainer(dc, dst, src); }
template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::list<T, Args...>& dst, std::list<T, Args...> const& src) { detail::deepCopyContainer(dc, dst, src); }
template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::forward_list<T, Args...>& dst, std::forward_list<T, Args...> const& src) {
    if (&dst == &src) return;
    dst.clear();
    auto it = dst.before_begin();
    for (auto const& srcElement : src) {
        T val{};
        deepCopy(dc, val, srcElement);
        it = dst.insert_after(it, std::move(val));
    }
}
template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::set<T, Args...>& dst, std::set<T, Args...> const& src) { detail::deepCopySet(dc, dst, src); }
template<typename T, typename... Args> inline void deepCopy(DeepCopier& dc, std::unordered_set<T, Args...>& dst, std::unordered_set<T, Args...> const& src) { detail::deepCopySet(dc, dst, src); }
template<typename K, typename V, typename... Args> inline void deepCopy(DeepCopier& dc, std::map<K, V, Args...>& dst, std::map<K, V, Args...> const& src) { detail::deepCopyMap(dc, dst, src); }
template<typename K, typename V, typename... Args> inline void deepCopy(DeepCopier& dc, std::unordered_map<K, V, Args...>& dst, std::unordered_map<K, V, Args...> const& src) { detail::deepCopyMap(dc, dst, src); }

// optional
template<typename T>
inline void deepCopy(DeepCopier& dc, std::optional<T>& dst, std::optional<T> const& src) {
    if (&dst == &src) return;
    if (!src.has_value()) { dst.reset(); return; }
    if (!dst.has_value()) dst.emplace();
    deepCopy(dc, *dst, *src);
}

// tuple
namespace detail {
    template<typename... Args, size_t... Is>
    void deepCopyTupleImpl(DeepCopier& dc, std::tuple<Args...>& dst, std::tuple<Args...> const& src, std::index_sequence<Is...>) {
        (deepCopy(dc, std::get<Is>(dst), std::get<Is>(src)), ...);
    }
}
template<typename... Args>
void deepCopy(DeepCopier& dc, std::tuple<Args...>& dst, std::tuple<Args...> const& src) {
    if (&dst == &src) return;
    detail::deepCopyTupleImpl(dc, dst, src, std::make_index_sequence<sizeof...(Args)>{});
}

// variant
template<typename... Args>
void deepCopy(DeepCopier& dc, std::variant<Args...>& dst, std::variant<Args...> const& src) {
    if (&dst == &src) return;
    std::visit([&](auto const& val) {
        using T = std::decay_t<decltype(val)>;
        if (!std::holds_alternative<T>(dst)) dst = T{};
        deepCopy(dc, std::get<T>(dst), val);
    }, src);
}

// path
inline void deepCopy(DeepCopier&, std::filesystem::path& dst, std::filesystem::path const& src) {
    if (&dst != &src) dst = src;
}

// atomic
template<typename T>
void deepCopy(DeepCopier&, std::atomic<T>& dst, std::atomic<T> const& src) { dst.store(src.load()); }

// raw pointer T*
template<typename T>
inline void deepCopy(DeepCopier& dc, T*& dst, T* const& src) {
    if (dst == src) return;
    if (src == nullptr) { dst = nullptr; return; }
    void* key = const_cast<void*>(static_cast<const void*>(src));
    auto it = dc.find(key);
    if (it == dc.end()) {
        dst = new T{};
        dc.emplace(key, PtrRefInfo{ .refCount = 0, .kind = PtrRefInfo::e_raw, .rawPtr = dst });
        deepCopy(dc, *dst, *src);
        return;
    }
    auto& info = it->second;
    switch (info.kind) {
        case PtrRefInfo::e_raw : dst = static_cast<T*>(info.rawPtr); break;
        case PtrRefInfo::e_uptr: throw SpTypeError("deepCopy raw ptr: target already owned by Uptr");
        case PtrRefInfo::e_sptr: dst = static_cast<T*>(info.sptr.get()); break;
        default: throw SpTypeError("PtrRefInfo kind error!");
    }
}

// shared_ptr<T>
template<typename T>
inline void deepCopy(DeepCopier& dc, Sptr<T>& dst, Sptr<T> const& src) {
    if (dst == src) return;
    if (src.get() == nullptr) { dst.reset(); return; }
    void* key = static_cast<void*>(src.get());
    auto it = dc.find(key);
    if (it == dc.end()) {
        dst.reset(new T{});
        dc.emplace(key, PtrRefInfo{ .refCount = 0, .kind = PtrRefInfo::e_sptr, .sptr = dst });
        deepCopy(dc, *dst, *src);
        return;
    }
    auto& info = it->second;
    switch (info.kind) {
        case PtrRefInfo::e_raw : dst.reset(static_cast<T*>(info.rawPtr)); info.kind = PtrRefInfo::e_sptr; info.sptr = dst; break;
        case PtrRefInfo::e_sptr: dst = std::static_pointer_cast<T>(info.sptr); break;
        case PtrRefInfo::e_uptr: throw SpTypeError("Expecting an Sptr, but occupied by Uptr");
        default: throw SpTypeError("PtrRefInfo kind error!");
    }
}

// weak_ptr<T>
template<typename T>
inline void deepCopy(DeepCopier& dc, Wptr<T>& dst, Wptr<T> const& src) {
    Sptr<T> dstSptr = dst.lock();
    Sptr<T> srcSptr = src.lock();
    deepCopy(dc, dstSptr, srcSptr);
    dst = dstSptr;
}

// unique_ptr<T>
template<typename T>
inline void deepCopy(DeepCopier& dc, Uptr<T>& dst, Uptr<T> const& src) {
    if (dst == src) return;
    if (src.get() == nullptr) { dst.reset(); return; }
    void* key = static_cast<void*>(src.get());
    auto it = dc.find(key);
    if (it == dc.end()) {
        dst.reset(new T{});
        dc.emplace(key, PtrRefInfo{ .refCount = 0, .kind = PtrRefInfo::e_uptr, .rawPtr = dst.get() });
        deepCopy(dc, *dst, *src);
        return;
    }
    auto& info = it->second;
    switch (info.kind) {
        case PtrRefInfo::e_raw : dst.reset(static_cast<T*>(info.rawPtr)); info.kind = PtrRefInfo::e_uptr; break;
        case PtrRefInfo::e_sptr: throw SpTypeError("Expecting a Uptr, but occupied by Sptr");
        case PtrRefInfo::e_uptr: throw SpTypeError("Expecting a Uptr, but target already owned by another Uptr");
        default: throw SpTypeError("PtrRefInfo kind error!");
    }
}

// 自定义类型（泛型 fallback）：通过 SP_REFLECT 生成的 serialize/deserialize 实现
template<typename T>
inline void deepCopy(DeepCopier& dc, T& dst, T const& src) {
    if constexpr (requires { dst = src; }) {
        if (&dst != &src) dst = src;
    }
}

// ============================== 反射工具 ==============================

template<typename T>
consteval std::string_view type_name() {
    if constexpr (std::meta::has_identifier(^^T)) {
        return std::meta::identifier_of(^^T);
    } else {
        return std::meta::display_string_of(^^T);
    }
}

// 检测类型是否有 [[sp::type_id(N)]] attribute
// TODO: GCC 16.0.1 不支持 std::meta::attributes_of，暂用 stub
namespace detail {
    template<typename T>
    consteval bool has_type_id_attr() {
        return false;
    }

    template<typename T>
    consteval Sz get_type_id_value() {
        return 0;
    }
} // namespace detail

// ============================== TypeList 类型注册（编译期 typeID 自增 + attribute 显式指定） ==============================

template<typename... Ts>
struct TypeList {
    // 编译期计算每个类型的 typeID
    template<Sz BaseID = 0>
    static consteval auto compute_type_ids() {
        // 遍历 Ts...，为每个类型分配 typeID
        // 有 [[sp::type_id(N)]] 的使用 N 并跳到 N+1
        // 无 attribute 的使用当前计数器值
        // 返回 std::array<Sz, sizeof...(Ts)>
        std::array<Sz, sizeof...(Ts)> ids{};
        Sz counter = BaseID;
        size_t idx = 0;
        auto process = [&](auto type_info) {
            // type_info 是 ^^T 的反射信息
            // 检查 attribute 并分配 ID
            ids[idx] = counter;
            ++counter;
            ++idx;
        };
        (process(^^Ts), ...);
        return ids;
    }
};

// 注册自定义类型工厂函数
template<typename T>
inline void register_custom_type(Sz typeID) {
    auto& arr = detail::creator_pfn_arr();
    if (typeID >= arr.size()) {
        arr.resize(typeID + 1, nullptr);
    }
    arr[typeID] = []() -> void* { return static_cast<void*>(new T{}); };
}

// 注册 TypeList 中的所有类型
template<typename... Ts>
inline void register_types(TypeList<Ts...>) {
    auto do_register = [&]<typename T>(std::type_identity<T>) {
        if constexpr (TypeID_t<T>::is_custom) {
            register_custom_type<T>(TypeID_t<T>::id);
        }
    };
    (do_register(std::type_identity<Ts>{}), ...);
}

// ============================== SpRegistry 跨模块类型注册表 ==============================

struct SpRegistry {
    std::vector<detail::PFN_Creator> creators;
    std::unordered_map<Sz, std::string> typeID2ClassName;
    std::unordered_map<Sz, std::string> ormTableMap;
};

inline SpRegistry*& _sp_registry_ptr() {
    static SpRegistry* ptr = nullptr;
    return ptr;
}

// typeID → 类名映射
inline std::unordered_map<Sz, std::string>& typeID2ClassName() {
    auto* reg = _sp_registry_ptr();
    if (reg) return reg->typeID2ClassName;
    static std::unordered_map<Sz, std::string> map;
    return map;
}

// 初始化：注册 TypeList 中的所有类型到注册表
inline void _init_stream_punk_impl(SpRegistry* reg = nullptr) {
    _sp_registry_ptr() = reg;
    // 用户需在调用前通过 register_types(RegisteredTypes{}) 注册类型
}

#define INIT_StreamPunk(...) sp26::_init_stream_punk_impl(__VA_ARGS__)

// ============================== 便捷函数 ==============================

template<typename T>
std::string to_binary(T const& v) {
    std::stringstream ss;
    O o(ss);
    serialize(o, v);
    return ss.str();
}

template<typename T>
T from_binary(std::string const& data) {
    std::stringstream ss(data);
    I i(ss);
    T v{};
    deserialize(i, v);
    return v;
}

// 就地反序列化：适用于不可拷贝/不可移动的类型（如 std::atomic<T>），
// 这类类型无法通过按值返回的 from_binary<T>() 表达。
template<typename T>
void from_binary(std::string const& data, T& out) {
    std::stringstream ss(data);
    I i(ss);
    deserialize(i, out);
}

// MemberTypeList 通用模板
template<typename T> struct MemberTypeList { using TypeList = std::tuple<>; };

} // namespace sp26

// ============================== SP_TYPE_ID 宏 ==============================
// 登记自定义类型及其 typeID（编译期常量）。
// 过渡方案：等 GCC 支持读取 [[sp::type_id(N)]] attribute 后，可改由 attribute 声明。
// 用法:
//   SP_TYPE_ID(Player, 0);
//   之后 TypeID_t<Player>::id == 0 且 is_custom == true，
//   指针/智能指针序列化会在对象前写入该 typeID，反序列化时经工厂重建。
#define SP_TYPE_ID(TYPE, ID) \
    template<> struct sp26::TypeID_t<TYPE> { \
        static constexpr sp26::Sz id = (ID); \
        static constexpr bool is_custom = true; \
    };

// ============================== SP_AUTO_TYPES 自动填号宏 ==============================
// 集中登记 + 自动填号：不再手写 SP_TYPE_ID(T, N)。
// 用法:
//   SP_AUTO_TYPES(A, B, C)
//   等价于按顺序自动生成 SP_TYPE_ID(A,0) SP_TYPE_ID(B,1) SP_TYPE_ID(C,2)，
//   并在 sp26 内定义 `using RegisteredTypes = TypeList<A,B,C>`。
//   之后统一 `register_types(sp26::RegisteredTypes{})` 注册工厂。
// 注意：typeID 由登记顺序决定——在列表中间增删类型会使其后所有 typeID 位移，
//       跨语言两端须保持同一登记顺序。需要稳定/显式 ID 时请用 SP_TYPE_ID。
#define SP_AUTO_ONE(TYPE, N) \
    template<> struct sp26::TypeID_t<TYPE> { \
        static constexpr sp26::Sz id = (N); \
        static constexpr bool is_custom = true; \
    };
#define SP_AUTO_EACH_1(M, A) \
    M(A, 0)
#define SP_AUTO_EACH_2(M, A, B) \
    M(A, 0) M(B, 1)
#define SP_AUTO_EACH_3(M, A, B, C) \
    M(A, 0) M(B, 1) M(C, 2)
#define SP_AUTO_EACH_4(M, A, B, C, D) \
    M(A, 0) M(B, 1) M(C, 2) M(D, 3)
#define SP_AUTO_EACH_5(M, A, B, C, D, E) \
    M(A, 0) M(B, 1) M(C, 2) M(D, 3) M(E, 4)
#define SP_AUTO_EACH_6(M, A, B, C, D, E, F) \
    M(A, 0) M(B, 1) M(C, 2) M(D, 3) M(E, 4) M(F, 5)
#define SP_AUTO_EACH_7(M, A, B, C, D, E, F, G) \
    M(A, 0) M(B, 1) M(C, 2) M(D, 3) M(E, 4) M(F, 5) M(G, 6)
#define SP_AUTO_EACH_8(M, A, B, C, D, E, F, G, H) \
    M(A, 0) M(B, 1) M(C, 2) M(D, 3) M(E, 4) M(F, 5) M(G, 6) M(H, 7)
#define SP_AUTO_SEL(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define SP_AUTO_NARG(...) SP_AUTO_SEL(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1)
#define SP_AUTO_CAT_(a, b) a##b
#define SP_AUTO_CAT(a, b) SP_AUTO_CAT_(a, b)
#define SP_AUTO_DISPATCH(M, ...) SP_AUTO_CAT(SP_AUTO_EACH_, SP_AUTO_NARG(__VA_ARGS__))(M, __VA_ARGS__)
#define SP_AUTO_TYPES(...) \
    SP_AUTO_DISPATCH(SP_AUTO_ONE, __VA_ARGS__) \
    namespace sp26 { using RegisteredTypes = TypeList<__VA_ARGS__>; }

// ============================== SP_REFLECT 宏 ==============================
// 过渡方案：等 GCC 的 nonstatic_data_members_of 修复后移除
// 用法: 
//   SP_REFLECT(Player, id, name, health, pos);
//   生成: serialize/deserialize 特化 + memberTuple() + member index enum

#define SP_REFLECT_SER_ONE(TYPE, MEMBER) \
    sp26::serialize(o, v.[:^^TYPE::MEMBER:]);

#define SP_REFLECT_DESER_ONE(TYPE, MEMBER) \
    sp26::deserialize(i, v.[:^^TYPE::MEMBER:]);

#define SP_REFLECT_ENUM_ONE(TYPE, MEMBER) \
    e_##MEMBER,

#define SP_REFLECT_TUPLE_REF_ONE(TYPE, MEMBER) \
    v.[:^^TYPE::MEMBER:],

#define SP_REFLECT_DEEP_COPY_ONE(TYPE, MEMBER) \
    sp26::deepCopy(dc, dst.[:^^TYPE::MEMBER:], src.[:^^TYPE::MEMBER:]);

#define SP_REFLECT_COMMA_DECLTYPE_ONE(TYPE, MEMBER) \
    , decltype(std::declval<TYPE&>().[:^^TYPE::MEMBER:])

#define SP_REFLECT_SER(TYPE, MEMBERS) \
    template<> inline void sp26::serialize<TYPE>(sp26::O& o, TYPE const& v) { \
        MEMBERS \
    }

#define SP_REFLECT_DESER(TYPE, MEMBERS) \
    template<> inline void sp26::deserialize<TYPE>(sp26::I& i, TYPE& v) { \
        MEMBERS \
    }

#define SP_REFLECT_ENUM(TYPE, MEMBERS) \
    namespace sp26::_reflect_enum_##TYPE { enum E { MEMBERS e_maxCount }; }

#define SP_REFLECT_MEMBER_TUPLE(TYPE, MEMBERS) \
    inline auto memberTuple(TYPE& v) { return std::tie(MEMBERS std::ignore); }

#define SP_REFLECT_TYPELIST(TYPE, MEMBERS) \
    template<> struct sp26::MemberTypeList<TYPE> { \
        using TypeList = std::tuple<void MEMBERS >; \
    };

#define SP_REFLECT_DEEP_COPY(TYPE, MEMBERS) \
    template<> inline void sp26::deepCopy<TYPE>(sp26::DeepCopier& dc, TYPE& dst, TYPE const& src) { \
        MEMBERS \
    }

#define SP_REFLECT_FULL(TYPE, MEMBERS_SER, MEMBERS_DESER, MEMBERS_ENUM, MEMBERS_TUPLE, MEMBERS_TYPELIST, MEMBERS_DEEP_COPY) \
    SP_REFLECT_SER(TYPE, MEMBERS_SER) \
    SP_REFLECT_DESER(TYPE, MEMBERS_DESER) \
    SP_REFLECT_ENUM(TYPE, MEMBERS_ENUM) \
    SP_REFLECT_MEMBER_TUPLE(TYPE, MEMBERS_TUPLE) \
    SP_REFLECT_TYPELIST(TYPE, MEMBERS_TYPELIST) \
    SP_REFLECT_DEEP_COPY(TYPE, MEMBERS_DEEP_COPY)

#define SP_REFLECT(TypeName, ...) \
    SP_REFLECT_SER(TypeName, SP_REFLECT_FOR_EACH_SER(TypeName, __VA_ARGS__)) \
    SP_REFLECT_DESER(TypeName, SP_REFLECT_FOR_EACH_DESER(TypeName, __VA_ARGS__)) \
    SP_REFLECT_ENUM(TypeName, SP_REFLECT_FOR_EACH_ENUM(TypeName, __VA_ARGS__)) \
    SP_REFLECT_MEMBER_TUPLE(TypeName, SP_REFLECT_FOR_EACH_TUPLE(TypeName, __VA_ARGS__)) \
    SP_REFLECT_TYPELIST(TypeName, SP_REFLECT_FOR_EACH_TYPELIST(TypeName, __VA_ARGS__)) \
    SP_REFLECT_DEEP_COPY(TypeName, SP_REFLECT_FOR_EACH_DEEP_COPY(TypeName, __VA_ARGS__))

#define SP_REFLECT_FOR_EACH_SER(TYPE, ...) \
    SP_REFLECT_FOR_EACH_IMPL(SP_REFLECT_SER_ONE, TYPE, __VA_ARGS__)

#define SP_REFLECT_FOR_EACH_DESER(TYPE, ...) \
    SP_REFLECT_FOR_EACH_IMPL(SP_REFLECT_DESER_ONE, TYPE, __VA_ARGS__)

#define SP_REFLECT_FOR_EACH_ENUM(TYPE, ...) \
    SP_REFLECT_FOR_EACH_IMPL(SP_REFLECT_ENUM_ONE, TYPE, __VA_ARGS__)

#define SP_REFLECT_FOR_EACH_TUPLE(TYPE, ...) \
    SP_REFLECT_FOR_EACH_IMPL(SP_REFLECT_TUPLE_REF_ONE, TYPE, __VA_ARGS__)

#define SP_REFLECT_FOR_EACH_TYPELIST(TYPE, ...) \
    SP_REFLECT_FOR_EACH_IMPL(SP_REFLECT_COMMA_DECLTYPE_ONE, TYPE, __VA_ARGS__)

#define SP_REFLECT_FOR_EACH_DEEP_COPY(TYPE, ...) \
    SP_REFLECT_FOR_EACH_IMPL(SP_REFLECT_DEEP_COPY_ONE, TYPE, __VA_ARGS__)

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

#define SP_REFLECT_NARG(...) SP_REFLECT_NARG_(__VA_ARGS__, SP_REFLECT_RSEQ_N())
#define SP_REFLECT_NARG_(...) SP_REFLECT_ARG_N(__VA_ARGS__)
#define SP_REFLECT_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, N, ...) N
#define SP_REFLECT_RSEQ_N() 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1

#define SP_REFLECT_CONCAT_(a, b) a##b
#define SP_REFLECT_CONCAT(a, b) SP_REFLECT_CONCAT_(a, b)
#define SP_REFLECT_EXPAND(x) x