// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT
#pragma once

/*
    StreamPunk JSON 模块 (v0.3.0)

    设计原则：
    1. 零外部依赖，自包含 JSON 值类型
    2. 复用现有 Xt_TypeName 宏，通过 UseDataJson 生成 toJson/fromJson
    3. 指针展开为内联 JSON 对象
    4. 与 StreamPunk_.hpp 中的 Base 虚函数对接

    使用方法：
        struct MyType : public Base {
            #define Xt_MyType(X__) ...
            UseData(MyType);          // 已有：二进制序列化
            UseDataJson(MyType);      // 新增：JSON 支持
        };
*/

#include "StreamPunk.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <charconv>
#include <cctype>

namespace sp {

// ============================== 辅助函数（与区域设置无关） ==============================

// 将整数写入流（不受 locale 影响，不使用千位分隔符）
namespace detail {
    inline void jsonWriteInt(std::ostream& os, i64 v) {
        char buf[32];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        if (ec == std::errc{}) os.write(buf, ptr - buf);
        else os << v;
    }
    inline void jsonWriteUint(std::ostream& os, u64 v) {
        char buf[32];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        if (ec == std::errc{}) os.write(buf, ptr - buf);
        else os << v;
    }
}

// ============================== JsonVal ==============================

struct JsonVal {
    enum Type : u8 { Null, Bool, Int, Uint, Float, String, Array, Object };

    Type type = Null;

    bool   bVal = false;
    i64    iVal = 0;
    u64    uVal = 0;
    f64    fVal = 0.0;
    std::string               strVal;
    std::vector<JsonVal>      arrVal;
    std::map<std::string, JsonVal> objVal;

    JsonVal() = default;
    JsonVal(std::nullptr_t) : type(Null) {}
    JsonVal(bool v) : type(Bool), bVal(v) {}
    JsonVal(i32 v) : type(Int), iVal(v) {}
    JsonVal(i64 v) : type(Int), iVal(v) {}
    JsonVal(u32 v) : type(Uint), uVal(v) {}
    JsonVal(u64 v) : type(Uint), uVal(v) {}
    JsonVal(f64 v) : type(Float), fVal(v) {}
    JsonVal(f32 v) : type(Float), fVal(static_cast<f64>(v)) {}
    JsonVal(std::string const& v) : type(String), strVal(v) {}
    JsonVal(std::string&& v) : type(String), strVal(std::move(v)) {}
    JsonVal(char const* v) : type(String), strVal(v) {}
    JsonVal(std::string_view v) : type(String), strVal(v) {}

    JsonVal& operator[](std::string const& key) {
        if (type != Object) { type = Object; objVal.clear(); }
        return objVal[key];
    }

    JsonVal const& operator[](std::string const& key) const {
        static JsonVal const nullVal;
        if (type != Object) return nullVal;
        auto it = objVal.find(key);
        return it != objVal.end() ? it->second : nullVal;
    }

    JsonVal& operator[](size_t idx) {
        if (type != Array) { type = Array; arrVal.clear(); }
        if (idx >= arrVal.size()) arrVal.resize(idx + 1);
        return arrVal[idx];
    }

    void push_back(JsonVal v) {
        if (type != Array) { type = Array; arrVal.clear(); }
        arrVal.push_back(std::move(v));
    }

    bool has_key(std::string const& key) const {
        return type == Object && objVal.count(key) > 0;
    }

    // ==================== 序列化为 JSON 字符串 ====================

    std::string dump(int indent = 0) const {
        std::stringstream ss;
        dumpTo(ss, indent, 0);
        return ss.str();
    }

private:
    void dumpTo(std::ostream& os, int indent, int depth) const {
        std::string pad(depth * indent, ' ');
        std::string padInner((depth + 1) * indent, ' ');

        switch (type) {
        case Null: os << "null"; break;
        case Bool: os << (bVal ? "true" : "false"); break;
        case Int:  detail::jsonWriteInt(os, iVal); break;
        case Uint: detail::jsonWriteUint(os, uVal); break;
        case Float: {
            if (std::isnan(fVal)) os << "null";
            else if (std::isinf(fVal)) os << (fVal > 0 ? "1e999" : "-1e999");
            else {
                char buf[64];
                auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), fVal, std::chars_format::general, 17);
                if (ec == std::errc{}) os.write(buf, ptr - buf);
                else os << fVal;
            }
            break;
        }
        case String: dumpString(os, strVal); break;
        case Array: {
            if (arrVal.empty()) { os << "[]"; break; }
            os << "[";
            if (indent > 0) os << "\n";
            for (size_t i = 0; i < arrVal.size(); ++i) {
                if (indent > 0) os << padInner;
                arrVal[i].dumpTo(os, indent, depth + 1);
                if (i + 1 < arrVal.size()) os << ",";
                if (indent > 0) os << "\n";
            }
            if (indent > 0) os << pad;
            os << "]";
            break;
        }
        case Object: {
            if (objVal.empty()) { os << "{}"; break; }
            os << "{";
            if (indent > 0) os << "\n";
            size_t i = 0;
            for (auto const& [k, v] : objVal) {
                if (indent > 0) os << padInner;
                dumpString(os, k);
                os << ":";
                if (indent > 0) os << " ";
                v.dumpTo(os, indent, depth + 1);
                if (++i < objVal.size()) os << ",";
                if (indent > 0) os << "\n";
            }
            if (indent > 0) os << pad;
            os << "}";
            break;
        }
        }
    }

    static void dumpString(std::ostream& os, std::string const& s) {
        os << '"';
        for (char c : s) {
            switch (c) {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b";  break;
            case '\f': os << "\\f";  break;
            case '\n': os << "\\n";  break;
            case '\r': os << "\\r";  break;
            case '\t': os << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    os << "\\u00" << "0123456789abcdef"[(c >> 4) & 0xF] << "0123456789abcdef"[c & 0xF];
                } else {
                    os << c;
                }
            }
        }
        os << '"';
    }

public:
    // ==================== 从 JSON 字符串反序列化 ====================

    static JsonVal parse(std::string_view sv) {
        ParseCtx ctx{sv};
        JsonVal v = parseValue(ctx);
        skipWhitespace(ctx);
        if (ctx.pos < ctx.sv.size()) {
            throw SpDataError("JSON parse: unexpected trailing content");
        }
        return v;
    }

private:
    struct ParseCtx {
        std::string_view sv;
        size_t pos = 0;
    };

    static void skipWhitespace(ParseCtx& ctx) {
        while (ctx.pos < ctx.sv.size() && std::isspace(static_cast<unsigned char>(ctx.sv[ctx.pos]))) {
            ++ctx.pos;
        }
    }

    static char peek(ParseCtx& ctx) {
        skipWhitespace(ctx);
        if (ctx.pos >= ctx.sv.size()) throw SpDataError("JSON parse: unexpected end of input");
        return ctx.sv[ctx.pos];
    }

    static char next(ParseCtx& ctx) {
        skipWhitespace(ctx);
        if (ctx.pos >= ctx.sv.size()) throw SpDataError("JSON parse: unexpected end of input");
        return ctx.sv[ctx.pos++];
    }

    static JsonVal parseValue(ParseCtx& ctx) {
        char c = peek(ctx);
        switch (c) {
        case 'n': return parseNull(ctx);
        case 't': case 'f': return parseBool(ctx);
        case '"': return parseString(ctx);
        case '[': return parseArray(ctx);
        case '{': return parseObject(ctx);
        default:
            if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber(ctx);
            throw SpDataError(std::string("JSON parse: unexpected character '") + c + "'");
        }
    }

    static JsonVal parseNull(ParseCtx& ctx) {
        if (ctx.sv.substr(ctx.pos, 4) != "null") throw SpDataError("JSON parse: expected 'null'");
        ctx.pos += 4;
        return JsonVal{};
    }

    static JsonVal parseBool(ParseCtx& ctx) {
        if (ctx.sv.substr(ctx.pos, 4) == "true") { ctx.pos += 4; return JsonVal{true}; }
        if (ctx.sv.substr(ctx.pos, 5) == "false") { ctx.pos += 5; return JsonVal{false}; }
        throw SpDataError("JSON parse: expected 'true' or 'false'");
    }

    static JsonVal parseString(ParseCtx& ctx) {
        ++ctx.pos; // 跳过开始引号
        std::string s;
        while (ctx.pos < ctx.sv.size()) {
            char c = ctx.sv[ctx.pos++];
            if (c == '"') return JsonVal{std::move(s)};
            if (c == '\\') {
                if (ctx.pos >= ctx.sv.size()) throw SpDataError("JSON parse: unexpected end in string escape");
                char e = ctx.sv[ctx.pos++];
                switch (e) {
                case '"':  s += '"';  break;
                case '\\': s += '\\'; break;
                case '/':  s += '/';  break;
                case 'b':  s += '\b'; break;
                case 'f':  s += '\f'; break;
                case 'n':  s += '\n'; break;
                case 'r':  s += '\r'; break;
                case 't':  s += '\t'; break;
                case 'u': {
                    // 简单处理：读取 4 位 hex，只支持 ASCII
                    if (ctx.pos + 4 > ctx.sv.size()) throw SpDataError("JSON parse: unexpected end in \\u escape");
                    uint16_t cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = ctx.sv[ctx.pos++];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                        else throw SpDataError("JSON parse: invalid hex in \\u escape");
                    }
                    if (cp < 0x80) s += static_cast<char>(cp);
                    else s += '?'; // 非 ASCII 暂用 ? 代替
                    break;
                }
                default: throw SpDataError("JSON parse: invalid escape character");
                }
            } else {
                s += c;
            }
        }
        throw SpDataError("JSON parse: unterminated string");
    }

    static JsonVal parseNumber(ParseCtx& ctx) {
        size_t start = ctx.pos;
        bool isFloat = false;
        if (ctx.pos < ctx.sv.size() && ctx.sv[ctx.pos] == '-') ++ctx.pos;
        while (ctx.pos < ctx.sv.size() && std::isdigit(static_cast<unsigned char>(ctx.sv[ctx.pos]))) ++ctx.pos;
        if (ctx.pos < ctx.sv.size() && ctx.sv[ctx.pos] == '.') { isFloat = true; ++ctx.pos; }
        while (ctx.pos < ctx.sv.size() && std::isdigit(static_cast<unsigned char>(ctx.sv[ctx.pos]))) ++ctx.pos;
        if (ctx.pos < ctx.sv.size() && (ctx.sv[ctx.pos] == 'e' || ctx.sv[ctx.pos] == 'E')) {
            isFloat = true; ++ctx.pos;
            if (ctx.pos < ctx.sv.size() && (ctx.sv[ctx.pos] == '+' || ctx.sv[ctx.pos] == '-')) ++ctx.pos;
            while (ctx.pos < ctx.sv.size() && std::isdigit(static_cast<unsigned char>(ctx.sv[ctx.pos]))) ++ctx.pos;
        }
        std::string numStr(ctx.sv.substr(start, ctx.pos - start));
        if (isFloat) {
            return JsonVal{std::stod(numStr)};
        } else {
            // 尝试解析为有符号整数
            try {
                i64 v = std::stoll(numStr);
                return JsonVal{v};
            } catch (...) {
                try {
                    u64 v = std::stoull(numStr);
                    return JsonVal{v};
                } catch (...) {
                    return JsonVal{std::stod(numStr)};
                }
            }
        }
    }

    static JsonVal parseArray(ParseCtx& ctx) {
        next(ctx); // 跳过 '['
        JsonVal arr;
        arr.type = Array;
        skipWhitespace(ctx);
        if (ctx.pos < ctx.sv.size() && ctx.sv[ctx.pos] == ']') { ++ctx.pos; return arr; }
        while (true) {
            arr.arrVal.push_back(parseValue(ctx));
            skipWhitespace(ctx);
            if (ctx.pos >= ctx.sv.size()) throw SpDataError("JSON parse: unterminated array");
            if (ctx.sv[ctx.pos] == ']') { ++ctx.pos; break; }
            if (ctx.sv[ctx.pos] == ',') { ++ctx.pos; continue; }
            throw SpDataError("JSON parse: expected ',' or ']' in array");
        }
        return arr;
    }

    static JsonVal parseObject(ParseCtx& ctx) {
        next(ctx); // 跳过 '{'
        JsonVal obj;
        obj.type = Object;
        skipWhitespace(ctx);
        if (ctx.pos < ctx.sv.size() && ctx.sv[ctx.pos] == '}') { ++ctx.pos; return obj; }
        while (true) {
            skipWhitespace(ctx);
            if (ctx.pos >= ctx.sv.size() || ctx.sv[ctx.pos] != '"') throw SpDataError("JSON parse: expected string key in object");
            JsonVal key = parseString(ctx);
            skipWhitespace(ctx);
            if (ctx.pos >= ctx.sv.size() || ctx.sv[ctx.pos] != ':') throw SpDataError("JSON parse: expected ':' after key");
            ++ctx.pos; // 跳过 ':'
            obj.objVal[key.strVal] = parseValue(ctx);
            skipWhitespace(ctx);
            if (ctx.pos >= ctx.sv.size()) throw SpDataError("JSON parse: unterminated object");
            if (ctx.sv[ctx.pos] == '}') { ++ctx.pos; break; }
            if (ctx.sv[ctx.pos] == ',') { ++ctx.pos; continue; }
            throw SpDataError("JSON parse: expected ',' or '}' in object");
        }
        return obj;
    }
};

// ============================== 类型转换辅助 ==============================

template<typename T> T json_cast(JsonVal const& j);

template<> inline bool json_cast<bool>(JsonVal const& j) {
    switch (j.type) {
    case JsonVal::Bool: return j.bVal;
    case JsonVal::Int:  return j.iVal != 0;
    case JsonVal::Uint: return j.uVal != 0;
    case JsonVal::Float: return j.fVal != 0.0;
    default: return false;
    }
}

#define SP_JSON_CAST_INT(T) \
template<> inline T json_cast<T>(JsonVal const& j) { \
    switch (j.type) { \
    case JsonVal::Int:  return static_cast<T>(j.iVal); \
    case JsonVal::Uint: return static_cast<T>(j.uVal); \
    case JsonVal::Float: return static_cast<T>(j.fVal); \
    case JsonVal::Bool: return static_cast<T>(j.bVal); \
    default: return T{}; \
    } \
}
SP_JSON_CAST_INT(i8)
SP_JSON_CAST_INT(i16)
SP_JSON_CAST_INT(i32)
SP_JSON_CAST_INT(i64)
SP_JSON_CAST_INT(u8)
SP_JSON_CAST_INT(u16)
SP_JSON_CAST_INT(u32)
SP_JSON_CAST_INT(u64)
#undef SP_JSON_CAST_INT

template<> inline f32 json_cast<f32>(JsonVal const& j) {
    switch (j.type) {
    case JsonVal::Float: return static_cast<f32>(j.fVal);
    case JsonVal::Int:   return static_cast<f32>(j.iVal);
    case JsonVal::Uint:  return static_cast<f32>(j.uVal);
    default: return 0.0f;
    }
}
template<> inline f64 json_cast<f64>(JsonVal const& j) {
    switch (j.type) {
    case JsonVal::Float: return j.fVal;
    case JsonVal::Int:   return static_cast<f64>(j.iVal);
    case JsonVal::Uint:  return static_cast<f64>(j.uVal);
    default: return 0.0;
    }
}
template<> inline ch  json_cast<ch>(JsonVal const& j)  { return j.type == JsonVal::String && !j.strVal.empty() ? j.strVal[0] : static_cast<ch>(json_cast<i32>(j)); }
template<> inline ch8  json_cast<ch8>(JsonVal const& j)  { return j.type == JsonVal::String && !j.strVal.empty() ? static_cast<ch8>(j.strVal[0]) : static_cast<ch8>(json_cast<i32>(j)); }
template<> inline ch16 json_cast<ch16>(JsonVal const& j) { return j.type == JsonVal::String && !j.strVal.empty() ? static_cast<ch16>(j.strVal[0]) : static_cast<ch16>(json_cast<i32>(j)); }
template<> inline ch32 json_cast<ch32>(JsonVal const& j) { return j.type == JsonVal::String && !j.strVal.empty() ? static_cast<ch32>(j.strVal[0]) : static_cast<ch32>(json_cast<i32>(j)); }
template<> inline std::string json_cast<std::string>(JsonVal const& j) { return j.type == JsonVal::String ? j.strVal : j.dump(); }

// ============================== spToJson 序列化函数 ==============================

// 基本类型
inline void spToJson(bool v, JsonVal& j)                    { j = JsonVal{v}; }
inline void spToJson(i8 v, JsonVal& j)                      { j = JsonVal{static_cast<i64>(v)}; }
inline void spToJson(i16 v, JsonVal& j)                     { j = JsonVal{static_cast<i64>(v)}; }
inline void spToJson(i32 v, JsonVal& j)                     { j = JsonVal{static_cast<i64>(v)}; }
inline void spToJson(i64 v, JsonVal& j)                     { j = JsonVal{v}; }
inline void spToJson(u8 v, JsonVal& j)                      { j = JsonVal{static_cast<u64>(v)}; }
inline void spToJson(u16 v, JsonVal& j)                     { j = JsonVal{static_cast<u64>(v)}; }
inline void spToJson(u32 v, JsonVal& j)                     { j = JsonVal{static_cast<u64>(v)}; }
inline void spToJson(u64 v, JsonVal& j)                     { j = JsonVal{v}; }
inline void spToJson(f32 v, JsonVal& j)                     { j = JsonVal{v}; }
inline void spToJson(f64 v, JsonVal& j)                     { j = JsonVal{v}; }
inline void spToJson(ch v, JsonVal& j)                      { j = JsonVal{std::string(1, v)}; }
inline void spToJson(ch8 v, JsonVal& j)                     { j = JsonVal{std::string(1, static_cast<char>(v))}; }
inline void spToJson(ch16 v, JsonVal& j)                    { j = JsonVal{static_cast<i64>(v)}; }
inline void spToJson(ch32 v, JsonVal& j)                    { j = JsonVal{static_cast<i64>(v)}; }

// enum
template<typename T> std::enable_if_t<std::is_enum_v<T>> spToJson(T v, JsonVal& j) {
    j = JsonVal{static_cast<i64>(v)};
}

// std::string / std::basic_string
template<typename T, typename... Args>
inline void spToJson(std::basic_string<T, Args...> const& v, JsonVal& j) {
    if constexpr (std::is_same_v<T, char>) {
        j = JsonVal{v};
    } else {
        // 非 char 字符串转为数字数组
        j.type = JsonVal::Array;
        for (auto c : v) j.arrVal.push_back(JsonVal{static_cast<i64>(c)});
    }
}

// std::vector
template<typename T, typename... Args>
inline void spToJson(std::vector<T, Args...> const& v, JsonVal& j) {
    j.type = JsonVal::Array;
    for (auto const& x : v) {
        JsonVal elem;
        spToJson(x, elem);
        j.arrVal.push_back(std::move(elem));
    }
}

// std::array
template<typename T, size_t N>
inline void spToJson(std::array<T, N> const& v, JsonVal& j) {
    j.type = JsonVal::Array;
    for (auto const& x : v) {
        JsonVal elem;
        spToJson(x, elem);
        j.arrVal.push_back(std::move(elem));
    }
}

// std::deque
template<typename T, typename... Args>
inline void spToJson(std::deque<T, Args...> const& v, JsonVal& j) {
    j.type = JsonVal::Array;
    for (auto const& x : v) {
        JsonVal elem;
        spToJson(x, elem);
        j.arrVal.push_back(std::move(elem));
    }
}

// std::list
template<typename T, typename... Args>
inline void spToJson(std::list<T, Args...> const& v, JsonVal& j) {
    j.type = JsonVal::Array;
    for (auto const& x : v) {
        JsonVal elem;
        spToJson(x, elem);
        j.arrVal.push_back(std::move(elem));
    }
}

// std::forward_list
template<typename T, typename... Args>
inline void spToJson(std::forward_list<T, Args...> const& v, JsonVal& j) {
    j.type = JsonVal::Array;
    for (auto const& x : v) {
        JsonVal elem;
        spToJson(x, elem);
        j.arrVal.push_back(std::move(elem));
    }
}

// std::set
template<typename T, typename... Args>
inline void spToJson(std::set<T, Args...> const& v, JsonVal& j) {
    j.type = JsonVal::Array;
    for (auto const& x : v) {
        JsonVal elem;
        spToJson(x, elem);
        j.arrVal.push_back(std::move(elem));
    }
}

// std::unordered_set
template<typename T, typename... Args>
inline void spToJson(std::unordered_set<T, Args...> const& v, JsonVal& j) {
    j.type = JsonVal::Array;
    for (auto const& x : v) {
        JsonVal elem;
        spToJson(x, elem);
        j.arrVal.push_back(std::move(elem));
    }
}

// std::map (key 是 string 时输出为 object，否则输出为 array of pairs)
template<typename K, typename V, typename... Args>
inline void spToJson(std::map<K, V, Args...> const& v, JsonVal& j) {
    if constexpr (std::is_same_v<K, std::string>) {
        j.type = JsonVal::Object;
        for (auto const& [k, val] : v) {
            JsonVal elem;
            spToJson(val, elem);
            j.objVal[k] = std::move(elem);
        }
    } else {
        j.type = JsonVal::Array;
        for (auto const& [k, val] : v) {
            JsonVal pair;
            pair.type = JsonVal::Array;
            JsonVal jk, jv;
            spToJson(k, jk);
            spToJson(val, jv);
            pair.arrVal.push_back(std::move(jk));
            pair.arrVal.push_back(std::move(jv));
            j.arrVal.push_back(std::move(pair));
        }
    }
}

// std::unordered_map
template<typename K, typename V, typename... Args>
inline void spToJson(std::unordered_map<K, V, Args...> const& v, JsonVal& j) {
    if constexpr (std::is_same_v<K, std::string>) {
        j.type = JsonVal::Object;
        for (auto const& [k, val] : v) {
            JsonVal elem;
            spToJson(val, elem);
            j.objVal[k] = std::move(elem);
        }
    } else {
        j.type = JsonVal::Array;
        for (auto const& [k, val] : v) {
            JsonVal pair;
            pair.type = JsonVal::Array;
            JsonVal jk, jv;
            spToJson(k, jk);
            spToJson(val, jv);
            pair.arrVal.push_back(std::move(jk));
            pair.arrVal.push_back(std::move(jv));
            j.arrVal.push_back(std::move(pair));
        }
    }
}

// std::optional
template<typename T>
inline void spToJson(std::optional<T> const& v, JsonVal& j) {
    if (v.has_value()) {
        spToJson(v.value(), j);
    } else {
        j = JsonVal{};
    }
}

// std::filesystem::path
inline void spToJson(std::filesystem::path const& v, JsonVal& j) {
    j = JsonVal{v.string()};
}

// std::atomic
template<typename T>
inline void spToJson(std::atomic<T> const& v, JsonVal& j) {
    spToJson(v.load(std::memory_order_acquire), j);
}

// std::chrono::time_point -> 纪元毫秒 (i64)
template<typename Clock, typename Duration>
inline void spToJson(std::chrono::time_point<Clock, Duration> const& tp, JsonVal& j) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    j = JsonVal{static_cast<i64>(ms)};
}

// std::chrono::duration -> 纳秒 (i64)
template<typename Rep, typename Period>
inline void spToJson(std::chrono::duration<Rep, Period> const& dur, JsonVal& j) {
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count();
    j = JsonVal{static_cast<i64>(ns)};
}

// std::bitset -> 字符串
template<size_t N>
inline void spToJson(std::bitset<N> const& v, JsonVal& j) {
    j = JsonVal{v.to_string()};
}

// std::variant
template<typename... Args>
inline void spToJson(std::variant<Args...> const& v, JsonVal& j) {
    j.type = JsonVal::Object;
    j.objVal["index"] = JsonVal{static_cast<i64>(v.index())};
    std::visit([&j](auto&& arg) { spToJson(arg, j.objVal["value"]); }, v);
}

// std::tuple -> 数组
template<typename... Args>
inline void spToJson(std::tuple<Args...> const& v, JsonVal& j) {
    j.type = JsonVal::Array;
    std::apply([&j](auto&&... args) {
        (([]<typename T>(JsonVal& j, T&& arg) {
            JsonVal elem;
            spToJson(std::forward<T>(arg), elem);
            j.arrVal.push_back(std::move(elem));
        }(j, std::forward<decltype(args)>(args))), ...);
    }, const_cast<std::tuple<Args...>&>(v));
}

// ============================== 多态类型注册表（抽象指针反序列化） ==============================

using JsonTypeFactory = std::function<Base*()>;

inline std::unordered_map<std::string, JsonTypeFactory>& getJsonTypeRegistry() {
    static std::unordered_map<std::string, JsonTypeFactory> reg;
    return reg;
}

#define REGISTER_JSON_TYPE(TypeName) \
    inline static bool _json_reg_##TypeName = []() { \
        getJsonTypeRegistry()[#TypeName] = []() -> Base* { return new TypeName{}; }; \
        return true; \
    }()

// 原始指针（展开）
template<typename T>
inline void spToJson(T* const v, JsonVal& j) {
    if (v == nullptr) { j = JsonVal{}; }
    else if constexpr (std::is_abstract_v<T>) {
        j["_type"] = v->getClassName();
        v->toJson(j["_data"]);
    }
    else { spToJson(*v, j); }
}

// Sptr
template<typename T>
inline void spToJson(Sptr<T> const& v, JsonVal& j) {
    if (v == nullptr) { j = JsonVal{}; }
    else if constexpr (std::is_abstract_v<T>) {
        j["_type"] = v->getClassName();
        v->toJson(j["_data"]);
    }
    else { spToJson(*v, j); }
}

// Uptr
template<typename T>
inline void spToJson(Uptr<T> const& v, JsonVal& j) {
    if (v == nullptr) { j = JsonVal{}; }
    else if constexpr (std::is_abstract_v<T>) {
        j["_type"] = v->getClassName();
        v->toJson(j["_data"]);
    }
    else { spToJson(*v, j); }
}

// Wptr
template<typename T>
inline void spToJson(Wptr<T> const& v, JsonVal& j) {
    auto locked = v.lock();
    if (locked == nullptr) { j = JsonVal{}; }
    else { spToJson(*locked, j); }
}

// Base 子类（通过虚函数）
inline void spToJson(Base const& v, JsonVal& j) {
    v.toJson(j);
}

// ============================== spFromJson 反序列化函数 ==============================

// 基本类型
inline void spFromJson(JsonVal const& j, bool& v)    { v = json_cast<bool>(j); }
inline void spFromJson(JsonVal const& j, i8& v)      { v = json_cast<i8>(j); }
inline void spFromJson(JsonVal const& j, i16& v)     { v = json_cast<i16>(j); }
inline void spFromJson(JsonVal const& j, i32& v)     { v = json_cast<i32>(j); }
inline void spFromJson(JsonVal const& j, i64& v)     { v = json_cast<i64>(j); }
inline void spFromJson(JsonVal const& j, u8& v)      { v = json_cast<u8>(j); }
inline void spFromJson(JsonVal const& j, u16& v)     { v = json_cast<u16>(j); }
inline void spFromJson(JsonVal const& j, u32& v)     { v = json_cast<u32>(j); }
inline void spFromJson(JsonVal const& j, u64& v)     { v = json_cast<u64>(j); }
inline void spFromJson(JsonVal const& j, f32& v)     { v = json_cast<f32>(j); }
inline void spFromJson(JsonVal const& j, f64& v)     { v = json_cast<f64>(j); }
inline void spFromJson(JsonVal const& j, ch& v)      { v = json_cast<ch>(j); }
inline void spFromJson(JsonVal const& j, ch8& v)     { v = json_cast<ch8>(j); }
inline void spFromJson(JsonVal const& j, ch16& v)    { v = json_cast<ch16>(j); }
inline void spFromJson(JsonVal const& j, ch32& v)    { v = json_cast<ch32>(j); }

// enum
template<typename T> std::enable_if_t<std::is_enum_v<T>> spFromJson(JsonVal const& j, T& v) {
    v = static_cast<T>(json_cast<i64>(j));
}

// std::string
template<typename T, typename... Args>
inline void spFromJson(JsonVal const& j, std::basic_string<T, Args...>& v) {
    if constexpr (std::is_same_v<T, char>) {
        v = j.type == JsonVal::String ? j.strVal : j.dump();
    } else {
        v.clear();
        if (j.type == JsonVal::Array) {
            for (auto const& elem : j.arrVal) {
                v.push_back(static_cast<T>(json_cast<i64>(elem)));
            }
        }
    }
}

// std::vector
template<typename T, typename... Args>
inline void spFromJson(JsonVal const& j, std::vector<T, Args...>& v) {
    v.clear();
    if (j.type == JsonVal::Array) {
        for (auto const& elem : j.arrVal) {
            T val{};
            spFromJson(elem, val);
            v.push_back(std::move(val));
        }
    }
}

// std::array
template<typename T, size_t N>
inline void spFromJson(JsonVal const& j, std::array<T, N>& v) {
    if (j.type == JsonVal::Array) {
        size_t n = std::min(j.arrVal.size(), N);
        for (size_t i = 0; i < n; ++i) {
            spFromJson(j.arrVal[i], v[i]);
        }
    }
}

// std::deque
template<typename T, typename... Args>
inline void spFromJson(JsonVal const& j, std::deque<T, Args...>& v) {
    v.clear();
    if (j.type == JsonVal::Array) {
        for (auto const& elem : j.arrVal) {
            T val{};
            spFromJson(elem, val);
            v.push_back(std::move(val));
        }
    }
}

// std::list
template<typename T, typename... Args>
inline void spFromJson(JsonVal const& j, std::list<T, Args...>& v) {
    v.clear();
    if (j.type == JsonVal::Array) {
        for (auto const& elem : j.arrVal) {
            T val{};
            spFromJson(elem, val);
            v.push_back(std::move(val));
        }
    }
}

// std::forward_list
template<typename T, typename... Args>
inline void spFromJson(JsonVal const& j, std::forward_list<T, Args...>& v) {
    v.clear();
    if (j.type == JsonVal::Array) {
        std::vector<T> tmp;
        for (auto const& elem : j.arrVal) {
            T val{};
            spFromJson(elem, val);
            tmp.push_back(std::move(val));
        }
        for (auto it = tmp.rbegin(); it != tmp.rend(); ++it) {
            v.push_front(std::move(*it));
        }
    }
}

// std::set
template<typename T, typename... Args>
inline void spFromJson(JsonVal const& j, std::set<T, Args...>& v) {
    v.clear();
    if (j.type == JsonVal::Array) {
        for (auto const& elem : j.arrVal) {
            T val{};
            spFromJson(elem, val);
            v.emplace(std::move(val));
        }
    }
}

// std::unordered_set
template<typename T, typename... Args>
inline void spFromJson(JsonVal const& j, std::unordered_set<T, Args...>& v) {
    v.clear();
    if (j.type == JsonVal::Array) {
        for (auto const& elem : j.arrVal) {
            T val{};
            spFromJson(elem, val);
            v.emplace(std::move(val));
        }
    }
}

// std::map
template<typename K, typename V, typename... Args>
inline void spFromJson(JsonVal const& j, std::map<K, V, Args...>& v) {
    v.clear();
    if constexpr (std::is_same_v<K, std::string>) {
        if (j.type == JsonVal::Object) {
            for (auto const& [k, jv] : j.objVal) {
                V val{};
                spFromJson(jv, val);
                v[k] = std::move(val);
            }
        }
    } else {
        if (j.type == JsonVal::Array) {
            for (auto const& pair : j.arrVal) {
                if (pair.type == JsonVal::Array && pair.arrVal.size() >= 2) {
                    K k{};
                    V val{};
                    spFromJson(pair.arrVal[0], k);
                    spFromJson(pair.arrVal[1], val);
                    v[k] = std::move(val);
                }
            }
        }
    }
}

// std::unordered_map
template<typename K, typename V, typename... Args>
inline void spFromJson(JsonVal const& j, std::unordered_map<K, V, Args...>& v) {
    v.clear();
    if constexpr (std::is_same_v<K, std::string>) {
        if (j.type == JsonVal::Object) {
            for (auto const& [k, jv] : j.objVal) {
                V val{};
                spFromJson(jv, val);
                v[k] = std::move(val);
            }
        }
    } else {
        if (j.type == JsonVal::Array) {
            for (auto const& pair : j.arrVal) {
                if (pair.type == JsonVal::Array && pair.arrVal.size() >= 2) {
                    K k{};
                    V val{};
                    spFromJson(pair.arrVal[0], k);
                    spFromJson(pair.arrVal[1], val);
                    v[k] = std::move(val);
                }
            }
        }
    }
}

// std::optional
template<typename T>
inline void spFromJson(JsonVal const& j, std::optional<T>& v) {
    if (j.type == JsonVal::Null) {
        v.reset();
    } else {
        T val{};
        spFromJson(j, val);
        v = std::move(val);
    }
}

// std::filesystem::path
inline void spFromJson(JsonVal const& j, std::filesystem::path& v) {
    v = j.type == JsonVal::String ? j.strVal : j.dump();
}

// std::atomic
template<typename T>
inline void spFromJson(JsonVal const& j, std::atomic<T>& v) {
    T val{};
    spFromJson(j, val);
    v.store(val, std::memory_order_release);
}

// std::chrono::time_point
template<typename Clock, typename Duration>
inline void spFromJson(JsonVal const& j, std::chrono::time_point<Clock, Duration>& tp) {
    i64 ms = json_cast<i64>(j);
    tp = std::chrono::time_point<Clock, Duration>(std::chrono::milliseconds(ms));
}

// std::chrono::duration
template<typename Rep, typename Period>
inline void spFromJson(JsonVal const& j, std::chrono::duration<Rep, Period>& dur) {
    i64 ns = json_cast<i64>(j);
    dur = std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(std::chrono::nanoseconds(ns));
}

// std::bitset
template<size_t N>
inline void spFromJson(JsonVal const& j, std::bitset<N>& v) {
    if (j.type == JsonVal::String) {
        v = std::bitset<N>(j.strVal);
    }
}

// std::variant
namespace detail {
    template<size_t CurrIdx = 0, typename... Args>
    inline void fromJsonVariant(JsonVal const& j, std::variant<Args...>& v, size_t idx) {
        constexpr size_t sz = sizeof...(Args);
        if (idx == CurrIdx) {
            using T = std::variant_alternative_t<CurrIdx, std::variant<Args...>>;
            T val{};
            if (j.type == JsonVal::Object && j.has_key("value")) {
                spFromJson(j["value"], val);
            } else {
                spFromJson(j, val);
            }
            v = std::move(val);
            return;
        }
        if constexpr (CurrIdx + 1 < sz) {
            fromJsonVariant<CurrIdx + 1>(j, v, idx);
        }
    }
}

template<typename... Args>
inline void spFromJson(JsonVal const& j, std::variant<Args...>& v) {
    size_t idx = 0;
    if (j.type == JsonVal::Object && j.has_key("index")) {
        idx = static_cast<size_t>(json_cast<i64>(j["index"]));
    }
    detail::fromJsonVariant(j, v, idx);
}

// std::tuple
namespace detail {
    template<typename Tuple, size_t... Is>
    inline void fromJsonTuple(JsonVal const& j, Tuple& t, std::index_sequence<Is...>) {
        if (j.type == JsonVal::Array) {
            ((spFromJson(j.arrVal.size() > Is ? j.arrVal[Is] : JsonVal{}, std::get<Is>(t))), ...);
        }
    }
}

template<typename... Args>
inline void spFromJson(JsonVal const& j, std::tuple<Args...>& v) {
    detail::fromJsonTuple(j, v, std::make_index_sequence<sizeof...(Args)>{});
}

// 原始指针（展开）
template<typename T>
inline void spFromJson(JsonVal const& j, T*& v) {
    if (j.type == JsonVal::Null) {
        delete v;
        v = nullptr;
    } else if constexpr (std::is_abstract_v<T>) {
        delete v;
        auto it = getJsonTypeRegistry().find(j["_type"].strVal);
        if (it != getJsonTypeRegistry().end()) {
            v = static_cast<T*>(it->second());
            v->fromJson(j["_data"]);
        } else {
            throw SpDataError("spFromJson: unknown type '" + j["_type"].strVal + "'");
        }
    } else {
        if (v == nullptr) v = new T{};
        spFromJson(j, *v);
    }
}

// Sptr
template<typename T>
inline void spFromJson(JsonVal const& j, Sptr<T>& v) {
    if (j.type == JsonVal::Null) {
        v.reset();
    } else if constexpr (std::is_abstract_v<T>) {
        auto it = getJsonTypeRegistry().find(j["_type"].strVal);
        if (it != getJsonTypeRegistry().end()) {
            v.reset(static_cast<T*>(it->second()));
            v->fromJson(j["_data"]);
        } else {
            throw SpDataError("spFromJson: unknown type '" + j["_type"].strVal + "'");
        }
    } else {
        if (v == nullptr) v = std::make_shared<T>();
        spFromJson(j, *v);
    }
}

// Uptr
template<typename T>
inline void spFromJson(JsonVal const& j, Uptr<T>& v) {
    if (j.type == JsonVal::Null) {
        v.reset();
    } else if constexpr (std::is_abstract_v<T>) {
        auto it = getJsonTypeRegistry().find(j["_type"].strVal);
        if (it != getJsonTypeRegistry().end()) {
            v.reset(static_cast<T*>(it->second()));
            v->fromJson(j["_data"]);
        } else {
            throw SpDataError("spFromJson: unknown type '" + j["_type"].strVal + "'");
        }
    } else {
        if (v == nullptr) v = std::make_unique<T>();
        spFromJson(j, *v);
    }
}

// Wptr (反序列化时无法还原，跳过)
template<typename T>
inline void spFromJson(JsonVal const& j, Wptr<T>& v) {
    // weak_ptr 需要 shared_ptr 来初始化，反序列化时无法可靠还原
    // 仅在非 null 时尝试用已有的 shared_ptr 去还原
    if (j.type == JsonVal::Null) {
        v.reset();
    }
    // 否则保持原样
}

// Base 子类（通过虚函数）
inline void spFromJson(JsonVal const& j, Base& v) {
    v.fromJson(j);
}

// ============================== UseDataJson 宏（同时生成 DOM 和流式 JSON 方法） ==============================

// 为每个成员生成 JSON 输出代码（DOM）
#define X_jsonOut(type, name, ...) spToJson(obj.name, j[#name]);

// 为每个成员生成 JSON 输入代码（DOM）
#define X_jsonIn(type, name, ...) if (j.has_key(#name)) spFromJson(j[#name], obj.name);

// 为每个成员生成流式 JSON 输出代码
#define X_jsonStreamOut(type, name, ...) \
    if (!_first) os << ','; \
    _first = false; \
    spJsonWriteString(os, #name); \
    os << ':'; \
    spToJsonStream(obj.name, os);

// 为每个成员生成流式 JSON 输入代码
#define X_jsonStreamIn(type, name, ...) \
    if (key == #name) { spFromJsonStream(r, obj.name); return true; }

// 为有基类的类型同时生成 DOM 和流式 JSON 方法
#define UseDataJsonXtBase(TypeName, Xt, Base__) \
    /* DOM 模式 */ \
    void toJson(JsonVal& j) const override { \
        TypeName const& obj = *this; \
        Base__::toJson(j); \
        Xt(X_jsonOut) \
    } \
    void fromJson(JsonVal const& j) override { \
        TypeName& obj = *this; \
        Base__::fromJson(j); \
        Xt(X_jsonIn) \
    } \
    /* 流式模式 */ \
    void toJsonStream(std::ostream& os) const override { \
        os << '{'; \
        bool _first = true; \
        writeJsonStreamMembers(os, _first); \
        os << '}'; \
    } \
    void writeJsonStreamMembers(std::ostream& os, bool& _first) const override { \
        TypeName const& obj = *this; \
        Base__::writeJsonStreamMembers(os, _first); \
        Xt(X_jsonStreamOut) \
    } \
    void fromJsonStream(JsonStreamReader& r) override { \
        TypeName& obj = *this; \
        r.expect('{'); \
        if (r.peek() == '}') { r.next(); return; } \
        do { \
            std::string key = r.readString(); \
            r.expect(':'); \
            if (!readJsonStreamMember(r, key)) { r.skipValue(); } \
        } while (r.hasMoreMembers()); \
    } \
    bool readJsonStreamMember(JsonStreamReader& r, std::string const& key) override { \
        TypeName& obj = *this; \
        if (Base__::readJsonStreamMember(r, key)) return true; \
        Xt(X_jsonStreamIn) \
        return false; \
    }

#define UseDataJsonXt(TypeName, Xt) UseDataJsonXtBase(TypeName, Xt, Base)
#define UseDataJsonBase(TypeName, Base__) UseDataJsonXtBase(TypeName, Xt_##TypeName, Base__)
#define UseDataJson(TypeName) UseDataJsonXtBase(TypeName, Xt_##TypeName, Base)

// ============================== 流式 JSON 写入 ==============================

// 将字符串写入流（带 JSON 转义）
inline void spJsonWriteString(std::ostream& os, std::string const& s) {
    os << '"';
    for (char c : s) {
        switch (c) {
        case '"':  os << "\\\""; break;
        case '\\': os << "\\\\"; break;
        case '\b': os << "\\b";  break;
        case '\f': os << "\\f";  break;
        case '\n': os << "\\n";  break;
        case '\r': os << "\\r";  break;
        case '\t': os << "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                os << "\\u00" << "0123456789abcdef"[(c >> 4) & 0xF] << "0123456789abcdef"[c & 0xF];
            } else {
                os << c;
            }
        }
    }
    os << '"';
}

// 将浮点数写入流（处理 NaN 和 Inf）
inline void spJsonWriteFloat(std::ostream& os, f64 v) {
    if (std::isnan(v)) os << "null";
    else if (std::isinf(v)) os << (v > 0 ? "1e999" : "-1e999");
    else {
        char buf[64];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::general, 17);
        if (ec == std::errc{}) os.write(buf, ptr - buf);
        else os << v;
    }
}

// ===================== spToJsonStream 基本类型 =====================

// 将整数写入流（不受 locale 影响，不使用千位分隔符）
inline void spJsonWriteInt(std::ostream& os, i64 v) { detail::jsonWriteInt(os, v); }
inline void spJsonWriteUint(std::ostream& os, u64 v) { detail::jsonWriteUint(os, v); }

inline void spToJsonStream(bool v, std::ostream& os)           { os << (v ? "true" : "false"); }
inline void spToJsonStream(i8 v, std::ostream& os)             { spJsonWriteInt(os, static_cast<i64>(v)); }
inline void spToJsonStream(i16 v, std::ostream& os)            { spJsonWriteInt(os, static_cast<i64>(v)); }
inline void spToJsonStream(i32 v, std::ostream& os)            { spJsonWriteInt(os, static_cast<i64>(v)); }
inline void spToJsonStream(i64 v, std::ostream& os)            { spJsonWriteInt(os, v); }
inline void spToJsonStream(u8 v, std::ostream& os)             { spJsonWriteUint(os, static_cast<u64>(v)); }
inline void spToJsonStream(u16 v, std::ostream& os)            { spJsonWriteUint(os, static_cast<u64>(v)); }
inline void spToJsonStream(u32 v, std::ostream& os)            { spJsonWriteUint(os, static_cast<u64>(v)); }
inline void spToJsonStream(u64 v, std::ostream& os)            { spJsonWriteUint(os, v); }
inline void spToJsonStream(f32 v, std::ostream& os)            { spJsonWriteFloat(os, static_cast<f64>(v)); }
inline void spToJsonStream(f64 v, std::ostream& os)            { spJsonWriteFloat(os, v); }
inline void spToJsonStream(ch v, std::ostream& os)             { spJsonWriteString(os, std::string(1, v)); }
inline void spToJsonStream(ch8 v, std::ostream& os)            { spJsonWriteString(os, std::string(1, static_cast<char>(v))); }
inline void spToJsonStream(ch16 v, std::ostream& os)           { spJsonWriteInt(os, static_cast<i64>(v)); }
inline void spToJsonStream(ch32 v, std::ostream& os)           { spJsonWriteInt(os, static_cast<i64>(v)); }

// enum
template<typename T> std::enable_if_t<std::is_enum_v<T>> spToJsonStream(T v, std::ostream& os) {
    spJsonWriteInt(os, static_cast<i64>(v));
}

// std::string / std::basic_string
template<typename T, typename... Args>
inline void spToJsonStream(std::basic_string<T, Args...> const& v, std::ostream& os) {
    if constexpr (std::is_same_v<T, char>) {
        spJsonWriteString(os, v);
    } else {
        os << '[';
        bool first = true;
        for (auto c : v) {
            if (!first) os << ',';
            first = false;
            spJsonWriteInt(os, static_cast<i64>(c));
        }
        os << ']';
    }
}

// ===================== 容器类型前向声明 =====================

// std::vector
template<typename T, typename... Args>
inline void spToJsonStream(std::vector<T, Args...> const& v, std::ostream& os);

// std::array
template<typename T, size_t N>
inline void spToJsonStream(std::array<T, N> const& v, std::ostream& os);

// std::deque
template<typename T, typename... Args>
inline void spToJsonStream(std::deque<T, Args...> const& v, std::ostream& os);

// std::list
template<typename T, typename... Args>
inline void spToJsonStream(std::list<T, Args...> const& v, std::ostream& os);

// std::forward_list
template<typename T, typename... Args>
inline void spToJsonStream(std::forward_list<T, Args...> const& v, std::ostream& os);

// std::set
template<typename T, typename... Args>
inline void spToJsonStream(std::set<T, Args...> const& v, std::ostream& os);

// std::unordered_set
template<typename T, typename... Args>
inline void spToJsonStream(std::unordered_set<T, Args...> const& v, std::ostream& os);

// std::map
template<typename K, typename V, typename... Args>
inline void spToJsonStream(std::map<K, V, Args...> const& v, std::ostream& os);

// std::unordered_map
template<typename K, typename V, typename... Args>
inline void spToJsonStream(std::unordered_map<K, V, Args...> const& v, std::ostream& os);

// std::optional
template<typename T>
inline void spToJsonStream(std::optional<T> const& v, std::ostream& os);

// std::variant
template<typename... Ts>
inline void spToJsonStream(std::variant<Ts...> const& v, std::ostream& os);

// std::tuple
template<typename... Ts>
inline void spToJsonStream(std::tuple<Ts...> const& v, std::ostream& os);

// 原始指针
template<typename T>
inline void spToJsonStream(T* const v, std::ostream& os);

// Sptr / Uptr / Wptr
template<typename T>
inline void spToJsonStream(Sptr<T> const& v, std::ostream& os);
template<typename T>
inline void spToJsonStream(Uptr<T> const& v, std::ostream& os);
template<typename T>
inline void spToJsonStream(Wptr<T> const& v, std::ostream& os);

// Base
inline void spToJsonStream(Base const& v, std::ostream& os);

// std::bitset
template<size_t N>
inline void spToJsonStream(std::bitset<N> const& v, std::ostream& os);

// std::filesystem::path
inline void spToJsonStream(std::filesystem::path const& v, std::ostream& os);

// std::atomic
template<typename T>
inline void spToJsonStream(std::atomic<T> const& v, std::ostream& os);

// std::chrono
template<typename Clock, typename Duration>
inline void spToJsonStream(std::chrono::time_point<Clock, Duration> const& tp, std::ostream& os);
template<typename Rep, typename Period>
inline void spToJsonStream(std::chrono::duration<Rep, Period> const& dur, std::ostream& os);

// ===================== 容器类型实现 =====================

// std::vector
template<typename T, typename... Args>
inline void spToJsonStream(std::vector<T, Args...> const& v, std::ostream& os) {
    os << '[';
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) os << ',';
        spToJsonStream(v[i], os);
    }
    os << ']';
}

// std::array
template<typename T, size_t N>
inline void spToJsonStream(std::array<T, N> const& v, std::ostream& os) {
    os << '[';
    for (size_t i = 0; i < N; ++i) {
        if (i > 0) os << ',';
        spToJsonStream(v[i], os);
    }
    os << ']';
}

// std::deque
template<typename T, typename... Args>
inline void spToJsonStream(std::deque<T, Args...> const& v, std::ostream& os) {
    os << '[';
    size_t i = 0;
    for (auto const& x : v) {
        if (i++ > 0) os << ',';
        spToJsonStream(x, os);
    }
    os << ']';
}

// std::list
template<typename T, typename... Args>
inline void spToJsonStream(std::list<T, Args...> const& v, std::ostream& os) {
    os << '[';
    size_t i = 0;
    for (auto const& x : v) {
        if (i++ > 0) os << ',';
        spToJsonStream(x, os);
    }
    os << ']';
}

// std::forward_list
template<typename T, typename... Args>
inline void spToJsonStream(std::forward_list<T, Args...> const& v, std::ostream& os) {
    os << '[';
    size_t i = 0;
    for (auto const& x : v) {
        if (i++ > 0) os << ',';
        spToJsonStream(x, os);
    }
    os << ']';
}

// std::set
template<typename T, typename... Args>
inline void spToJsonStream(std::set<T, Args...> const& v, std::ostream& os) {
    os << '[';
    size_t i = 0;
    for (auto const& x : v) {
        if (i++ > 0) os << ',';
        spToJsonStream(x, os);
    }
    os << ']';
}

// std::unordered_set
template<typename T, typename... Args>
inline void spToJsonStream(std::unordered_set<T, Args...> const& v, std::ostream& os) {
    os << '[';
    size_t i = 0;
    for (auto const& x : v) {
        if (i++ > 0) os << ',';
        spToJsonStream(x, os);
    }
    os << ']';
}

// std::map (string key -> object, otherwise -> array of pairs)
template<typename K, typename V, typename... Args>
inline void spToJsonStream(std::map<K, V, Args...> const& v, std::ostream& os) {
    if constexpr (std::is_same_v<K, std::string>) {
        os << '{';
        size_t i = 0;
        for (auto const& [k, val] : v) {
            if (i++ > 0) os << ',';
            spJsonWriteString(os, k);
            os << ':';
            spToJsonStream(val, os);
        }
        os << '}';
    } else {
        os << '[';
        size_t i = 0;
        for (auto const& [k, val] : v) {
            if (i++ > 0) os << ',';
            os << '[';
            spToJsonStream(k, os);
            os << ',';
            spToJsonStream(val, os);
            os << ']';
        }
        os << ']';
    }
}

// std::unordered_map
template<typename K, typename V, typename... Args>
inline void spToJsonStream(std::unordered_map<K, V, Args...> const& v, std::ostream& os) {
    if constexpr (std::is_same_v<K, std::string>) {
        os << '{';
        size_t i = 0;
        for (auto const& [k, val] : v) {
            if (i++ > 0) os << ',';
            spJsonWriteString(os, k);
            os << ':';
            spToJsonStream(val, os);
        }
        os << '}';
    } else {
        os << '[';
        size_t i = 0;
        for (auto const& [k, val] : v) {
            if (i++ > 0) os << ',';
            os << '[';
            spToJsonStream(k, os);
            os << ',';
            spToJsonStream(val, os);
            os << ']';
        }
        os << ']';
    }
}

// std::optional
template<typename T>
inline void spToJsonStream(std::optional<T> const& v, std::ostream& os) {
    if (v.has_value()) {
        spToJsonStream(v.value(), os);
    } else {
        os << "null";
    }
}

// std::filesystem::path
inline void spToJsonStream(std::filesystem::path const& v, std::ostream& os) {
    spJsonWriteString(os, v.string());
}

// std::atomic
template<typename T>
inline void spToJsonStream(std::atomic<T> const& v, std::ostream& os) {
    spToJsonStream(v.load(std::memory_order_acquire), os);
}

// std::chrono::time_point
template<typename Clock, typename Duration>
inline void spToJsonStream(std::chrono::time_point<Clock, Duration> const& tp, std::ostream& os) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    spJsonWriteInt(os, static_cast<i64>(ms));
}

// std::chrono::duration
template<typename Rep, typename Period>
inline void spToJsonStream(std::chrono::duration<Rep, Period> const& dur, std::ostream& os) {
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count();
    spJsonWriteInt(os, static_cast<i64>(ns));
}

// std::bitset
template<size_t N>
inline void spToJsonStream(std::bitset<N> const& v, std::ostream& os) {
    spJsonWriteString(os, v.to_string());
}

// std::variant
template<typename... Args>
inline void spToJsonStream(std::variant<Args...> const& v, std::ostream& os) {
    os << "{\"index\":" << v.index() << ",\"value\":";
    std::visit([&os](auto&& arg) { spToJsonStream(arg, os); }, v);
    os << '}';
}

// std::tuple
template<typename... Args>
inline void spToJsonStream(std::tuple<Args...> const& v, std::ostream& os) {
    os << '[';
    bool first = true;
    std::apply([&](auto&&... args) {
        ((first ? (first = false, os << "") : (os << ','), spToJsonStream(args, os)), ...);
    }, v);
    os << ']';
}

// 原始指针
template<typename T>
inline void spToJsonStream(T* const v, std::ostream& os) {
    if (v == nullptr) { os << "null"; }
    else if constexpr (std::is_abstract_v<T>) {
        os << "{\"_type\":";
        spJsonWriteString(os, v->getClassName());
        os << ",\"_data\":";
        v->toJsonStream(os);
        os << '}';
    }
    else { spToJsonStream(*v, os); }
}

// Sptr
template<typename T>
inline void spToJsonStream(Sptr<T> const& v, std::ostream& os) {
    if (v == nullptr) { os << "null"; }
    else if constexpr (std::is_abstract_v<T>) {
        os << "{\"_type\":";
        spJsonWriteString(os, v->getClassName());
        os << ",\"_data\":";
        v->toJsonStream(os);
        os << '}';
    }
    else { spToJsonStream(*v, os); }
}

// Uptr
template<typename T>
inline void spToJsonStream(Uptr<T> const& v, std::ostream& os) {
    if (v == nullptr) { os << "null"; }
    else if constexpr (std::is_abstract_v<T>) {
        os << "{\"_type\":";
        spJsonWriteString(os, v->getClassName());
        os << ",\"_data\":";
        v->toJsonStream(os);
        os << '}';
    }
    else { spToJsonStream(*v, os); }
}

// Wptr
template<typename T>
inline void spToJsonStream(Wptr<T> const& v, std::ostream& os) {
    auto locked = v.lock();
    if (locked == nullptr) { os << "null"; }
    else { spToJsonStream(*locked, os); }
}

// Base 子类（通过虚函数）
inline void spToJsonStream(Base const& v, std::ostream& os) {
    v.toJsonStream(os);
}

// ============================== 流式 JSON 读取 ==============================

struct JsonStreamReader {
    std::string_view sv;
    size_t pos = 0;

    void skipWhitespace() {
        while (pos < sv.size() && std::isspace(static_cast<unsigned char>(sv[pos]))) {
            ++pos;
        }
    }

    char peek() {
        skipWhitespace();
        if (pos >= sv.size()) throw SpDataError("JSON stream parse: unexpected end of input");
        return sv[pos];
    }

    char next() {
        skipWhitespace();
        if (pos >= sv.size()) throw SpDataError("JSON stream parse: unexpected end of input");
        return sv[pos++];
    }

    void expect(char c) {
        char actual = next();
        if (actual != c) {
            throw SpDataError(std::string("JSON stream parse: expected '") + c + "' but got '" + actual + "'");
        }
    }

    bool tryConsume(char c) {
        skipWhitespace();
        if (pos < sv.size() && sv[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }

    std::string readString() {
        expect('"');
        std::string s;
        while (pos < sv.size()) {
            char c = sv[pos++];
            if (c == '"') return s;
            if (c == '\\') {
                if (pos >= sv.size()) throw SpDataError("JSON stream parse: unexpected end in string escape");
                char e = sv[pos++];
                switch (e) {
                case '"':  s += '"';  break;
                case '\\': s += '\\'; break;
                case '/':  s += '/';  break;
                case 'b':  s += '\b'; break;
                case 'f':  s += '\f'; break;
                case 'n':  s += '\n'; break;
                case 'r':  s += '\r'; break;
                case 't':  s += '\t'; break;
                case 'u': {
                    if (pos + 4 > sv.size()) throw SpDataError("JSON stream parse: unexpected end in \\u escape");
                    uint16_t cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = sv[pos++];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                        else throw SpDataError("JSON stream parse: invalid hex in \\u escape");
                    }
                    if (cp < 0x80) s += static_cast<char>(cp);
                    else s += '?';
                    break;
                }
                default: throw SpDataError("JSON stream parse: invalid escape character");
                }
            } else {
                s += c;
            }
        }
        throw SpDataError("JSON stream parse: unterminated string");
    }

    // 读取数字（返回字符串，由调用者决定如何解析）
    std::string readNumberRaw() {
        skipWhitespace();
        size_t start = pos;
        if (pos < sv.size() && sv[pos] == '-') ++pos;
        while (pos < sv.size() && std::isdigit(static_cast<unsigned char>(sv[pos]))) ++pos;
        if (pos < sv.size() && sv[pos] == '.') {
            ++pos;
            while (pos < sv.size() && std::isdigit(static_cast<unsigned char>(sv[pos]))) ++pos;
        }
        if (pos < sv.size() && (sv[pos] == 'e' || sv[pos] == 'E')) {
            ++pos;
            if (pos < sv.size() && (sv[pos] == '+' || sv[pos] == '-')) ++pos;
            while (pos < sv.size() && std::isdigit(static_cast<unsigned char>(sv[pos]))) ++pos;
        }
        return std::string(sv.substr(start, pos - start));
    }

    // 跳过任意 JSON 值（递归）
    void skipValue() {
        char c = peek();
        switch (c) {
        case 'n': if (sv.substr(pos, 4) != "null") throw SpDataError("JSON stream: expected 'null'");
                  pos += 4; break;
        case 't': if (sv.substr(pos, 4) != "true") throw SpDataError("JSON stream: expected 'true'");
                  pos += 4; break;
        case 'f': if (sv.substr(pos, 5) != "false") throw SpDataError("JSON stream: expected 'false'");
                  pos += 5; break;
        case '"': readString(); break;
        case '[': {
            next(); // 跳过 '['
            skipWhitespace();
            if (pos < sv.size() && sv[pos] == ']') { ++pos; return; }
            while (true) {
                skipValue();
                skipWhitespace();
                if (pos >= sv.size()) throw SpDataError("JSON stream: unterminated array");
                if (sv[pos] == ']') { ++pos; return; }
                if (sv[pos] == ',') { ++pos; continue; }
                throw SpDataError("JSON stream: expected ',' or ']' in array");
            }
        }
        case '{': {
            next(); // 跳过 '{'
            skipWhitespace();
            if (pos < sv.size() && sv[pos] == '}') { ++pos; return; }
            while (true) {
                readString(); // key
                expect(':');
                skipValue();
                skipWhitespace();
                if (pos >= sv.size()) throw SpDataError("JSON stream: unterminated object");
                if (sv[pos] == '}') { ++pos; return; }
                if (sv[pos] == ',') { ++pos; continue; }
                throw SpDataError("JSON stream: expected ',' or '}' in object");
            }
        }
        default:
            if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
                readNumberRaw();
            } else {
                throw SpDataError(std::string("JSON stream: unexpected character '") + c + "'");
            }
        }
    }

    // 检查数组/对象中是否还有更多元素
    bool hasMoreElements() {
        skipWhitespace();
        if (pos >= sv.size()) throw SpDataError("JSON stream: unterminated array");
        if (sv[pos] == ']') { ++pos; return false; }
        if (sv[pos] == ',') { ++pos; return true; }
        throw SpDataError("JSON stream: expected ',' or ']' in array");
    }

    bool hasMoreMembers() {
        skipWhitespace();
        if (pos >= sv.size()) throw SpDataError("JSON stream: unterminated object");
        if (sv[pos] == '}') { ++pos; return false; }
        if (sv[pos] == ',') { ++pos; return true; }
        throw SpDataError("JSON stream: expected ',' or '}' in object");
    }
};

// Base::fromJsonStream 默认实现（需要 JsonStreamReader 定义后）
inline void Base::fromJsonStream(JsonStreamReader& r) {
    r.expect('{');
    if (r.peek() == '}') { r.next(); return; }
    do {
        std::string key = r.readString();
        r.expect(':');
        if (!readJsonStreamMember(r, key)) {
            r.skipValue();
        }
    } while (r.hasMoreMembers());
}

// ===================== spFromJsonStream 基本类型 =====================

inline void spFromJsonStream(JsonStreamReader& r, bool& v) {
    char c = r.peek();
    if (c == 't') { if (r.sv.substr(r.pos, 4) != "true") throw SpDataError("JSON stream: expected 'true'"); r.pos += 4; v = true; }
    else if (c == 'f') { if (r.sv.substr(r.pos, 5) != "false") throw SpDataError("JSON stream: expected 'false'"); r.pos += 5; v = false; }
    else throw SpDataError("JSON stream: expected bool");
}

#define SP_JSON_STREAM_FROM_INT(T) \
inline void spFromJsonStream(JsonStreamReader& r, T& v) { \
    std::string num = r.readNumberRaw(); \
    try { v = static_cast<T>(std::stoll(num)); } \
    catch (...) { v = static_cast<T>(std::stoull(num)); } \
}
SP_JSON_STREAM_FROM_INT(i8)
SP_JSON_STREAM_FROM_INT(i16)
SP_JSON_STREAM_FROM_INT(i32)
SP_JSON_STREAM_FROM_INT(i64)
SP_JSON_STREAM_FROM_INT(u8)
SP_JSON_STREAM_FROM_INT(u16)
SP_JSON_STREAM_FROM_INT(u32)
SP_JSON_STREAM_FROM_INT(u64)
#undef SP_JSON_STREAM_FROM_INT

inline void spFromJsonStream(JsonStreamReader& r, f32& v) {
    std::string num = r.readNumberRaw();
    v = static_cast<f32>(std::stod(num));
}
inline void spFromJsonStream(JsonStreamReader& r, f64& v) {
    std::string num = r.readNumberRaw();
    v = std::stod(num);
}

inline void spFromJsonStream(JsonStreamReader& r, ch& v) {
    char c = r.peek();
    if (c == '"') {
        std::string s = r.readString();
        v = s.empty() ? '\0' : s[0];
    } else {
        v = static_cast<ch>(std::stoll(r.readNumberRaw()));
    }
}
inline void spFromJsonStream(JsonStreamReader& r, ch8& v) {
    char c = r.peek();
    if (c == '"') {
        std::string s = r.readString();
        v = s.empty() ? static_cast<ch8>(0) : static_cast<ch8>(s[0]);
    } else {
        v = static_cast<ch8>(std::stoll(r.readNumberRaw()));
    }
}
inline void spFromJsonStream(JsonStreamReader& r, ch16& v) {
    v = static_cast<ch16>(std::stoll(r.readNumberRaw()));
}
inline void spFromJsonStream(JsonStreamReader& r, ch32& v) {
    v = static_cast<ch32>(std::stoll(r.readNumberRaw()));
}

// enum
template<typename T> std::enable_if_t<std::is_enum_v<T>> spFromJsonStream(JsonStreamReader& r, T& v) {
    v = static_cast<T>(std::stoll(r.readNumberRaw()));
}

// std::string
template<typename T, typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::basic_string<T, Args...>& v) {
    if constexpr (std::is_same_v<T, char>) {
        v = r.readString();
    } else {
        v.clear();
        r.expect('[');
        if (r.tryConsume(']')) return;
        while (true) {
            v.push_back(static_cast<T>(std::stoll(r.readNumberRaw())));
            if (!r.hasMoreElements()) break;
        }
    }
}

// std::vector
template<typename T, typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::vector<T, Args...>& v) {
    v.clear();
    r.expect('[');
    if (r.tryConsume(']')) return;
    while (true) {
        T val{};
        spFromJsonStream(r, val);
        v.push_back(std::move(val));
        if (!r.hasMoreElements()) break;
    }
}

// std::array
template<typename T, size_t N>
inline void spFromJsonStream(JsonStreamReader& r, std::array<T, N>& v) {
    r.expect('[');
    if (r.tryConsume(']')) return;
    size_t i = 0;
    while (true) {
        if (i < N) spFromJsonStream(r, v[i]);
        else r.skipValue();
        ++i;
        if (!r.hasMoreElements()) break;
    }
}

// std::deque
template<typename T, typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::deque<T, Args...>& v) {
    v.clear();
    r.expect('[');
    if (r.tryConsume(']')) return;
    while (true) {
        T val{};
        spFromJsonStream(r, val);
        v.push_back(std::move(val));
        if (!r.hasMoreElements()) break;
    }
}

// std::list
template<typename T, typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::list<T, Args...>& v) {
    v.clear();
    r.expect('[');
    if (r.tryConsume(']')) return;
    while (true) {
        T val{};
        spFromJsonStream(r, val);
        v.push_back(std::move(val));
        if (!r.hasMoreElements()) break;
    }
}

// std::forward_list
template<typename T, typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::forward_list<T, Args...>& v) {
    v.clear();
    r.expect('[');
    if (r.tryConsume(']')) return;
    std::vector<T> tmp;
    while (true) {
        T val{};
        spFromJsonStream(r, val);
        tmp.push_back(std::move(val));
        if (!r.hasMoreElements()) break;
    }
    for (auto it = tmp.rbegin(); it != tmp.rend(); ++it) {
        v.push_front(std::move(*it));
    }
}

// std::set
template<typename T, typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::set<T, Args...>& v) {
    v.clear();
    r.expect('[');
    if (r.tryConsume(']')) return;
    while (true) {
        T val{};
        spFromJsonStream(r, val);
        v.emplace(std::move(val));
        if (!r.hasMoreElements()) break;
    }
}

// std::unordered_set
template<typename T, typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::unordered_set<T, Args...>& v) {
    v.clear();
    r.expect('[');
    if (r.tryConsume(']')) return;
    while (true) {
        T val{};
        spFromJsonStream(r, val);
        v.emplace(std::move(val));
        if (!r.hasMoreElements()) break;
    }
}

// std::map
template<typename K, typename V, typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::map<K, V, Args...>& v) {
    v.clear();
    if constexpr (std::is_same_v<K, std::string>) {
        r.expect('{');
        if (r.tryConsume('}')) return;
        while (true) {
            std::string key = r.readString();
            r.expect(':');
            V val{};
            spFromJsonStream(r, val);
            v[key] = std::move(val);
            if (!r.hasMoreMembers()) break;
        }
    } else {
        r.expect('[');
        if (r.tryConsume(']')) return;
        while (true) {
            r.expect('[');
            K k{};
            V val{};
            spFromJsonStream(r, k);
            r.expect(','); // 跳过逗号（由 spToJsonStream 写入）
            spFromJsonStream(r, val);
            r.expect(']');
            v[k] = std::move(val);
            if (!r.hasMoreElements()) break;
        }
    }
}

// std::unordered_map
template<typename K, typename V, typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::unordered_map<K, V, Args...>& v) {
    v.clear();
    if constexpr (std::is_same_v<K, std::string>) {
        r.expect('{');
        if (r.tryConsume('}')) return;
        while (true) {
            std::string key = r.readString();
            r.expect(':');
            V val{};
            spFromJsonStream(r, val);
            v[key] = std::move(val);
            if (!r.hasMoreMembers()) break;
        }
    } else {
        r.expect('[');
        if (r.tryConsume(']')) return;
        while (true) {
            r.expect('[');
            K k{};
            V val{};
            spFromJsonStream(r, k);
            r.expect(',');
            spFromJsonStream(r, val);
            r.expect(']');
            v[k] = std::move(val);
            if (!r.hasMoreElements()) break;
        }
    }
}

// std::optional
template<typename T>
inline void spFromJsonStream(JsonStreamReader& r, std::optional<T>& v) {
    char c = r.peek();
    if (c == 'n') {
        if (r.sv.substr(r.pos, 4) != "null") throw SpDataError("JSON stream: expected 'null'");
        r.pos += 4;
        v.reset();
    } else {
        T val{};
        spFromJsonStream(r, val);
        v = std::move(val);
    }
}

// std::filesystem::path
inline void spFromJsonStream(JsonStreamReader& r, std::filesystem::path& v) {
    v = r.readString();
}

// std::atomic
template<typename T>
inline void spFromJsonStream(JsonStreamReader& r, std::atomic<T>& v) {
    T val{};
    spFromJsonStream(r, val);
    v.store(val, std::memory_order_release);
}

// std::chrono::time_point
template<typename Clock, typename Duration>
inline void spFromJsonStream(JsonStreamReader& r, std::chrono::time_point<Clock, Duration>& tp) {
    i64 ms = std::stoll(r.readNumberRaw());
    tp = std::chrono::time_point<Clock, Duration>(std::chrono::milliseconds(ms));
}

// std::chrono::duration
template<typename Rep, typename Period>
inline void spFromJsonStream(JsonStreamReader& r, std::chrono::duration<Rep, Period>& dur) {
    i64 ns = std::stoll(r.readNumberRaw());
    dur = std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(std::chrono::nanoseconds(ns));
}

// std::bitset
template<size_t N>
inline void spFromJsonStream(JsonStreamReader& r, std::bitset<N>& v) {
    v = std::bitset<N>(r.readString());
}

// std::variant
namespace detail {
    template<size_t CurrIdx = 0, typename... Args>
    inline void fromJsonStreamVariant(JsonStreamReader& r, std::variant<Args...>& v, size_t idx) {
        constexpr size_t sz = sizeof...(Args);
        if (idx == CurrIdx) {
            using T = std::variant_alternative_t<CurrIdx, std::variant<Args...>>;
            T val{};
            spFromJsonStream(r, val);
            v = std::move(val);
            return;
        }
        if constexpr (CurrIdx + 1 < sz) {
            fromJsonStreamVariant<CurrIdx + 1>(r, v, idx);
        }
    }
}

template<typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::variant<Args...>& v) {
    r.expect('{');
    std::string key = r.readString(); // "index"
    r.expect(':');
    size_t idx = static_cast<size_t>(std::stoll(r.readNumberRaw()));
    r.expect(','); // 跳过逗号
    key = r.readString(); // "value"
    r.expect(':');
    detail::fromJsonStreamVariant(r, v, idx);
    r.expect('}');
}

// std::tuple
namespace detail {
    template<typename Tuple, size_t... Is>
    inline void fromJsonStreamTuple(JsonStreamReader& r, Tuple& t, std::index_sequence<Is...>) {
        r.expect('[');
        if (r.tryConsume(']')) return;
        size_t i = 0;
        while (true) {
            ((i == Is ? (spFromJsonStream(r, std::get<Is>(t)), void()) : void()), ...);
            ++i;
            if (!r.hasMoreElements()) break;
        }
    }
}

template<typename... Args>
inline void spFromJsonStream(JsonStreamReader& r, std::tuple<Args...>& v) {
    detail::fromJsonStreamTuple(r, v, std::make_index_sequence<sizeof...(Args)>{});
}

// 原始指针
template<typename T>
inline void spFromJsonStream(JsonStreamReader& r, T*& v) {
    char c = r.peek();
    if (c == 'n') {
        if (r.sv.substr(r.pos, 4) != "null") throw SpDataError("JSON stream: expected 'null'");
        r.pos += 4;
        delete v;
        v = nullptr;
    } else if constexpr (std::is_abstract_v<T>) {
        // 读取 {"_type":"...","_data":{...}}
        r.expect('{');
        std::string key = r.readString(); // "_type"
        r.expect(':');
        std::string typeName = r.readString();
        r.expect(',');
        key = r.readString(); // "_data"
        r.expect(':');
        delete v;
        auto it = getJsonTypeRegistry().find(typeName);
        if (it != getJsonTypeRegistry().end()) {
            v = static_cast<T*>(it->second());
            spFromJsonStream(r, *v);
        } else {
            throw SpDataError("JSON stream: unknown type '" + typeName + "'");
        }
        r.expect('}');
    } else {
        if (v == nullptr) v = new T{};
        spFromJsonStream(r, *v);
    }
}

// Sptr
template<typename T>
inline void spFromJsonStream(JsonStreamReader& r, Sptr<T>& v) {
    char c = r.peek();
    if (c == 'n') {
        if (r.sv.substr(r.pos, 4) != "null") throw SpDataError("JSON stream: expected 'null'");
        r.pos += 4;
        v.reset();
    } else if constexpr (std::is_abstract_v<T>) {
        r.expect('{');
        std::string key = r.readString(); // "_type"
        r.expect(':');
        std::string typeName = r.readString();
        r.expect(',');
        key = r.readString(); // "_data"
        r.expect(':');
        auto it = getJsonTypeRegistry().find(typeName);
        if (it != getJsonTypeRegistry().end()) {
            v.reset(static_cast<T*>(it->second()));
            spFromJsonStream(r, *v);
        } else {
            throw SpDataError("JSON stream: unknown type '" + typeName + "'");
        }
        r.expect('}');
    } else {
        if (v == nullptr) v = std::make_shared<T>();
        spFromJsonStream(r, *v);
    }
}

// Uptr
template<typename T>
inline void spFromJsonStream(JsonStreamReader& r, Uptr<T>& v) {
    char c = r.peek();
    if (c == 'n') {
        if (r.sv.substr(r.pos, 4) != "null") throw SpDataError("JSON stream: expected 'null'");
        r.pos += 4;
        v.reset();
    } else if constexpr (std::is_abstract_v<T>) {
        r.expect('{');
        std::string key = r.readString(); // "_type"
        r.expect(':');
        std::string typeName = r.readString();
        r.expect(',');
        key = r.readString(); // "_data"
        r.expect(':');
        auto it = getJsonTypeRegistry().find(typeName);
        if (it != getJsonTypeRegistry().end()) {
            v.reset(static_cast<T*>(it->second()));
            spFromJsonStream(r, *v);
        } else {
            throw SpDataError("JSON stream: unknown type '" + typeName + "'");
        }
        r.expect('}');
    } else {
        if (v == nullptr) v = std::make_unique<T>();
        spFromJsonStream(r, *v);
    }
}

// Wptr
template<typename T>
inline void spFromJsonStream(JsonStreamReader& r, Wptr<T>& v) {
    char c = r.peek();
    if (c == 'n') {
        if (r.sv.substr(r.pos, 4) != "null") throw SpDataError("JSON stream: expected 'null'");
        r.pos += 4;
        v.reset();
    }
    // 否则保持原样（weak_ptr 无法还原）
}

// Base 子类
inline void spFromJsonStream(JsonStreamReader& r, Base& v) {
    v.fromJsonStream(r);
}

} // namespace sp