// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT
#pragma once
// ===================================================================
// StreamPunkOrmMeta.hpp — ORM 元数据基础设施
//
// 依赖：StreamPunk.hpp（X_classMember 已改为 (type__, name__, default__, ...)）
// ===================================================================
#include "StreamPunk.hpp"
#include <string_view>
#include <vector>
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <utility>

// ============================== ORM 注解位掩码常量 ==============================

#define ORM_PRIMARY_KEY        (1 << 0)   // 主键
#define ORM_AUTO_INCREMENT     (1 << 1)   // 自增（需配合 PRIMARY_KEY）
#define ORM_INDEX              (1 << 2)   // 创建普通索引
#define ORM_UNIQUE             (1 << 3)   // 创建唯一索引
#define ORM_NOT_NULL           (1 << 4)   // NOT NULL 约束
#define ORM_TRANSIENT          (1 << 5)   // 不持久化
#define ORM_ON_DELETE_CASCADE  (1 << 6)   // 级联删除（指针字段）
#define ORM_ON_DELETE_SET_NULL (1 << 7)   // 置空删除（指针字段）

// 类级标志
#define ORM_CLASS_ABSTRACT     (1 << 0)   // 抽象类，不建表

// ============================== SqlParam & SqlStatement ==============================

namespace sp {

using SqlParam = std::variant<std::nullptr_t, i64, u64, f64, bool, std::string>;

struct SqlStatement {
    std::string sql;
    std::vector<SqlParam> params;
};

// ============================== C++ → SQL 类型映射 ==============================

template<typename T> struct SqlTypeMap { static constexpr auto value = "VARCHAR(255)"; };

template<> struct SqlTypeMap<i8>      { static constexpr auto value = "TINYINT"; };
template<> struct SqlTypeMap<u8>      { static constexpr auto value = "TINYINT UNSIGNED"; };
template<> struct SqlTypeMap<i16>     { static constexpr auto value = "SMALLINT"; };
template<> struct SqlTypeMap<u16>     { static constexpr auto value = "SMALLINT UNSIGNED"; };
template<> struct SqlTypeMap<i32>     { static constexpr auto value = "INT"; };
template<> struct SqlTypeMap<u32>     { static constexpr auto value = "INT UNSIGNED"; };
template<> struct SqlTypeMap<i64>     { static constexpr auto value = "BIGINT"; };
template<> struct SqlTypeMap<u64>     { static constexpr auto value = "BIGINT UNSIGNED"; };
template<> struct SqlTypeMap<f32>     { static constexpr auto value = "FLOAT"; };
template<> struct SqlTypeMap<f64>     { static constexpr auto value = "DOUBLE"; };
template<> struct SqlTypeMap<bool>    { static constexpr auto value = "TINYINT(1)"; };
template<> struct SqlTypeMap<ch>      { static constexpr auto value = "TINYINT"; };
template<> struct SqlTypeMap<ch8>     { static constexpr auto value = "TINYINT"; };
template<> struct SqlTypeMap<ch16>    { static constexpr auto value = "INT"; };
template<> struct SqlTypeMap<ch32>    { static constexpr auto value = "INT"; };
template<> struct SqlTypeMap<std::string> { static constexpr auto value = "VARCHAR(255)"; };

// ============================== 值 → SqlParam 转换 ==============================

inline SqlParam toSqlParam(i8  v) { return static_cast<i64>(v); }
inline SqlParam toSqlParam(u8  v) { return static_cast<u64>(v); }
inline SqlParam toSqlParam(i16 v) { return static_cast<i64>(v); }
inline SqlParam toSqlParam(u16 v) { return static_cast<u64>(v); }
inline SqlParam toSqlParam(i32 v) { return static_cast<i64>(v); }
inline SqlParam toSqlParam(u32 v) { return static_cast<u64>(v); }
inline SqlParam toSqlParam(i64 v) { return static_cast<i64>(v); }
inline SqlParam toSqlParam(u64 v) { return static_cast<u64>(v); }
inline SqlParam toSqlParam(f32 v) { return static_cast<f64>(v); }
inline SqlParam toSqlParam(f64 v) { return static_cast<f64>(v); }
inline SqlParam toSqlParam(bool v) { return v; }
inline SqlParam toSqlParam(ch  v) { return static_cast<i64>(v); }
inline SqlParam toSqlParam(ch8 v) { return static_cast<i64>(v); }
inline SqlParam toSqlParam(ch16 v) { return static_cast<i64>(v); }
inline SqlParam toSqlParam(ch32 v) { return static_cast<i64>(v); }
inline SqlParam toSqlParam(std::string const& v) { return v; }
inline SqlParam toSqlParam(std::nullptr_t) { return nullptr; }

// std::optional 转换
template<typename T>
inline SqlParam toSqlParam(std::optional<T> const& v) {
    if (v.has_value()) return toSqlParam(v.value());
    return nullptr;
}

// enum 类型泛型转换
template<typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
inline SqlParam toSqlParam(T v) { return static_cast<i64>(static_cast<std::underlying_type_t<T>>(v)); }

} // namespace sp

// ============================== X_orm 宏：从 Xt_ 宏中提取 ORM 元数据 ==============================

#define X_ormFieldNameStr(type, name, default__, ...) #name ,

#define X_ormFieldFlags(type, name, default__, ...) (0 __VA_OPT__(|) __VA_ARGS__) ,

// ============================== UseDataOrm / UseOrmValue 宏 ==============================

#define UseDataOrmXtBase(TypeName, Xt, Base__, ...) \
    static constexpr inline int _ormClassFlags = (0 __VA_OPT__(|) __VA_ARGS__); \
    static constexpr inline char const* _ormFieldNames[] = { Xt(X_ormFieldNameStr) }; \
    static constexpr inline int       _ormFieldFlags[] = { Xt(X_ormFieldFlags) }; \
    static constexpr inline size_t    _ormFieldCount = sizeof(_ormFieldNames) / sizeof(_ormFieldNames[0]); \
    \
    static sp::SqlStatement _ormBuildInsert(TypeName const& _obj) { \
        std::string _cols, _vals; \
        std::vector<sp::SqlParam> _params; \
        bool _first = true; \
        Xt(X_ormCollectInsert); \
        std::string _sql = "INSERT INTO " + std::string(sp::OrmTableName<TypeName>::value) + " (" + _cols + ") VALUES (" + _vals + ")"; \
        return {_sql, std::move(_params)}; \
    } \
    static sp::SqlStatement _ormBuildInsertCtx(TypeName const& _obj, sp::OrmPersistContext const& _ctx) { \
        std::string _cols, _vals; \
        std::vector<sp::SqlParam> _params; \
        bool _first = true; \
        Xt(X_ormCollectInsertCtx); \
        std::string _sql = "INSERT INTO " + std::string(sp::OrmTableName<TypeName>::value) + " (" + _cols + ") VALUES (" + _vals + ")"; \
        return {_sql, std::move(_params)}; \
    } \
    static sp::SqlStatement _ormBuildUpdate(TypeName const& _obj) { \
        std::string _sets, _pkWhere; \
        std::vector<sp::SqlParam> _params; \
        bool _first = true; \
        Xt(X_ormCollectUpdateSet); \
        Xt(X_ormCollectPKWhere); \
        std::string _sql = "UPDATE " + std::string(sp::OrmTableName<TypeName>::value) + " SET " + _sets + " WHERE " + _pkWhere; \
        return {_sql, std::move(_params)}; \
    } \
    static std::string _ormBuildSelectCols() { \
        std::string _cols; \
        bool _first = true; \
        Xt(X_ormCollectSelectCol); \
        return _cols; \
    } \
    static std::string _ormPKName() { \
        std::string _name; \
        Xt(X_ormCollectPKName); \
        return _name; \
    }

#define UseDataOrmXt(TypeName, Xt) UseDataOrmXtBase(TypeName, Xt, Base)

#define UseDataOrm(TypeName, ...)   UseDataOrmXtBase(TypeName, Xt_##TypeName, Base __VA_OPT__(,) __VA_ARGS__)
#define UseDataOrmBase(TypeName, Base__, ...) UseDataOrmXtBase(TypeName, Xt_##TypeName, Base__ __VA_OPT__(,) __VA_ARGS__)

#define UseOrmValue(TypeName) \
    static constexpr inline bool _isOrmValue = true;

#define X_ormTypeExtract(type, name, default__, ...) type ,

#define UseOrmValueXt(TypeName, Xt) \
    static constexpr inline bool _isOrmValue = true; \
    static constexpr inline char const* _ormFieldNames[] = { Xt(X_ormFieldNameStr) }; \
    static constexpr inline int       _ormFieldFlags[] = { Xt(X_ormFieldFlags) }; \
    static constexpr inline size_t    _ormFieldCount = sizeof(_ormFieldNames) / sizeof(_ormFieldNames[0]); \
    using _ormTypeList = std::tuple< Xt(X_ormTypeExtract) std::nullptr_t >; \
    \
    static void _ormCollectInsert(TypeName const& _obj, std::string const& _prefix, \
        std::string& _cols, std::string& _vals, std::vector<sp::SqlParam>& _params, bool& _first) { \
        Xt(X_ormCollectInsertValue); \
    } \
    static void _ormCollectUpdateSet(TypeName const& _obj, std::string const& _prefix, \
        std::string& _sets, std::vector<sp::SqlParam>& _params, bool& _first) { \
        Xt(X_ormCollectUpdateSetValue); \
    } \
    static std::string _ormBuildSelectCols(std::string const& _prefix) { \
        std::string _cols; \
        bool _first = true; \
        Xt(X_ormCollectSelectColValue); \
        return _cols; \
    } \
    static std::string _ormBuildCreateCols(std::string const& _prefix) { \
        std::string _cols; \
        bool _first = true; \
        Xt(X_ormCollectCreateColValue); \
        return _cols; \
    }

// ============================== OrmMetadata 编译期元数据收集 ==============================

namespace sp {

template<typename T>
struct OrmTableName {
    static constexpr std::string_view value = T::_className;
};

template<typename T, size_t I>
struct OrmFieldMeta {
    static constexpr const char* name     = T::_ormFieldNames[I];
    static constexpr int         flags    = T::_ormFieldFlags[I];
    static constexpr bool        isTransient = (flags & ORM_TRANSIENT) != 0;
    static constexpr bool        isPrimaryKey = (flags & ORM_PRIMARY_KEY) != 0;
    static constexpr bool        isAutoIncrement = (flags & ORM_AUTO_INCREMENT) != 0;
    static constexpr bool        isNotNull = (flags & ORM_NOT_NULL) != 0;
    static constexpr bool        isUnique  = (flags & ORM_UNIQUE) != 0;
    static constexpr bool        hasIndex  = (flags & ORM_INDEX) != 0;
};

template<typename T>
struct OrmMetadata {
    using Type = T;
    static constexpr std::string_view tableName = OrmTableName<T>::value;
    static constexpr int classFlags = T::_ormClassFlags;
    static constexpr bool isAbstract = (classFlags & ORM_CLASS_ABSTRACT) != 0;
    static constexpr size_t fieldCount = T::_ormFieldCount;
};

// ---- typeID → 表名 路由注册 ----

struct OrmTypeRegistry {
    static std::unordered_map<Sz, std::string>& tableMap() {
        auto* reg = _sp_registry_ptr();
        if (reg) return reg->ormTableMap;
        static std::unordered_map<Sz, std::string> map;
        return map;
    }
    static void registerType(Sz typeId, std::string_view tableName) {
        tableMap()[typeId] = std::string(tableName);
    }
    static std::string const* getTableName(Sz typeId) {
        auto& map = tableMap();
        auto it = map.find(typeId);
        return it != map.end() ? &it->second : nullptr;
    }
    static bool isRegistered(Sz typeId) {
        return tableMap().count(typeId) > 0;
    }
};

// ---- OrmPersistContext：持久化上下文（运行时，处理循环引用） ----

struct OrmPersistContext {
    // 对象地址 → (type_id, db_row_id)
    std::unordered_map<void*, std::pair<Sz, i64>> persisted;

    // 记录已持久化对象
    void record(Base* obj, i64 rowId) {
        if (obj) persisted[static_cast<void*>(obj)] = {obj->typeID(), rowId};
    }

    // 查找对象是否已持久化，返回 rowId（0 表示未找到）
    i64 find(Base* obj) const {
        if (!obj) return 0;
        auto it = persisted.find(static_cast<void*>(const_cast<Base*>(obj)));
        return it != persisted.end() ? it->second.second : 0;
    }

    bool contains(Base* obj) const {
        return obj && persisted.count(static_cast<void*>(const_cast<Base*>(obj))) > 0;
    }

    // 标记对象是否已被 persist() 访问过（去重用，独立于 rowId）
    bool tryVisit(Base* obj) {
        if (!obj) return false;
        return visited.insert(static_cast<void*>(obj)).second;
    }

    void clear() { persisted.clear(); visited.clear(); }

private:
    std::unordered_set<void*> visited;
};

// ---- OrmLoadContext：加载上下文（运行时，处理循环引用） ----

struct OrmPairHash {
    size_t operator()(std::pair<Sz, i64> const& p) const noexcept {
        return std::hash<Sz>{}(p.first) ^ (std::hash<i64>{}(p.second) << 1);
    }
};

struct OrmLoadContext {
    // (type_id, row_id) → 对象指针
    std::unordered_map<std::pair<Sz, i64>, Base*, OrmPairHash> loaded;

    void record(Sz typeId, i64 rowId, Base* obj) {
        if (obj) loaded[{typeId, rowId}] = obj;
    }

    Base* find(Sz typeId, i64 rowId) const {
        auto it = loaded.find({typeId, rowId});
        return it != loaded.end() ? it->second : nullptr;
    }

    bool contains(Sz typeId, i64 rowId) const {
        return loaded.count({typeId, rowId}) > 0;
    }

    void clear() { loaded.clear(); }
};

// ---- 列名覆盖（按索引） ----

template<typename T, size_t I>
struct OrmColumnName {
    static constexpr std::string_view value = OrmFieldMeta<T, I>::name;
};

// ---- 列名覆盖（按字段名字符串） ----

template<char...>
struct OrmColumnNameOverride {
    static constexpr std::string_view value = "";
};

// ---- 数据库默认值覆盖（按索引） ----

template<typename T, size_t I, typename = void>
struct OrmDefaultValue {
    static constexpr std::string_view value = "";
};

// ---- SQL 列类型覆盖（按索引） ----

template<typename T, size_t I, typename = void>
struct OrmColumnType {
    static constexpr std::string_view value = "";
};

// ---- 复合索引默认实现 ----

template<typename T, typename = void>
struct OrmCompositeIndexesDefault {
    static std::vector<std::string> get() { return {}; }
};

template<typename T, typename = void>
struct OrmCompositeUniquesDefault {
    static std::vector<std::string> get() { return {}; }
};

// 用户特化后使用的别名
template<typename T>
using OrmCompositeIndexes = OrmCompositeIndexesDefault<T>;

template<typename T>
using OrmCompositeUniques = OrmCompositeUniquesDefault<T>;

} // namespace sp

// ============================== 独立注解宏 ==============================

#define ORM_TABLE(TypeName, table_name_str) \
    template<> struct sp::OrmTableName<TypeName> { \
        static constexpr std::string_view value = table_name_str; \
    };

// 按索引位置覆盖列名
#define ORM_COLUMN_BY_IDX(TypeName, idx, col_name_str) \
    template<> struct sp::OrmColumnName<TypeName, idx> { \
        static constexpr std::string_view value = col_name_str; \
    };

// 按字段名覆盖列名（使用 ORM_COLUMN_NAME 宏）
#define ORM_COLUMN_NAME(field, col_name_str) \
    template<> struct sp::OrmColumnNameOverride<#field> { \
        static constexpr std::string_view value = col_name_str; \
    };

// 按索引覆盖数据库默认值
#define ORM_DEFAULT_BY_IDX(TypeName, idx, sql_default) \
    template<> struct sp::OrmDefaultValue<TypeName, idx> { \
        static constexpr std::string_view value = sql_default; \
    };

// 按索引覆盖 SQL 列类型
#define ORM_TYPE_BY_IDX(TypeName, idx, sql_type_str) \
    template<> struct sp::OrmColumnType<TypeName, idx> { \
        static constexpr std::string_view value = sql_type_str; \
    };

// 复合索引
#define ORM_COMPOSITE_INDEX(TypeName, idx_name, ...) \
    template<> struct sp::OrmCompositeIndexesDefault<TypeName> { \
        static std::vector<std::string> get() { \
            std::vector<std::string> v; \
            v.push_back("CREATE INDEX " #idx_name " ON " + std::string(sp::OrmTableName<TypeName>::value) + " (" + std::string(#__VA_ARGS__) + ")"); \
            return v; \
        } \
    };

// 复合唯一索引
#define ORM_COMPOSITE_UNIQUE(TypeName, idx_name, ...) \
    template<> struct sp::OrmCompositeUniquesDefault<TypeName> { \
        static std::vector<std::string> get() { \
            std::vector<std::string> v; \
            v.push_back("CREATE UNIQUE INDEX " #idx_name " ON " + std::string(sp::OrmTableName<TypeName>::value) + " (" + std::string(#__VA_ARGS__) + ")"); \
            return v; \
        } \
    };

// 手动注册 typeID → 表名 映射（在 INIT_StreamPunk() 之后调用）
#define ORM_REGISTER_TYPE(TypeName) \
    sp::OrmTypeRegistry::registerType(TypeName::typeID(), sp::OrmTableName<TypeName>::value);