// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT
#pragma once
// ===================================================================
// StreamPunkOrmGen.hpp — ORM SQL 生成层
//
// 依赖：StreamPunkOrmMeta.hpp
// 功能：基于 M::TypeList 编译期生成 CREATE TABLE / INSERT / UPDATE / DELETE / SELECT
//
// 使用方式：
//   #include "StreamPunkOrmGen.hpp"  // 需在定义 ORM 类之前 include
//   struct User : Base { UseData(User); UseDataOrm(User); };
//   auto stmt = sp::insert(user);
// ===================================================================
#include "StreamPunkOrmMeta.hpp"
#include <sstream>

namespace sp {

// ============================== ORM 专用类型特征检测 ==============================
// 注意：is_optional_v 等公共 trait 已合并到 StreamPunk.hpp 中，此处不再重复定义

// 指针类型（T*, Sptr<T>, Uptr<T>, Wptr<T>）
template<typename T> struct is_orm_ptr_impl : std::false_type {};
template<typename T> struct is_orm_ptr_impl<T*> : std::true_type {};
template<typename T> struct is_orm_ptr_impl<Sptr<T>> : std::true_type {};
template<typename T> struct is_orm_ptr_impl<Uptr<T>> : std::true_type {};
template<typename T> struct is_orm_ptr_impl<Wptr<T>> : std::true_type {};
template<typename T> inline constexpr bool is_orm_ptr_v = is_orm_ptr_impl<std::decay_t<T>>::value;

// std::vector, std::list, std::deque 等容器（暂不处理，Phase 3）
template<typename T> struct is_orm_container_impl : std::false_type {};
template<typename T, typename A> struct is_orm_container_impl<std::vector<T, A>> : std::true_type {};
template<typename T, typename A> struct is_orm_container_impl<std::list<T, A>> : std::true_type {};
template<typename T, typename A> struct is_orm_container_impl<std::deque<T, A>> : std::true_type {};
template<typename T, size_t N> struct is_orm_container_impl<std::array<T, N>> : std::true_type {};
template<typename T, typename C> struct is_orm_container_impl<std::set<T, C>> : std::true_type {};
template<typename T, typename C> struct is_orm_container_impl<std::unordered_set<T, C>> : std::true_type {};
template<typename K, typename V, typename C> struct is_orm_container_impl<std::map<K, V, C>> : std::true_type {};
template<typename K, typename V, typename C> struct is_orm_container_impl<std::unordered_map<K, V, C>> : std::true_type {};
template<typename T> inline constexpr bool is_orm_container_v = is_orm_container_impl<std::decay_t<T>>::value;

// std::variant
template<typename T> struct is_variant_impl : std::false_type {};
template<typename... Ts> struct is_variant_impl<std::variant<Ts...>> : std::true_type {};
template<typename T> inline constexpr bool is_variant_v = is_variant_impl<std::decay_t<T>>::value;

// std::tuple
template<typename T> struct is_tuple_impl : std::false_type {};
template<typename... Ts> struct is_tuple_impl<std::tuple<Ts...>> : std::true_type {};
template<typename T> inline constexpr bool is_tuple_v = is_tuple_impl<std::decay_t<T>>::value;

// 是否有 _isOrmValue（复合值类型）
template<typename T, typename = void> struct has_orm_value_impl : std::false_type {};
template<typename T> struct has_orm_value_impl<T, std::void_t<decltype(T::_isOrmValue)>> : std::true_type {};
template<typename T> inline constexpr bool has_orm_value_v = has_orm_value_impl<std::decay_t<T>>::value;

// enum 检测
template<typename T> inline constexpr bool is_enum_orm_v = std::is_enum_v<std::decay_t<T>>;

// ============================== 指针原始值提取 ==============================

template<typename T>
inline Base* ormGetRawPtr(T* p) { return static_cast<Base*>(p); }
template<typename T>
inline Base* ormGetRawPtr(Sptr<T> const& p) { return static_cast<Base*>(p.get()); }
template<typename T>
inline Base* ormGetRawPtr(Uptr<T> const& p) { return static_cast<Base*>(p.get()); }
template<typename T>
inline Base* ormGetRawPtr(Wptr<T> const& p) { return static_cast<Base*>(p.lock().get()); }

// 指针/智能指针 → 所指类型提取
template<typename T> struct orm_pointee_type { using type = T; };
template<typename T> struct orm_pointee_type<T*> { using type = T; };
template<typename T> struct orm_pointee_type<Sptr<T>> { using type = T; };
template<typename T> struct orm_pointee_type<Uptr<T>> { using type = T; };
template<typename T> struct orm_pointee_type<Wptr<T>> { using type = T; };

// 通过 Base* 获取 ORM id（利用编译期已知的 Pointee 类型安全转换）
template<typename PtrType>
inline i64 ormGetTargetId(Base* target) {
    if (!target) return 0;
    using Pointee = typename orm_pointee_type<std::decay_t<PtrType>>::type;
    return static_cast<Pointee*>(target)->id;
}

// ============================== 列定义构建（CREATE TABLE 用） ==============================

inline std::string buildColumnCore(std::string const& name, std::string const& sqlType, bool nullable, int flags, std::string const& defaultVal = "") {
    std::string def = name + " " + sqlType;
    if (flags & ORM_NOT_NULL) {
        def += " NOT NULL";
    } else if (!nullable) {
        def += " NOT NULL";
    }
    if (!defaultVal.empty()) {
        def += " DEFAULT " + defaultVal;
    }
    if (flags & ORM_AUTO_INCREMENT) def += " AUTO_INCREMENT";
    return def;
}

// 根据类型返回合理的 SQL DEFAULT 值
template<typename T>
inline std::string getSqlDefaultForType() {
    if constexpr (is_optional_v<T>) {
        return "NULL";
    }
    else if constexpr (is_orm_ptr_v<T>) {
        return ""; // 指针双列分别处理
    }
    else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return "0";
    }
    else if constexpr (std::is_integral_v<std::decay_t<T>>) {
        return "0";
    }
    else if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
        return "0";
    }
    else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        return "''";
    }
    else if constexpr (is_enum_orm_v<T>) {
        return "0";
    }
    else {
        return "";
    }
}

// 获取字段的 DEFAULT 值（优先使用 OrmDefaultValue 覆盖，否则用类型默认值）
template<typename T, size_t I>
inline std::string getDefaultForField() {
    if constexpr (OrmDefaultValue<T, I>::value.size() > 0) {
        return std::string(OrmDefaultValue<T, I>::value);
    } else {
        using FieldType = std::tuple_element_t<I, typename T::M::TypeList>;
        return getSqlDefaultForType<FieldType>();
    }
}

// 单列定义构建（用于复合值类型的递归展开和 tuple/variant 展开）
template<typename T>
inline std::string buildSingleColumnDef(const char* name, int flags) {
    if (flags & ORM_TRANSIENT) return "";

    if constexpr (is_optional_v<T>) {
        return buildColumnCore(name, SqlTypeMap<typename T::value_type>::value, true, flags, "NULL");
    }
    else if constexpr (is_orm_ptr_v<T>) {
        return buildColumnCore(std::string(name) + "_type_id", "INT", true, flags, "NULL") + ",\n    " +
               buildColumnCore(std::string(name) + "_id", "BIGINT", true, flags, "NULL");
    }
    else if constexpr (has_orm_value_v<T>) {
        return T::_ormBuildCreateCols(name);
    }
    else if constexpr (is_orm_container_v<T>) {
        return ""; // 容器 → 子表（Phase 3）
    }
    else if constexpr (is_variant_v<T>) {
        return buildVariantColumns<T>(name, flags);
    }
    else if constexpr (is_tuple_v<T>) {
        return buildTupleColumns<T>(name, flags);
    }
    else if constexpr (is_enum_orm_v<T>) {
        return buildColumnCore(name, "INT", false, flags, "0");
    }
    else {
        return buildColumnCore(name, SqlTypeMap<T>::value, false, flags, getSqlDefaultForType<T>());
    }
}

// ---- variant 列展开 ----

template<typename Variant, size_t I>
struct VariantAltColumnDef {
    static std::string build(const char* prefix) {
        using AltType = std::variant_alternative_t<I, Variant>;
        std::string colName = std::string(prefix) + "_" + std::to_string(I);

        if constexpr (std::is_base_of_v<Base, AltType>) {
            return buildColumnCore(colName + "_type_id", "INT", true, 0, "NULL") + ",\n    " +
                   buildColumnCore(colName + "_id", "BIGINT", true, 0, "NULL");
        } else if constexpr (has_orm_value_v<AltType>) {
            return AltType::_ormBuildCreateCols(colName);
        } else if constexpr (is_optional_v<AltType>) {
            return buildColumnCore(colName, SqlTypeMap<typename AltType::value_type>::value, true, 0, "NULL");
        } else {
            return buildColumnCore(colName, SqlTypeMap<AltType>::value, true, 0, getSqlDefaultForType<AltType>());
        }
    }
};

template<typename Variant, size_t... Is>
inline std::string buildVariantColumnsImpl(const char* name, int, std::index_sequence<Is...>) {
    std::string result;
    result += buildColumnCore(std::string(name) + "_type_idx", "INT", false, 0, "0");
    bool first = false;
    auto append = [&](std::string const& col) {
        if (col.empty()) return;
        result += ",\n    ";
        result += col;
    };
    (append(VariantAltColumnDef<Variant, Is>::build(name)), ...);
    return result;
}

template<typename Variant>
inline std::string buildVariantColumns(const char* name, int flags) {
    return buildVariantColumnsImpl<Variant>(name, flags, std::make_index_sequence<std::variant_size_v<Variant>>{});
}

// ---- tuple 列展开 ----

template<typename Tuple, size_t I>
struct TupleElementColumnDef {
    static std::string build(const char* prefix) {
        using ElemType = std::tuple_element_t<I, Tuple>;
        std::string colName = std::string(prefix) + "_" + std::to_string(I);
        return buildSingleColumnDef<ElemType>(colName.c_str(), 0);
    }
};

template<typename Tuple, size_t... Is>
inline std::string buildTupleColumnsImpl(const char* name, std::index_sequence<Is...>) {
    std::string result;
    bool first = true;
    auto append = [&](std::string const& col) {
        if (col.empty()) return;
        if (!first) result += ",\n    ";
        result += col;
        first = false;
    };
    (append(TupleElementColumnDef<Tuple, Is>::build(name)), ...);
    return result;
}

template<typename Tuple>
inline std::string buildTupleColumns(const char* name, int) {
    return buildTupleColumnsImpl<Tuple>(name, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// ---- ColumnDef（主入口） ----

template<typename T, size_t I>
struct ColumnDef {
    static std::string build() {
        using FieldMeta = OrmFieldMeta<T, I>;
        if constexpr (FieldMeta::isTransient) return "";

        using FieldType = std::tuple_element_t<I, typename T::M::TypeList>;
        constexpr const char* name = OrmColumnName<T, I>::value.data();
        constexpr int flags = FieldMeta::flags;

        return buildSingleColumnDefWithDefault<FieldType, T, I>(name, flags);
    }
};

// 带 DEFAULT 子句的单列定义（用于 ColumnDef，可获取字段级的默认值覆盖）
template<typename FieldType, typename T, size_t I>
inline std::string buildSingleColumnDefWithDefault(const char* name, int flags) {
    if (flags & ORM_TRANSIENT) return "";

    // AUTO_INCREMENT 列不应有 DEFAULT
    const bool isAutoInc = (flags & ORM_AUTO_INCREMENT) != 0;
    std::string defaultVal = isAutoInc ? "" : getDefaultForField<T, I>();

    if constexpr (is_optional_v<FieldType>) {
        return buildColumnCore(name, SqlTypeMap<typename FieldType::value_type>::value, true, flags, defaultVal);
    }
    else if constexpr (is_orm_ptr_v<FieldType>) {
        return buildColumnCore(std::string(name) + "_type_id", "INT", true, flags, isAutoInc ? "" : "NULL") + ",\n    " +
               buildColumnCore(std::string(name) + "_id", "BIGINT", true, flags, isAutoInc ? "" : "NULL");
    }
    else if constexpr (has_orm_value_v<FieldType>) {
        return FieldType::_ormBuildCreateCols(name);
    }
    else if constexpr (is_orm_container_v<FieldType>) {
        return "";
    }
    else if constexpr (is_variant_v<FieldType>) {
        return buildVariantColumns<FieldType>(name, flags);
    }
    else if constexpr (is_tuple_v<FieldType>) {
        return buildTupleColumns<FieldType>(name, flags);
    }
    else if constexpr (is_enum_orm_v<FieldType>) {
        return buildColumnCore(name, "INT", false, flags, defaultVal);
    }
    else {
        return buildColumnCore(name, SqlTypeMap<FieldType>::value, false, flags, defaultVal);
    }
}

// ============================== 字段收集辅助函数（INSERT / UPDATE / SELECT 用） ==============================

// ---- 前向声明 ----

template<typename Tuple, size_t... Is>
inline void collectTupleInsertFields(Tuple const& val, const char* prefix, std::index_sequence<Is...>, std::string& cols, std::string& vals, std::vector<SqlParam>& params, bool& first);

template<typename Tuple, size_t... Is>
inline void collectTupleUpdateFields(Tuple const& val, const char* prefix, std::index_sequence<Is...>, std::string& sets, std::vector<SqlParam>& params, bool& first);

template<typename Tuple>
inline void collectTupleSelectCols(const char* prefix, std::string& cols, bool& first);

template<typename Variant>
inline void collectVariantSelectCols(const char* prefix, std::string& cols, bool& first);

// 收集复合值类型子字段的列名（SELECT 用）
template<typename T>
inline void collectOrmValueSelectCols(std::string const& prefix, std::string& cols, bool& first) {
    cols += T::_ormBuildSelectCols(prefix);
    // 注意：_ormBuildSelectCols 会重建自带分隔符的列名，因此需要调整
    // 实际上，_ormBuildSelectCols 返回的是纯净列名。我们需要处理首列标志。
}

template<typename T>
inline void collectInsertField(T const& val, const char* colName, int flags, std::string& cols, std::string& vals, std::vector<SqlParam>& params, bool& first) {
    if (flags & (ORM_TRANSIENT | ORM_AUTO_INCREMENT)) return;

    if constexpr (is_optional_v<T>) {
        if (!first) { cols += ", "; vals += ", "; }
        cols += colName;
        if (val.has_value()) {
            vals += ":p" + std::to_string(params.size());
            params.push_back(toSqlParam(val.value()));
        } else {
            vals += "NULL";
        }
        first = false;
    }
    else if constexpr (is_orm_ptr_v<T>) {
        // 指针 → (type_id, id) 双列
        Base* target = ormGetRawPtr(val);
        if (!first) { cols += ", "; vals += ", "; }
        cols += std::string(colName) + "_type_id, " + std::string(colName) + "_id";
        if (target) {
            vals += ":p" + std::to_string(params.size()) + ", :p" + std::to_string(params.size() + 1);
            params.push_back(toSqlParam(static_cast<i64>(target->typeID())));
            params.push_back(toSqlParam(ormGetTargetId<T>(target)));
        } else {
            vals += "NULL, NULL";
        }
        first = false;
    }
    else if constexpr (has_orm_value_v<T>) {
        // 复合值类型 → 递归展开
        T::_ormCollectInsert(val, colName, cols, vals, params, first);
    }
    else if constexpr (is_orm_container_v<T>) {
        return; // 容器 → 子表（Phase 3）
    }
    else if constexpr (is_variant_v<T>) {
        // variant → type_idx + 活跃分支
        if (!first) { cols += ", "; vals += ", "; }
        cols += std::string(colName) + "_type_idx";
        vals += ":p" + std::to_string(params.size());
        params.push_back(toSqlParam(static_cast<i64>(val.index())));
        first = false;

        std::visit([&](auto&& alt) {
            using AltType = std::decay_t<decltype(alt)>;
            std::string altColName = std::string(colName) + "_" + std::to_string(val.index());

            if constexpr (std::is_base_of_v<Base, AltType>) {
                if (!first) { cols += ", "; vals += ", "; }
                cols += altColName + "_type_id";
                vals += ":p" + std::to_string(params.size());
                params.push_back(toSqlParam(static_cast<i64>(alt.typeID())));
                first = false;

                if (!first) { cols += ", "; vals += ", "; }
                cols += altColName + "_id";
                vals += ":p" + std::to_string(params.size());
                params.push_back(toSqlParam(static_cast<i64>(alt.id)));
                first = false;
            } else if constexpr (has_orm_value_v<AltType>) {
                AltType::_ormCollectInsert(alt, altColName, cols, vals, params, first);
            } else {
                if (!first) { cols += ", "; vals += ", "; }
                cols += altColName;
                vals += ":p" + std::to_string(params.size());
                params.push_back(toSqlParam(alt));
                first = false;
            }
        }, val);
    }
    else if constexpr (is_tuple_v<T>) {
        // tuple → 递归展开每个元素
        collectTupleInsertFields(val, colName, std::make_index_sequence<std::tuple_size_v<T>>{}, cols, vals, params, first);
    }
    else if constexpr (is_enum_orm_v<T>) {
        if (!first) { cols += ", "; vals += ", "; }
        cols += colName;
        vals += ":p" + std::to_string(params.size());
        params.push_back(toSqlParam(static_cast<i64>(static_cast<std::underlying_type_t<T>>(val))));
        first = false;
    }
    else {
        if (!first) { cols += ", "; vals += ", "; }
        cols += colName;
        vals += ":p" + std::to_string(params.size());
        params.push_back(toSqlParam(val));
        first = false;
    }
}

// 上下文感知版本的 collectInsertField：指针字段从 ctx 中查找已持久化对象的 ID
template<typename T>
inline void collectInsertFieldCtx(T const& val, const char* colName, int flags, std::string& cols, std::string& vals, std::vector<SqlParam>& params, bool& first, OrmPersistContext const& ctx) {
    if (flags & (ORM_TRANSIENT | ORM_AUTO_INCREMENT)) return;

    if constexpr (is_optional_v<T>) {
        if (!first) { cols += ", "; vals += ", "; }
        cols += colName;
        if (val.has_value()) {
            vals += ":p" + std::to_string(params.size());
            params.push_back(toSqlParam(val.value()));
        } else {
            vals += "NULL";
        }
        first = false;
    }
    else if constexpr (is_orm_ptr_v<T>) {
        Base* target = ormGetRawPtr(val);
        if (!first) { cols += ", "; vals += ", "; }
        cols += std::string(colName) + "_type_id, " + std::string(colName) + "_id";
        if (target) {
            i64 persistedId = ctx.find(const_cast<Base*>(target));
            vals += ":p" + std::to_string(params.size()) + ", :p" + std::to_string(params.size() + 1);
            params.push_back(toSqlParam(static_cast<i64>(target->typeID())));
            params.push_back(toSqlParam(persistedId));
        } else {
            vals += "NULL, NULL";
        }
        first = false;
    }
    else if constexpr (has_orm_value_v<T>) {
        T::_ormCollectInsert(val, colName, cols, vals, params, first);
    }
    else if constexpr (is_orm_container_v<T>) {
        return;
    }
    else if constexpr (is_variant_v<T>) {
        if (!first) { cols += ", "; vals += ", "; }
        cols += std::string(colName) + "_type_idx";
        vals += ":p" + std::to_string(params.size());
        params.push_back(toSqlParam(static_cast<i64>(val.index())));
        first = false;

        std::visit([&](auto&& alt) {
            using AltType = std::decay_t<decltype(alt)>;
            std::string altColName = std::string(colName) + "_" + std::to_string(val.index());

            if constexpr (std::is_base_of_v<Base, AltType>) {
                if (!first) { cols += ", "; vals += ", "; }
                cols += altColName + "_type_id";
                vals += ":p" + std::to_string(params.size());
                params.push_back(toSqlParam(static_cast<i64>(alt.typeID())));
                first = false;

                if (!first) { cols += ", "; vals += ", "; }
                cols += altColName + "_id";
                i64 persistedId = ctx.find(const_cast<AltType*>(&alt));
                vals += ":p" + std::to_string(params.size());
                params.push_back(toSqlParam(persistedId));
                first = false;
            } else if constexpr (has_orm_value_v<AltType>) {
                AltType::_ormCollectInsert(alt, altColName, cols, vals, params, first);
            } else {
                if (!first) { cols += ", "; vals += ", "; }
                cols += altColName;
                vals += ":p" + std::to_string(params.size());
                params.push_back(toSqlParam(alt));
                first = false;
            }
        }, val);
    }
    else if constexpr (is_tuple_v<T>) {
        collectTupleInsertFields(val, colName, std::make_index_sequence<std::tuple_size_v<T>>{}, cols, vals, params, first);
    }
    else if constexpr (is_enum_orm_v<T>) {
        if (!first) { cols += ", "; vals += ", "; }
        cols += colName;
        vals += ":p" + std::to_string(params.size());
        params.push_back(toSqlParam(static_cast<i64>(static_cast<std::underlying_type_t<T>>(val))));
        first = false;
    }
    else {
        if (!first) { cols += ", "; vals += ", "; }
        cols += colName;
        vals += ":p" + std::to_string(params.size());
        params.push_back(toSqlParam(val));
        first = false;
    }
}

template<typename T>
inline void collectUpdateSetField(T const& val, const char* colName, int flags, std::string& sets, std::vector<SqlParam>& params, bool& first) {
    if (flags & (ORM_TRANSIENT | ORM_PRIMARY_KEY)) return;

    if constexpr (is_optional_v<T>) {
        if (!first) sets += ", ";
        sets += std::string(colName) + " = ";
        if (val.has_value()) {
            sets += ":p" + std::to_string(params.size());
            params.push_back(toSqlParam(val.value()));
        } else {
            sets += "NULL";
        }
        first = false;
    }
    else if constexpr (is_orm_ptr_v<T>) {
        // 指针 → (type_id, id) 双列
        Base* target = ormGetRawPtr(val);
        if (!first) sets += ", ";
        if (target) {
            sets += std::string(colName) + "_type_id = :p" + std::to_string(params.size()) + ", " + std::string(colName) + "_id = :p" + std::to_string(params.size() + 1);
            params.push_back(toSqlParam(static_cast<i64>(target->typeID())));
            params.push_back(toSqlParam(ormGetTargetId<T>(target)));
        } else {
            sets += std::string(colName) + "_type_id = NULL, " + std::string(colName) + "_id = NULL";
        }
        first = false;
    }
    else if constexpr (has_orm_value_v<T>) {
        T::_ormCollectUpdateSet(val, colName, sets, params, first);
    }
    else if constexpr (is_orm_container_v<T>) {
        return;
    }
    else if constexpr (is_variant_v<T>) {
        if (!first) sets += ", ";
        sets += std::string(colName) + "_type_idx = :p" + std::to_string(params.size());
        params.push_back(toSqlParam(static_cast<i64>(val.index())));
        first = false;

        std::visit([&](auto&& alt) {
            using AltType = std::decay_t<decltype(alt)>;
            std::string altColName = std::string(colName) + "_" + std::to_string(val.index());

            if constexpr (std::is_base_of_v<Base, AltType>) {
                if (!first) sets += ", ";
                sets += altColName + "_type_id = :p" + std::to_string(params.size());
                params.push_back(toSqlParam(static_cast<i64>(alt.typeID())));
                first = false;

                if (!first) sets += ", ";
                sets += altColName + "_id = :p" + std::to_string(params.size());
                params.push_back(toSqlParam(static_cast<i64>(alt.id)));
                first = false;
            } else if constexpr (has_orm_value_v<AltType>) {
                AltType::_ormCollectUpdateSet(alt, altColName, sets, params, first);
            } else {
                if (!first) sets += ", ";
                sets += altColName + " = :p" + std::to_string(params.size());
                params.push_back(toSqlParam(alt));
                first = false;
            }
        }, val);
    }
    else if constexpr (is_tuple_v<T>) {
        collectTupleUpdateFields(val, colName, std::make_index_sequence<std::tuple_size_v<T>>{}, sets, params, first);
    }
    else if constexpr (is_enum_orm_v<T>) {
        if (!first) sets += ", ";
        sets += std::string(colName) + " = :p" + std::to_string(params.size());
        params.push_back(toSqlParam(static_cast<i64>(static_cast<std::underlying_type_t<T>>(val))));
        first = false;
    }
    else {
        if (!first) sets += ", ";
        sets += std::string(colName) + " = :p" + std::to_string(params.size());
        params.push_back(toSqlParam(val));
        first = false;
    }
}

template<typename T, std::enable_if_t<!has_orm_value_v<T> && !is_orm_container_v<T> && !is_orm_ptr_v<T> && !is_variant_v<T> && !is_tuple_v<T>, int> = 0>
inline void collectPKWhere(T const& val, const char* colName, int flags, std::string& pkWhere, std::vector<SqlParam>& params) {
    if (flags & ORM_PRIMARY_KEY) {
        pkWhere = std::string(colName) + " = :p" + std::to_string(params.size());
        params.push_back(toSqlParam(val));
    }
}

template<typename T, std::enable_if_t<has_orm_value_v<T> || is_orm_container_v<T> || is_orm_ptr_v<T> || is_variant_v<T> || is_tuple_v<T>, int> = 0>
inline void collectPKWhere(T const&, const char*, int, std::string&, std::vector<SqlParam>&) {
    // 复合类型不可能是主键，跳过
}

template<typename T>
inline void collectSelectCol(const char* colName, int flags, std::string& cols, bool& first) {
    if (flags & ORM_TRANSIENT) return;

    if constexpr (has_orm_value_v<T>) {
        auto subCols = T::_ormBuildSelectCols(colName);
        if (!subCols.empty()) {
            if (!first) cols += ", ";
            cols += subCols;
            first = false;
        }
    }
    else if constexpr (is_orm_ptr_v<T>) {
        if (!first) cols += ", ";
        cols += std::string(colName) + "_type_id, " + std::string(colName) + "_id";
        first = false;
    }
    else if constexpr (is_variant_v<T>) {
        if (!first) cols += ", ";
        cols += std::string(colName) + "_type_idx";
        first = false;
        collectVariantSelectCols<T>(colName, cols, first);
    }
    else if constexpr (is_tuple_v<T>) {
        collectTupleSelectCols<T>(colName, cols, first);
    }
    else if constexpr (is_orm_container_v<T>) {
        return;
    }
    else {
        if (!first) cols += ", ";
        cols += colName;
        first = false;
    }
}

inline void collectPKName(const char* colName, int flags, std::string& name) {
    if (flags & ORM_PRIMARY_KEY) {
        if (!name.empty()) name += ", ";
        name += colName;
    }
}

// ---- tuple 展开辅助 ----

template<typename Tuple, size_t I>
inline void collectTupleOneInsert(Tuple const& val, const char* prefix, std::string& cols, std::string& vals, std::vector<SqlParam>& params, bool& first) {
    std::string colName = std::string(prefix) + "_" + std::to_string(I);
    collectInsertField<std::tuple_element_t<I, Tuple>>(std::get<I>(val), colName.c_str(), 0, cols, vals, params, first);
}

template<typename Tuple, size_t... Is>
inline void collectTupleInsertFields(Tuple const& val, const char* prefix, std::index_sequence<Is...>, std::string& cols, std::string& vals, std::vector<SqlParam>& params, bool& first) {
    (collectTupleOneInsert<Tuple, Is>(val, prefix, cols, vals, params, first), ...);
}

template<typename Tuple, size_t I>
inline void collectTupleOneUpdate(Tuple const& val, const char* prefix, std::string& sets, std::vector<SqlParam>& params, bool& first) {
    std::string colName = std::string(prefix) + "_" + std::to_string(I);
    collectUpdateSetField<std::tuple_element_t<I, Tuple>>(std::get<I>(val), colName.c_str(), 0, sets, params, first);
}

template<typename Tuple, size_t... Is>
inline void collectTupleUpdateFields(Tuple const& val, const char* prefix, std::index_sequence<Is...>, std::string& sets, std::vector<SqlParam>& params, bool& first) {
    (collectTupleOneUpdate<Tuple, Is>(val, prefix, sets, params, first), ...);
}

// ---- tuple SELECT 列名收集 ----

template<typename Tuple, size_t I>
inline void collectTupleOneSelectCol(const char* prefix, std::string& cols, bool& first) {
    using ElemType = std::tuple_element_t<I, Tuple>;
    std::string colName = std::string(prefix) + "_" + std::to_string(I);
    collectSelectCol<ElemType>(colName.c_str(), 0, cols, first);
}

template<typename Tuple, size_t... Is>
inline void collectTupleSelectCols(const char* prefix, std::string& cols, bool& first, std::index_sequence<Is...>) {
    (collectTupleOneSelectCol<Tuple, Is>(prefix, cols, first), ...);
}

template<typename Tuple>
inline void collectTupleSelectCols(const char* prefix, std::string& cols, bool& first) {
    collectTupleSelectCols<Tuple>(prefix, cols, first, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// ---- variant SELECT 列名收集 ----

template<typename Variant, size_t I>
inline void collectVariantOneSelectCol(const char* prefix, std::string& cols, bool& first) {
    using AltType = std::variant_alternative_t<I, Variant>;
    std::string colName = std::string(prefix) + "_" + std::to_string(I);
    collectSelectCol<AltType>(colName.c_str(), 0, cols, first);
}

template<typename Variant, size_t... Is>
inline void collectVariantSelectColsImpl(const char* prefix, std::string& cols, bool& first, std::index_sequence<Is...>) {
    (collectVariantOneSelectCol<Variant, Is>(prefix, cols, first), ...);
}

template<typename Variant>
inline void collectVariantSelectCols(const char* prefix, std::string& cols, bool& first) {
    collectVariantSelectColsImpl<Variant>(prefix, cols, first, std::make_index_sequence<std::variant_size_v<Variant>>{});
}

// ---- 复合值类型子字段列名收集（SELECT 用） ----

template<typename T>
inline void collectSelectColOrmValue(std::string const& prefix, std::string& cols, bool& first) {
    auto subCols = T::_ormBuildSelectCols(prefix);
    if (!subCols.empty()) {
        if (!first) cols += ", ";
        cols += subCols;
        first = false;
    }
}

} // namespace sp

// ============================== X_orm SQL 生成宏 ==============================
// 这些宏在 UseDataOrm 生成的成员函数体内展开，依赖 _obj / _cols / _vals / _params / _first / _sets / _pkWhere / _name

#define X_ormCollectInsert(type, name, default__, ...) \
    sp::collectInsertField<type>(_obj.name, #name, (0 __VA_OPT__(|) __VA_ARGS__), _cols, _vals, _params, _first);

#define X_ormCollectInsertCtx(type, name, default__, ...) \
    sp::collectInsertFieldCtx<type>(_obj.name, #name, (0 __VA_OPT__(|) __VA_ARGS__), _cols, _vals, _params, _first, _ctx);

#define X_ormCollectUpdateSet(type, name, default__, ...) \
    sp::collectUpdateSetField<type>(_obj.name, #name, (0 __VA_OPT__(|) __VA_ARGS__), _sets, _params, _first);

#define X_ormCollectPKWhere(type, name, default__, ...) \
    sp::collectPKWhere<type>(_obj.name, #name, (0 __VA_OPT__(|) __VA_ARGS__), _pkWhere, _params);

#define X_ormCollectSelectCol(type, name, default__, ...) \
    sp::collectSelectCol<type>(#name, (0 __VA_OPT__(|) __VA_ARGS__), _cols, _first);

#define X_ormCollectPKName(type, name, default__, ...) \
    sp::collectPKName(#name, (0 __VA_OPT__(|) __VA_ARGS__), _name);

// ============================== 复合值类型 X_orm 宏 ==============================
// 在 UseOrmValueXt 生成的成员函数体内展开，依赖 _obj / _prefix / _cols / _vals / _params / _first / _sets

#define X_ormCollectInsertValue(type, name, default__, ...) \
    sp::collectInsertField<type>(_obj.name, (_prefix + "_" + #name).c_str(), \
        (0 __VA_OPT__(|) __VA_ARGS__), _cols, _vals, _params, _first);

#define X_ormCollectUpdateSetValue(type, name, default__, ...) \
    sp::collectUpdateSetField<type>(_obj.name, (_prefix + "_" + #name).c_str(), \
        (0 __VA_OPT__(|) __VA_ARGS__), _sets, _params, _first);

#define X_ormCollectSelectColValue(type, name, default__, ...) \
    sp::collectSelectCol<type>((_prefix + "_" + #name).c_str(), \
        (0 __VA_OPT__(|) __VA_ARGS__), _cols, _first);

#define X_ormCollectCreateColValue(type, name, default__, ...) \
    { auto _cd = sp::buildSingleColumnDef<type>((_prefix + "_" + #name).c_str(), (0 __VA_OPT__(|) __VA_ARGS__)); \
      if (!_cd.empty()) { if (!_first) _cols += ",\n    "; _cols += _cd; _first = false; } }

// ============================== 公共 API ==============================

namespace sp {

// ---- CREATE TABLE ----

template<typename T, size_t... Is>
std::string buildCreateTableColumns(std::index_sequence<Is...>) {
    std::string result;
    bool first = true;
    auto append = [&](std::string const& col) {
        if (col.empty()) return;
        if (!first) result += ",\n    ";
        result += col;
        first = false;
    };
    (append(ColumnDef<T, Is>::build()), ...);
    return result;
}

template<typename T, size_t... Is>
std::string buildPrimaryKey(std::index_sequence<Is...>) {
    std::string pk;
    (collectPKName(OrmFieldMeta<T, Is>::name, OrmFieldMeta<T, Is>::flags, pk), ...);
    return pk;
}

template<typename T, size_t... Is>
std::vector<std::string> buildIndexes(std::string const& tableName, std::index_sequence<Is...>) {
    std::vector<std::string> indexes;
    auto collect = [&](const char* name, int flags) {
        if (flags & ORM_INDEX) {
            indexes.push_back("CREATE INDEX " + tableName + "_" + name + "_idx ON " + tableName + "(" + name + ")");
        }
        if (flags & ORM_UNIQUE) {
            indexes.push_back("CREATE UNIQUE INDEX " + tableName + "_" + name + "_uniq ON " + tableName + "(" + name + ")");
        }
    };
    (collect(OrmFieldMeta<T, Is>::name, OrmFieldMeta<T, Is>::flags), ...);
    return indexes;
}

template<typename T>
SqlStatement createTable() {
    using Meta = OrmMetadata<T>;
    if constexpr (Meta::isAbstract) return {"", {}};

    constexpr auto N = Meta::fieldCount;
    std::string tableName(Meta::tableName);

    std::string sql;
    sql += "CREATE TABLE " + tableName + " (\n    ";
    sql += buildCreateTableColumns<T>(std::make_index_sequence<N>{});

    std::string pk = buildPrimaryKey<T>(std::make_index_sequence<N>{});
    if (!pk.empty()) {
        sql += ",\n    PRIMARY KEY (" + pk + ")";
    }
    sql += "\n)";

    return {sql, {}};
}

template<typename T>
std::vector<SqlStatement> createTableFull() {
    std::vector<SqlStatement> stmts;
    stmts.push_back(createTable<T>());

    using Meta = OrmMetadata<T>;
    if constexpr (!Meta::isAbstract) {
        std::string tableName(Meta::tableName);
        auto indexes = buildIndexes<T>(tableName, std::make_index_sequence<Meta::fieldCount>{});
        for (auto& idx : indexes) {
            stmts.push_back({idx, {}});
        }
        // 复合索引
        auto compositeIdx = OrmCompositeIndexes<T>::get();
        for (auto& idx : compositeIdx) {
            stmts.push_back({idx, {}});
        }
        auto compositeUniq = OrmCompositeUniques<T>::get();
        for (auto& uniq : compositeUniq) {
            stmts.push_back({uniq, {}});
        }
    }
    return stmts;
}

template<typename T>
SqlStatement dropTable() {
    using Meta = OrmMetadata<T>;
    if constexpr (Meta::isAbstract) return {"", {}};
    return {"DROP TABLE IF EXISTS " + std::string(Meta::tableName), {}};
}

// ---- INSERT ----

// 基础 INSERT（不跟踪上下文）
template<typename T>
SqlStatement insert(T const& obj) {
    return T::_ormBuildInsert(obj);
}

// 带上下文的 INSERT：指针字段会查询 ctx 中已持久化对象的 ID
template<typename T>
SqlStatement insert(T const& obj, OrmPersistContext const& ctx) {
    return T::_ormBuildInsertCtx(obj, ctx);
}

// ---- persist：递归持久化对象图 ----
// 使用方式：
//   OrmPersistContext ctx;
//   auto stmts = sp::persist(rootObj, ctx);   // 生成所有 INSERT 语句
//   for (auto& stmt : stmts) {
//       i64 newId = db.execute(stmt);          // 执行 INSERT，获取自增 ID
//       // 调用方需要自行将 (obj, newId) 记录到 ctx 中
//   }
//
// 注意：persist() 只负责语句生成和去重，不执行 SQL。
// 如果对象图中存在循环引用，调用方需要两阶段处理：
//   1. 执行所有 INSERT（此时指针字段为 NULL）
//   2. 执行 UPDATE 回填指针字段

template<typename T>
std::vector<SqlStatement> persist(T const& obj, OrmPersistContext& ctx) {
    std::vector<SqlStatement> stmts;
    // 去重：如果该对象已被 persist 访问过，跳过
    if constexpr (std::is_base_of_v<Base, T>) {
        if (!ctx.tryVisit(const_cast<T*>(&obj))) {
            return stmts;
        }
    }
    stmts.push_back(T::_ormBuildInsertCtx(obj, ctx));
    return stmts;
}

// ---- UPDATE ----

template<typename T>
SqlStatement update(T const& obj) {
    return T::_ormBuildUpdate(obj);
}

// ---- DELETE ----

template<typename T>
SqlStatement deleteById(i64 id) {
    using Meta = OrmMetadata<T>;
    std::string pkName = T::_ormPKName();
    return {"DELETE FROM " + std::string(Meta::tableName) + " WHERE " + pkName + " = :p0", {id}};
}

// ---- SELECT ----

template<typename T>
SqlStatement findById(i64 id) {
    using Meta = OrmMetadata<T>;
    std::string cols = T::_ormBuildSelectCols();
    std::string pkName = T::_ormPKName();
    return {"SELECT " + cols + " FROM " + std::string(Meta::tableName) + " WHERE " + pkName + " = :p0", {id}};
}

template<typename T>
SqlStatement selectAll() {
    using Meta = OrmMetadata<T>;
    std::string cols = T::_ormBuildSelectCols();
    return {"SELECT " + cols + " FROM " + std::string(Meta::tableName), {}};
}

// ============================== 查询构建器（QueryBuilder） ==============================

// 将逗号分隔的列名列表中的每个列名加上表名前缀
inline std::string qualifyColumns(std::string const& tableName, std::string const& cols) {
    std::string result;
    std::string_view sv(cols);
    size_t pos = 0;
    bool first = true;
    while (pos < sv.size()) {
        while (pos < sv.size() && (sv[pos] == ' ' || sv[pos] == '\t')) ++pos;
        if (pos >= sv.size()) break;
        size_t end = pos;
        while (end < sv.size() && sv[end] != ',') ++end;
        size_t colEnd = end;
        while (colEnd > pos && (sv[colEnd - 1] == ' ' || sv[colEnd - 1] == '\t')) --colEnd;
        if (!first) result += ", ";
        result += tableName + "." + std::string(sv.substr(pos, colEnd - pos));
        first = false;
        pos = end + 1;
    }
    return result;
}

template<typename T>
class QueryBuilder {
    std::string _tableName;
    std::string _selectCols;
    std::vector<std::string> _whereClauses;
    std::vector<SqlParam> _whereParams;
    std::string _orderBy;
    int _limitVal = -1;
    int _offsetVal = -1;
    std::vector<std::string> _joins;
    bool _hasJoin = false;
    std::string _groupBy;
    std::string _having;

public:
    QueryBuilder() {
        using Meta = OrmMetadata<T>;
        _tableName = std::string(Meta::tableName);
        _selectCols = T::_ormBuildSelectCols();
    }

    // ---- 条件构建 ----

    // where(col, value) — 等值条件
    QueryBuilder& where(std::string const& col, SqlParam value) {
        _whereClauses.push_back(col + " = :p" + std::to_string(_whereParams.size()));
        _whereParams.push_back(std::move(value));
        return *this;
    }

    // where(col, op, value) — 比较条件（如 "age", ">", 18）
    QueryBuilder& where(std::string const& col, std::string const& op, SqlParam value) {
        _whereClauses.push_back(col + " " + op + " :p" + std::to_string(_whereParams.size()));
        _whereParams.push_back(std::move(value));
        return *this;
    }

    // whereRaw(sql) — 原始 SQL 片段（无参数）
    QueryBuilder& whereRaw(std::string const& sql) {
        _whereClauses.push_back(sql);
        return *this;
    }

    // whereRaw(sql, value) — 原始 SQL 片段（带参数）
    QueryBuilder& whereRaw(std::string const& sql, SqlParam value) {
        _whereClauses.push_back(sql);
        _whereParams.push_back(std::move(value));
        return *this;
    }

    // whereRaw(sql, v1, v2) — 原始 SQL 片段（2个参数）
    QueryBuilder& whereRaw(std::string const& sql, SqlParam v1, SqlParam v2) {
        _whereClauses.push_back(sql);
        _whereParams.push_back(std::move(v1));
        _whereParams.push_back(std::move(v2));
        return *this;
    }

    // ---- JOIN ----

    // join<JoinT>(localCol, remoteCol) — INNER JOIN
    // join<JoinT>(localCol, remoteCol, "LEFT JOIN") — 指定 JOIN 类型
    template<typename JoinT>
    QueryBuilder& join(std::string const& localCol, std::string const& remoteCol,
                       std::string const& joinType = "JOIN") {
        using JoinMeta = OrmMetadata<JoinT>;
        if constexpr (JoinMeta::isAbstract) return *this;

        std::string joinTable = std::string(JoinMeta::tableName);

        if (!_hasJoin) {
            // 首次 JOIN：为主表列名加上表名前缀
            _selectCols = qualifyColumns(_tableName, _selectCols);
            _hasJoin = true;
        }

        // 添加 JOIN 表的列名（带表名前缀）
        std::string joinCols = JoinT::_ormBuildSelectCols();
        _selectCols += ", " + qualifyColumns(joinTable, joinCols);

        _joins.push_back(joinType + " " + joinTable + " ON " +
                         _tableName + "." + localCol + " = " + joinTable + "." + remoteCol);
        return *this;
    }

    // ---- 排序 ----

    QueryBuilder& orderBy(std::string const& col, std::string const& dir = "ASC") {
        if (!_orderBy.empty()) _orderBy += ", ";
        _orderBy += col + " " + dir;
        return *this;
    }

    // ---- 分页 ----

    QueryBuilder& limit(int n) { _limitVal = n; return *this; }
    QueryBuilder& offset(int n) { _offsetVal = n; return *this; }

    // ---- 分组 ----

    QueryBuilder& groupBy(std::string const& cols) { _groupBy = cols; return *this; }
    QueryBuilder& having(std::string const& clause, SqlParam value) {
        _having = clause;
        _whereParams.push_back(std::move(value));
        return *this;
    }
    QueryBuilder& havingRaw(std::string const& clause) { _having = clause; return *this; }

    // ---- 生成 ----

    SqlStatement build() {
        std::string sql = "SELECT " + _selectCols + " FROM " + _tableName;

        if (!_joins.empty()) {
            for (auto& j : _joins) sql += " " + j;
        }

        if (!_whereClauses.empty()) {
            sql += " WHERE ";
            for (size_t i = 0; i < _whereClauses.size(); ++i) {
                if (i > 0) sql += " AND ";
                sql += _whereClauses[i];
            }
        }

        if (!_groupBy.empty()) sql += " GROUP BY " + _groupBy;
        if (!_having.empty()) sql += " HAVING " + _having;
        if (!_orderBy.empty()) sql += " ORDER BY " + _orderBy;
        if (_limitVal >= 0) sql += " LIMIT " + std::to_string(_limitVal);
        if (_offsetVal >= 0) sql += " OFFSET " + std::to_string(_offsetVal);

        return {sql, _whereParams};
    }
};

// ---- 快捷入口 ----

template<typename T>
inline QueryBuilder<T> query() {
    return QueryBuilder<T>();
}

// ============================== 容器子表 SQL 生成 ==============================

// ---- 容器子表命名 ----

template<typename T>
inline std::string containerSubTableName(const char* fieldName) {
    return std::string(OrmMetadata<T>::tableName) + "__" + fieldName;
}

// ---- 容器元素类型提取 ----

template<typename T> struct ContainerElemType;
template<typename T, typename A> struct ContainerElemType<std::vector<T, A>> { using type = T; };
template<typename T, typename A> struct ContainerElemType<std::list<T, A>> { using type = T; };
template<typename T, typename A> struct ContainerElemType<std::deque<T, A>> { using type = T; };
template<typename T, typename A> struct ContainerElemType<std::forward_list<T, A>> { using type = T; };
template<typename T, size_t N> struct ContainerElemType<std::array<T, N>> { using type = T; };
template<typename T, typename C> struct ContainerElemType<std::set<T, C>> { using type = T; };
template<typename T, typename C> struct ContainerElemType<std::unordered_set<T, C>> { using type = T; };
template<typename K, typename V, typename C> struct ContainerElemType<std::map<K, V, C>> { using type = std::pair<K, V>; };
template<typename K, typename V, typename C> struct ContainerElemType<std::unordered_map<K, V, C>> { using type = std::pair<K, V>; };

// 容器类别：有序容器（有 idx）、集合容器（无 idx）、映射容器（有 key）
enum class ContainerKind { Ordered, Set, Map };

template<typename T> struct ContainerKindOf;
template<typename T, typename A> struct ContainerKindOf<std::vector<T, A>> { static constexpr auto value = ContainerKind::Ordered; };
template<typename T, typename A> struct ContainerKindOf<std::list<T, A>> { static constexpr auto value = ContainerKind::Ordered; };
template<typename T, typename A> struct ContainerKindOf<std::deque<T, A>> { static constexpr auto value = ContainerKind::Ordered; };
template<typename T, typename A> struct ContainerKindOf<std::forward_list<T, A>> { static constexpr auto value = ContainerKind::Ordered; };
template<typename T, size_t N> struct ContainerKindOf<std::array<T, N>> { static constexpr auto value = ContainerKind::Ordered; };
template<typename T, typename C> struct ContainerKindOf<std::set<T, C>> { static constexpr auto value = ContainerKind::Set; };
template<typename T, typename C> struct ContainerKindOf<std::unordered_set<T, C>> { static constexpr auto value = ContainerKind::Set; };
template<typename K, typename V, typename C> struct ContainerKindOf<std::map<K, V, C>> { static constexpr auto value = ContainerKind::Map; };
template<typename K, typename V, typename C> struct ContainerKindOf<std::unordered_map<K, V, C>> { static constexpr auto value = ContainerKind::Map; };

// ---- 容器子表列定义 ----

template<typename ElemType, ContainerKind Kind>
inline std::string buildContainerColumns() {
    std::string result = "owner_id BIGINT NOT NULL";
    if constexpr (Kind == ContainerKind::Ordered) {
        result += ",\n    idx INT NOT NULL";
    }

    if constexpr (Kind == ContainerKind::Map) {
        // pair<K, V>: key_cols + value_cols
        using K = typename ElemType::first_type;
        using V = typename ElemType::second_type;

        if constexpr (is_orm_ptr_v<K>) {
            result += ",\n    `key_type_id` INT NOT NULL,\n    `key_id` BIGINT NOT NULL";
        } else if constexpr (has_orm_value_v<K>) {
            auto subCols = K::_ormBuildCreateCols("key");
            if (!subCols.empty()) result += ",\n    " + subCols;
        } else {
            result += ",\n    `key` " + std::string(SqlTypeMap<K>::value) + " NOT NULL";
        }

        if constexpr (is_orm_ptr_v<V>) {
            result += ",\n    `target_type_id` INT NOT NULL,\n    `target_id` BIGINT NOT NULL";
        } else if constexpr (has_orm_value_v<V>) {
            auto subCols = V::_ormBuildCreateCols("value");
            if (!subCols.empty()) result += ",\n    " + subCols;
        } else {
            result += ",\n    `value` " + std::string(SqlTypeMap<V>::value) + " NOT NULL";
        }
    } else {
        // 有序容器 / 集合容器：仅值列
        if constexpr (is_orm_ptr_v<ElemType>) {
            result += ",\n    `target_type_id` INT NOT NULL,\n    `target_id` BIGINT NOT NULL";
        } else if constexpr (has_orm_value_v<ElemType>) {
            auto subCols = ElemType::_ormBuildCreateCols("value");
            if (!subCols.empty()) result += ",\n    " + subCols;
        } else {
            result += ",\n    `value` " + std::string(SqlTypeMap<ElemType>::value) + " NOT NULL";
        }
    }
    return result;
}

// ---- 容器子表主键 ----

template<ContainerKind Kind>
inline std::string buildContainerPK() {
    if constexpr (Kind == ContainerKind::Ordered) {
        return "PRIMARY KEY (owner_id, idx)";
    } else if constexpr (Kind == ContainerKind::Set) {
        return "PRIMARY KEY (owner_id, `value`)";
    } else {
        return "PRIMARY KEY (owner_id, `key`)";
    }
}

// ---- 容器子表 CREATE TABLE ----

template<typename T, size_t I, typename = void>
struct ContainerTableBuilder {
    static SqlStatement build() { return {"", {}}; }
};

template<typename T, size_t I>
struct ContainerTableBuilder<T, I, std::enable_if_t<is_orm_container_v<std::tuple_element_t<I, typename T::M::TypeList>>>> {
    static SqlStatement build() {
        using FieldMeta = OrmFieldMeta<T, I>;
        if constexpr (FieldMeta::isTransient) return {"", {}};

        using FieldType = std::tuple_element_t<I, typename T::M::TypeList>;
        using ElemType = typename ContainerElemType<FieldType>::type;
        constexpr auto kind = ContainerKindOf<FieldType>::value;
        constexpr int fieldFlags = FieldMeta::flags;

        std::string tblName = containerSubTableName<T>(FieldMeta::name);
        std::string sql = "CREATE TABLE " + tblName + " (\n    ";
        sql += buildContainerColumns<ElemType, kind>();
        sql += ",\n    " + buildContainerPK<kind>();

        // FOREIGN KEY 带有 ON DELETE CASCADE / SET NULL
        if (fieldFlags & (ORM_ON_DELETE_CASCADE | ORM_ON_DELETE_SET_NULL)) {
            std::string parentTable(OrmMetadata<T>::tableName);
            sql += ",\n    FOREIGN KEY (owner_id) REFERENCES " + parentTable + "(id)";
            if (fieldFlags & ORM_ON_DELETE_CASCADE) {
                sql += " ON DELETE CASCADE";
            } else if (fieldFlags & ORM_ON_DELETE_SET_NULL) {
                sql += " ON DELETE SET NULL";
            }
        }

        sql += "\n)";
        return {sql, {}};
    }
};

template<typename T, size_t I>
inline SqlStatement createContainerTable() {
    return ContainerTableBuilder<T, I>::build();
}

template<typename T, size_t... Is>
inline std::vector<SqlStatement> createContainerTablesImpl(std::index_sequence<Is...>) {
    std::vector<SqlStatement> stmts;
    (..., [&]() {
        auto stmt = createContainerTable<T, Is>();
        if (!stmt.sql.empty()) stmts.push_back(std::move(stmt));
    }());
    return stmts;
}

template<typename T>
inline std::vector<SqlStatement> createContainerTables() {
    using Meta = OrmMetadata<T>;
    if constexpr (Meta::isAbstract) return {};
    return createContainerTablesImpl<T>(std::make_index_sequence<Meta::fieldCount>{});
}

// ---- createTableFull 增强版（包含容器子表） ----

template<typename T>
inline std::vector<SqlStatement> createTableFullWithContainers() {
    auto stmts = createTableFull<T>();
    auto containerStmts = createContainerTables<T>();
    stmts.insert(stmts.end(), containerStmts.begin(), containerStmts.end());
    return stmts;
}

// ---- 容器 SELECT ----

template<typename T, size_t I>
inline SqlStatement selectContainerElements(i64 ownerId) {
    using FieldMeta = OrmFieldMeta<T, I>;
    if constexpr (FieldMeta::isTransient) return {"", {}};

    using FieldType = std::tuple_element_t<I, typename T::M::TypeList>;
    if constexpr (!is_orm_container_v<FieldType>) return {"", {}};

    std::string tblName = containerSubTableName<T>(FieldMeta::name);
    return {"SELECT * FROM " + tblName + " WHERE owner_id = :p0", {ownerId}};
}

// ---- 容器 DELETE ----

template<typename T, size_t I>
inline SqlStatement deleteContainerByOwner(i64 ownerId) {
    using FieldMeta = OrmFieldMeta<T, I>;
    if constexpr (FieldMeta::isTransient) return {"", {}};

    using FieldType = std::tuple_element_t<I, typename T::M::TypeList>;
    if constexpr (!is_orm_container_v<FieldType>) return {"", {}};

    std::string tblName = containerSubTableName<T>(FieldMeta::name);
    return {"DELETE FROM " + tblName + " WHERE owner_id = :p0", {ownerId}};
}

} // namespace sp