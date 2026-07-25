// Copyright (c) 2026 aochenxiao
// SPDX-License-Identifier: MIT
#pragma once
/*
    StreamPunkSPOIExecutor.hpp — SPOI 执行器 (v0.2.0)

    负责：
    - 反序列化 SpoiStream（从 istream 读入）
    - 按函数指针表分发指令（O(1) 按 opcode 索引）
    - 路径导航（递归模板，在到达目标时用具体类型反序列化）
    - 写操作：SET / ADD / APPEND / REMOVE / INSERT / REPLACE / RESET / SETNULL
    - 读操作：FILTER / SELECT / SORT / REVERSE / TAKE / DROP / DISTINCT 等
    - 聚合：COUNT / ANY / ALL / FIND
    - 结果序列化：e_exec 将管道结果写入 std::ostream

    依赖：StreamPunkSPOI.hpp（协议定义）、StreamPunk.hpp（O/I 流）
*/

#include "StreamPunkSPOI.hpp"
#include "StreamPunkSPOIRange.hpp"
#include <iostream>
#include <type_traits>
#include <stdexcept>
#include <any>
#include <sstream>
#include <ranges>
#include <cstring>
#include <memory>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <deque>
#include <list>
#include <forward_list>

namespace sp {

// =============================== 类型特征（SPOI 专用） ===============================

template<typename T> struct has_type_list {
private:
    template<typename U> static auto test(int) -> decltype(typename U::M::TypeList{}, std::true_type{});
    template<typename U> static auto test(...) -> std::false_type;
public:
    static constexpr bool value = decltype(test<T>(0))::value;
};
template<typename T> inline constexpr bool has_type_list_v = has_type_list<T>::value;

// =============================== 读管道接口 ===============================

// C++23 后处理操作类型
enum class PostOp : u8 {
    e_none = 0,
    e_enumerate,
    e_chunk,
    e_slide,
    e_stride,
    e_adjacent,
    e_join,
};

// 管道输出模式
enum class OutputMode : u8 {
    e_normal = 0,  // 正常输出（elements 或 selectedBuf）
    e_keys,        // 输出 map 的 keys
    e_values,      // 输出 map 的 values
};

// 类型擦除的读管道，支持所有读操作
struct IReadPipeline {
    virtual ~IReadPipeline() = default;

    virtual void filter(SpoiCmpExpr const& cmp) = 0;
    virtual void takeWhile(SpoiCmpExpr const& cmp) = 0;
    virtual void dropWhile(SpoiCmpExpr const& cmp) = 0;
    virtual void sort(u32 memberIdx, bool ascending) = 0;
    virtual void take(u32 n) = 0;
    virtual void drop(u32 n) = 0;
    virtual void reverse() = 0;
    virtual void distinct() = 0;
    virtual void select(std::vector<u32> const& fieldIndices) = 0;
    virtual void serialize(std::ostream& os) = 0;
    virtual size_t elementCount() const = 0;
    virtual bool anyMatch(SpoiCmpExpr const& cmp) const = 0;
    virtual bool allMatch(SpoiCmpExpr const& cmp) const = 0;
    virtual void* firstMatch(SpoiCmpExpr const& cmp) const = 0;
    virtual void serializeFound(std::ostream& os, void* ptr) const = 0;
    virtual bool supportsKeysValues() const { return false; }
    virtual void setOutputMode(OutputMode) {}
    virtual bool isSelected() const { return false; }

    // C++23 ranges 后处理
    virtual void postEnumerate(u32 start) = 0;
    virtual void postChunk(u32 size) = 0;
    virtual void postSlide(u32 size) = 0;
    virtual void postStride(u32 step) = 0;
    virtual void postAdjacent(u32 n) = 0;
    virtual std::unique_ptr<IReadPipeline> joinFlatten(u32 memberIdx) = 0;
};

// =============================== 成员操作辅助 ===============================

// 对指定索引的成员执行操作（编译期展开成员 tuple）
template<typename Tuple, typename Op, size_t... Is>
auto _withMemberImpl(Tuple& members, u32 memberIdx, Op&& op, std::index_sequence<Is...>) {
    using R = decltype(op(std::get<0>(members))); // 推导返回类型
    bool found = false;
    if constexpr (std::is_void_v<R>) {
        auto tryMember = [&](auto idxConst) {
            if (!found && static_cast<u32>(idxConst) == memberIdx) {
                found = true;
                op(std::get<idxConst>(members));
            }
        };
        (tryMember(std::integral_constant<size_t, Is>{}), ...);
    } else {
        R result{};
        auto tryMember = [&](auto idxConst) {
            if (!found && static_cast<u32>(idxConst) == memberIdx) {
                found = true;
                result = op(std::get<idxConst>(members));
            }
        };
        (tryMember(std::integral_constant<size_t, Is>{}), ...);
        if (!found) throw SpDataError("SPOI: member index " + std::to_string(memberIdx) + " not found");
        return result;
    }
}

template<typename ElemT, typename Op>
auto _withMember(ElemT& elem, u32 memberIdx, Op&& op) {
    using TupleType = decltype(std::declval<ElemT&>().memberTuple());
    constexpr size_t N = std::tuple_size_v<std::decay_t<TupleType>>;
    auto members = elem.memberTuple();
    return _withMemberImpl(members, memberIdx, std::forward<Op>(op), std::make_index_sequence<N - 1>{});
}

// 对指定索引的成员执行操作（const 版本）
template<typename ElemT, typename Op>
auto _withMemberConst(ElemT const& elem, u32 memberIdx, Op&& op) {
    using TupleType = decltype(std::declval<ElemT&>().memberTuple());
    constexpr size_t N = std::tuple_size_v<std::decay_t<TupleType>>;
    auto members = const_cast<ElemT&>(elem).memberTuple();
    return _withMemberImpl(members, memberIdx, std::forward<Op>(op), std::make_index_sequence<N - 1>{});
}

// 对指定字段索引列表执行操作（用于 select 批量序列化）
template<typename Tuple, typename Fn, size_t... Is>
void _withFieldIndices(Tuple& members, std::vector<u32> const& indices, Fn&& fn, std::index_sequence<Is...>) {
    for (u32 idx : indices) {
        bool found = false;
        auto tryMember = [&](auto idxConst) {
            if (!found && static_cast<u32>(idxConst) == idx) {
                found = true;
                fn(idx, std::get<idxConst>(members));
            }
        };
        (tryMember(std::integral_constant<size_t, Is>{}), ...);
    }
}

template<typename ElemT, typename Fn>
void _withFieldIndices(ElemT& elem, std::vector<u32> const& indices, Fn&& fn) {
    using TupleType = decltype(std::declval<ElemT&>().memberTuple());
    constexpr size_t N = std::tuple_size_v<std::decay_t<TupleType>>;
    auto members = elem.memberTuple();
    _withFieldIndices(members, indices, std::forward<Fn>(fn), std::make_index_sequence<N - 1>{});
}

// =============================== ReadPipeline 模板实现 ===============================

template<typename ElemT>
struct ReadPipeline : IReadPipeline {
    std::vector<ElemT*> elements;       // 指向匹配元素的指针
    std::vector<std::vector<u8>> selectedBuf; // select 后的缓冲区（非空表示已 select）
    std::vector<u32> selectedFields;    // select 的字段索引
    bool _isMap = false;                // 是否来自 map 容器
    bool _isSet = false;                // 是否来自 set 容器
    OutputMode _outputMode = OutputMode::e_normal; // 输出模式
    std::vector<std::vector<u8>> _mapKeys; // map 的序列化 keys（_isMap 时有效）

    // C++23 后处理参数
    PostOp postOp = PostOp::e_none;
    u32 postParam = 0;
    u32 joinMemberIdx = 0;

    // 从容器填充元素指针
    template<typename ContainerT>
    void populate(ContainerT& container) {
        elements.clear();
        for (auto& elem : container) {
            elements.push_back(&elem);
        }
    }

    // 从 set 容器填充（元素为 const，需 const_cast）
    template<typename ContainerT>
    void populateSet(ContainerT& container) {
        elements.clear();
        for (auto& elem : container) {
            elements.push_back(const_cast<ElemT*>(&elem));
        }
    }

    // 从 map 填充：存储 values 指针和序列化 keys
    template<typename ContainerT>
    void populateMap(ContainerT& container) {
        elements.clear();
        _mapKeys.clear();
        for (auto& kv : container) {
            elements.push_back(&kv.second);
            std::stringstream keySS;
            O ko(keySS);
            ko << kv.first;
            auto keyStr = keySS.str();
            _mapKeys.push_back(std::vector<u8>(keyStr.begin(), keyStr.end()));
        }
    }

    void setOutputMode(OutputMode mode) override { _outputMode = mode; }

    size_t elementCount() const override {
        if (_outputMode == OutputMode::e_keys) return _mapKeys.size();
        if (postOp == PostOp::e_stride) {
            size_t base = selectedBuf.empty() ? elements.size() : selectedBuf.size();
            return (base + postParam - 1) / postParam;
        }
        return selectedBuf.empty() ? elements.size() : selectedBuf.size();
    }

    void filter(SpoiCmpExpr const& cmp) override {
        if (_outputMode != OutputMode::e_normal) return;
        if (!selectedBuf.empty()) return;
        if constexpr (has_type_list_v<ElemT>) {
            auto it = std::remove_if(elements.begin(), elements.end(), [&](ElemT* elem) {
                return !_evaluateCmpExpr(*elem, cmp);
            });
            elements.erase(it, elements.end());
        }
    }

    void takeWhile(SpoiCmpExpr const& cmp) override {
        if (_outputMode != OutputMode::e_normal) return;
        if (!selectedBuf.empty()) return;
        if constexpr (has_type_list_v<ElemT>) {
            auto it = std::find_if(elements.begin(), elements.end(), [&](ElemT* elem) {
                return !_evaluateCmpExpr(*elem, cmp);
            });
            elements.erase(it, elements.end());
        }
    }

    void dropWhile(SpoiCmpExpr const& cmp) override {
        if (_outputMode != OutputMode::e_normal) return;
        if (!selectedBuf.empty()) return;
        if constexpr (has_type_list_v<ElemT>) {
            auto it = std::find_if(elements.begin(), elements.end(), [&](ElemT* elem) {
                return !_evaluateCmpExpr(*elem, cmp);
            });
            elements.erase(elements.begin(), it);
        }
    }

    void sort(u32 memberIdx, bool ascending) override {
        if (_outputMode != OutputMode::e_normal) return;
        if (!selectedBuf.empty()) return;
        if constexpr (has_type_list_v<ElemT>) {
            std::sort(elements.begin(), elements.end(), [&](ElemT* a, ElemT* b) {
                return _compareMember(*a, *b, memberIdx, ascending);
            });
        }
    }

    void take(u32 n) override {
        if (!selectedBuf.empty()) {
            if (n < selectedBuf.size()) selectedBuf.resize(n);
            if (_isMap) _mapKeys.resize(n);
            return;
        }
        if (n < elements.size()) {
            elements.resize(n);
            if (_isMap) _mapKeys.resize(n);
        }
    }

    void drop(u32 n) override {
        if (!selectedBuf.empty()) {
            if (n >= selectedBuf.size()) selectedBuf.clear();
            else selectedBuf.erase(selectedBuf.begin(), selectedBuf.begin() + n);
            return;
        }
        if (n >= elements.size()) {
            elements.clear();
            if (_isMap) _mapKeys.clear();
        } else {
            elements.erase(elements.begin(), elements.begin() + n);
            if (_isMap) _mapKeys.erase(_mapKeys.begin(), _mapKeys.begin() + n);
        }
    }

    void reverse() override {
        if (!selectedBuf.empty()) {
            std::reverse(selectedBuf.begin(), selectedBuf.end());
            return;
        }
        std::reverse(elements.begin(), elements.end());
        if (_isMap) std::reverse(_mapKeys.begin(), _mapKeys.end());
    }

    void distinct() override {
        if (_outputMode != OutputMode::e_normal) return;
        if (!selectedBuf.empty()) return;
        if (_isMap) {
            std::unordered_set<std::string> seen;
            std::vector<size_t> keepIndices;
            for (size_t i = 0; i < elements.size(); ++i) {
                std::stringstream ss;
                O o(ss);
                o << *elements[i];
                auto key = ss.str();
                if (!seen.count(key)) {
                    seen.insert(key);
                    keepIndices.push_back(i);
                }
            }
            std::vector<ElemT*> newElements;
            std::vector<std::vector<u8>> newKeys;
            for (auto idx : keepIndices) {
                newElements.push_back(elements[idx]);
                newKeys.push_back(std::move(_mapKeys[idx]));
            }
            elements = std::move(newElements);
            _mapKeys = std::move(newKeys);
        } else {
            std::unordered_set<std::string> seen;
            auto it = std::remove_if(elements.begin(), elements.end(), [&](ElemT* elem) {
                std::stringstream ss;
                O o(ss);
                o << *elem;
                auto key = ss.str();
                if (seen.count(key)) return true;
                seen.insert(std::move(key));
                return false;
            });
            elements.erase(it, elements.end());
        }
    }

    void select(std::vector<u32> const& fieldIndices) override {
        if (_outputMode != OutputMode::e_normal) return;
        if constexpr (has_type_list_v<ElemT>) {
            selectedFields = fieldIndices;
            selectedBuf.clear();
            for (auto* elem : elements) {
                std::stringstream ss;
                O o(ss);
                _withFieldIndices(*elem, fieldIndices, [&](u32, auto& member) {
                    o << member;
                });
                auto str = ss.str();
                selectedBuf.push_back(std::vector<u8>(str.begin(), str.end()));
            }
            elements.clear(); // select 后元素指针不再有效
        }
    }

    void serialize(std::ostream& os) override {
        SpoiResult result;
        result.resultType = static_cast<u8>(ResultType::e_vector);

        // ── map keys 输出模式 ──
        if (_outputMode == OutputMode::e_keys) {
            std::stringstream dataSS;
            writeVarint(dataSS, static_cast<u32>(_mapKeys.size()));
            for (auto& keyBuf : _mapKeys) {
                dataSS.write(reinterpret_cast<const char*>(keyBuf.data()), keyBuf.size());
            }
            auto dataStr = dataSS.str();
            result.data = std::vector<u8>(dataStr.begin(), dataStr.end());
            O o(os);
            o << result;
            return;
        }

        if (postOp == PostOp::e_join) {
            _serializeJoin(os);
            return;
        }

        if (postOp != PostOp::e_none) {
            _serializeWithPostOp(os);
            return;
        }

        if (!selectedBuf.empty()) {
            std::stringstream dataSS;
            writeVarint(dataSS, static_cast<u32>(selectedBuf.size()));
            for (auto& buf : selectedBuf) {
                dataSS.write(reinterpret_cast<const char*>(buf.data()), buf.size());
            }
            auto dataStr = dataSS.str();
            result.data = std::vector<u8>(dataStr.begin(), dataStr.end());
            O o(os);
            o << result;
        } else {
            std::stringstream dataSS;
            writeVarint(dataSS, static_cast<u32>(elements.size()));
            for (auto* elem : elements) {
                std::stringstream elemSS;
                O eo(elemSS);
                eo << *elem;
                auto elemStr = elemSS.str();
                dataSS.write(elemStr.data(), elemStr.size());
            }
            auto dataStr = dataSS.str();
            result.data = std::vector<u8>(dataStr.begin(), dataStr.end());
            O o(os);
            o << result;
        }
    }

    // ── join 序列化（支持 select 后回退）──
    void _serializeJoin(std::ostream& os) {
        if (!selectedBuf.empty()) {
            // select 后的 join：直接输出 select 结果
            SpoiResult result;
            result.resultType = static_cast<u8>(ResultType::e_vector);
            std::stringstream dataSS;
            writeVarint(dataSS, static_cast<u32>(selectedBuf.size()));
            for (auto& buf : selectedBuf) {
                dataSS.write(reinterpret_cast<const char*>(buf.data()), buf.size());
            }
            auto dataStr = dataSS.str();
            result.data = std::vector<u8>(dataStr.begin(), dataStr.end());
            O o(os);
            o << result;
            return;
        }

        if constexpr (has_type_list_v<ElemT>) {
            std::stringstream dataSS;
            u32 totalCount = 0;

            for (auto* elem : elements) {
                _withMemberConst(*elem, joinMemberIdx, [&](auto& nestedContainer) {
                    using NestedT = std::decay_t<decltype(nestedContainer)>;
                    if constexpr (is_ordered_container_v<NestedT> || is_set_container_v<NestedT> || is_flist_v<NestedT>) {
                        for (auto& nestedElem : nestedContainer) {
                            std::stringstream elemSS;
                            O eo(elemSS);
                            if constexpr (is_set_container_v<NestedT>) {
                                eo << const_cast<typename NestedT::value_type&>(nestedElem);
                            } else {
                                eo << nestedElem;
                            }
                            auto elemStr = elemSS.str();
                            dataSS.write(elemStr.data(), elemStr.size());
                            ++totalCount;
                        }
                    } else if constexpr (is_assoc_container_v<NestedT>) {
                        for (auto& kv : nestedContainer) {
                            std::stringstream elemSS;
                            O eo(elemSS);
                            eo << kv.second;
                            auto elemStr = elemSS.str();
                            dataSS.write(elemStr.data(), elemStr.size());
                            ++totalCount;
                        }
                    }
                });
            }

            SpoiResult result;
            result.resultType = static_cast<u8>(ResultType::e_vector);
            std::stringstream finalSS;
            writeVarint(finalSS, totalCount);
            auto dataStr = dataSS.str();
            finalSS.write(dataStr.data(), dataStr.size());
            auto finalStr = finalSS.str();
            result.data = std::vector<u8>(finalStr.begin(), finalStr.end());
            O o(os);
            o << result;
        }
    }

    // ── C++23 后处理序列化（支持 select 后）──
    void _serializeWithPostOp(std::ostream& os) {
        if (!selectedBuf.empty()) {
            _serializePostOpOnSelected(os);
            return;
        }

        size_t N = elements.size();
        std::stringstream dataSS;

        if (postOp == PostOp::e_enumerate) {
            u32 start = postParam;
            writeVarint(dataSS, static_cast<u32>(N));
            for (size_t i = 0; i < N; ++i) {
                std::stringstream idxSS;
                O idxO(idxSS);
                idxO << static_cast<u32>(start + i);
                auto idxStr = idxSS.str();
                dataSS.write(idxStr.data(), idxStr.size());
                std::stringstream elemSS;
                O eo(elemSS);
                eo << *elements[i];
                auto elemStr = elemSS.str();
                dataSS.write(elemStr.data(), elemStr.size());
            }
        } else if (postOp == PostOp::e_chunk) {
            u32 chunkSize = postParam;
            u32 numChunks = static_cast<u32>((N + chunkSize - 1) / chunkSize);
            writeVarint(dataSS, numChunks);
            for (size_t i = 0; i < N; i += chunkSize) {
                u32 actualSize = static_cast<u32>(std::min(chunkSize, static_cast<u32>(N - i)));
                writeVarint(dataSS, actualSize);
                for (size_t j = i; j < std::min(i + chunkSize, N); ++j) {
                    std::stringstream elemSS;
                    O eo(elemSS);
                    eo << *elements[j];
                    auto elemStr = elemSS.str();
                    dataSS.write(elemStr.data(), elemStr.size());
                }
            }
        } else if (postOp == PostOp::e_slide) {
            u32 windowSize = postParam;
            if (windowSize > N) {
                // C++23 std::views::slide: 窗口超过总数时为空
                writeVarint(dataSS, 0);
            } else {
                u32 numWindows = static_cast<u32>(N - windowSize + 1);
                writeVarint(dataSS, numWindows);
                for (size_t i = 0; i <= N - windowSize; ++i) {
                    writeVarint(dataSS, windowSize);
                    for (size_t j = i; j < i + windowSize; ++j) {
                        std::stringstream elemSS;
                        O eo(elemSS);
                        eo << *elements[j];
                        auto elemStr = elemSS.str();
                        dataSS.write(elemStr.data(), elemStr.size());
                    }
                }
            }
        } else if (postOp == PostOp::e_stride) {
            u32 step = postParam;
            u32 count = 0;
            std::stringstream bufSS;
            for (size_t i = 0; i < N; i += step) {
                std::stringstream elemSS;
                O eo(elemSS);
                eo << *elements[i];
                auto elemStr = elemSS.str();
                bufSS.write(elemStr.data(), elemStr.size());
                ++count;
            }
            writeVarint(dataSS, count);
            auto bufStr = bufSS.str();
            dataSS.write(bufStr.data(), bufStr.size());
        } else if (postOp == PostOp::e_adjacent) {
            u32 adjSize = postParam;
            if (adjSize < 2) adjSize = 2;
            if (adjSize > N) adjSize = static_cast<u32>(N);
            u32 numGroups = static_cast<u32>(N - adjSize + 1);
            writeVarint(dataSS, numGroups);
            for (size_t i = 0; i <= N - adjSize; ++i) {
                writeVarint(dataSS, adjSize);
                for (size_t j = i; j < i + adjSize; ++j) {
                    std::stringstream elemSS;
                    O eo(elemSS);
                    eo << *elements[j];
                    auto elemStr = elemSS.str();
                    dataSS.write(elemStr.data(), elemStr.size());
                }
            }
        }

        SpoiResult result;
        result.resultType = static_cast<u8>(ResultType::e_vector);
        auto dataStr = dataSS.str();
        result.data = std::vector<u8>(dataStr.begin(), dataStr.end());
        O o(os);
        o << result;
    }

    // ── select 后的 C++23 后处理 ──
    void _serializePostOpOnSelected(std::ostream& os) {
        size_t N = selectedBuf.size();
        std::stringstream dataSS;

        if (postOp == PostOp::e_enumerate) {
            u32 start = postParam;
            writeVarint(dataSS, static_cast<u32>(N));
            for (size_t i = 0; i < N; ++i) {
                std::stringstream idxSS;
                O idxO(idxSS);
                idxO << static_cast<u32>(start + i);
                auto idxStr = idxSS.str();
                dataSS.write(idxStr.data(), idxStr.size());
                dataSS.write(reinterpret_cast<const char*>(selectedBuf[i].data()), selectedBuf[i].size());
            }
        } else if (postOp == PostOp::e_chunk) {
            u32 chunkSize = postParam;
            u32 numChunks = static_cast<u32>((N + chunkSize - 1) / chunkSize);
            writeVarint(dataSS, numChunks);
            for (size_t i = 0; i < N; i += chunkSize) {
                u32 actualSize = static_cast<u32>(std::min(chunkSize, static_cast<u32>(N - i)));
                writeVarint(dataSS, actualSize);
                for (size_t j = i; j < std::min(i + chunkSize, N); ++j) {
                    dataSS.write(reinterpret_cast<const char*>(selectedBuf[j].data()), selectedBuf[j].size());
                }
            }
        } else if (postOp == PostOp::e_slide) {
            u32 windowSize = postParam;
            if (windowSize > N) {
                writeVarint(dataSS, 0);
            } else {
                u32 numWindows = static_cast<u32>(N - windowSize + 1);
                writeVarint(dataSS, numWindows);
                for (size_t i = 0; i <= N - windowSize; ++i) {
                    writeVarint(dataSS, windowSize);
                    for (size_t j = i; j < i + windowSize; ++j) {
                        dataSS.write(reinterpret_cast<const char*>(selectedBuf[j].data()), selectedBuf[j].size());
                    }
                }
            }
        } else if (postOp == PostOp::e_stride) {
            u32 step = postParam;
            u32 count = 0;
            std::stringstream bufSS;
            for (size_t i = 0; i < N; i += step) {
                bufSS.write(reinterpret_cast<const char*>(selectedBuf[i].data()), selectedBuf[i].size());
                ++count;
            }
            writeVarint(dataSS, count);
            auto bufStr = bufSS.str();
            dataSS.write(bufStr.data(), bufStr.size());
        } else if (postOp == PostOp::e_adjacent) {
            u32 adjSize = postParam;
            if (adjSize < 2) adjSize = 2;
            if (adjSize > N) adjSize = static_cast<u32>(N);
            u32 numGroups = static_cast<u32>(N - adjSize + 1);
            writeVarint(dataSS, numGroups);
            for (size_t i = 0; i <= N - adjSize; ++i) {
                writeVarint(dataSS, adjSize);
                for (size_t j = i; j < i + adjSize; ++j) {
                    dataSS.write(reinterpret_cast<const char*>(selectedBuf[j].data()), selectedBuf[j].size());
                }
            }
        }

        SpoiResult result;
        result.resultType = static_cast<u8>(ResultType::e_vector);
        auto dataStr = dataSS.str();
        result.data = std::vector<u8>(dataStr.begin(), dataStr.end());
        O o(os);
        o << result;
    }

    // ── C++23 后处理方法 ──
    void postEnumerate(u32 start) override { postOp = PostOp::e_enumerate; postParam = start; }
    void postChunk(u32 size) override { postOp = PostOp::e_chunk; postParam = size; }
    void postSlide(u32 size) override { postOp = PostOp::e_slide; postParam = size; }
    void postStride(u32 step) override { postOp = PostOp::e_stride; postParam = step; }
    void postAdjacent(u32 n) override { postOp = PostOp::e_adjacent; postParam = n; }
    std::unique_ptr<IReadPipeline> joinFlatten(u32 memberIdx) override {
        std::unique_ptr<IReadPipeline> result;

        if constexpr (has_type_list_v<ElemT>) {
            for (auto* elem : elements) {
                _withMemberConst(*elem, memberIdx, [&](auto& nestedContainer) {
                    using NestedT = std::decay_t<decltype(nestedContainer)>;
                    using NestedElemT = spoi_elem_type_t<NestedT>;

                    if (!result) {
                        result = std::make_unique<ReadPipeline<NestedElemT>>();
                    }

                    auto* pipeline = static_cast<ReadPipeline<NestedElemT>*>(result.get());

                    if constexpr (is_ordered_container_v<NestedT> || is_flist_v<NestedT>) {
                        for (auto& nestedElem : nestedContainer) {
                            pipeline->elements.push_back(&nestedElem);
                        }
                    } else if constexpr (is_set_container_v<NestedT>) {
                        for (auto& nestedElem : nestedContainer) {
                            pipeline->elements.push_back(const_cast<NestedElemT*>(&nestedElem));
                        }
                    } else if constexpr (is_assoc_container_v<NestedT>) {
                        pipeline->_isMap = true;
                        for (auto& kv : nestedContainer) {
                            pipeline->elements.push_back(&kv.second);
                            std::stringstream keySS;
                            O ko(keySS);
                            ko << kv.first;
                            auto keyStr = keySS.str();
                            pipeline->_mapKeys.push_back(std::vector<u8>(keyStr.begin(), keyStr.end()));
                        }
                    }
                });
            }
        }

        if (!result) {
            result = std::make_unique<ReadPipeline<ElemT>>();
        }
        return result;
    }

    bool anyMatch(SpoiCmpExpr const& cmp) const override {
        if constexpr (has_type_list_v<ElemT>) {
            for (auto* elem : elements) {
                if (_evaluateCmpExpr(*elem, cmp)) return true;
            }
        }
        return false;
    }

    bool allMatch(SpoiCmpExpr const& cmp) const override {
        if constexpr (has_type_list_v<ElemT>) {
            for (auto* elem : elements) {
                if (!_evaluateCmpExpr(*elem, cmp)) return false;
            }
            return true;
        }
        return false;
    }

    void* firstMatch(SpoiCmpExpr const& cmp) const override {
        if constexpr (has_type_list_v<ElemT>) {
            for (auto* elem : elements) {
                if (_evaluateCmpExpr(*elem, cmp)) return static_cast<void*>(elem);
            }
        }
        return nullptr;
    }

    // 序列化 find 找到的单个元素
    void serializeFound(std::ostream& os, void* ptr) const override {
        auto* elem = static_cast<ElemT*>(ptr);
        SpoiResult result;
        result.resultType = static_cast<u8>(ResultType::e_single);
        std::stringstream elemSS;
        O eo(elemSS);
        eo << *elem;
        auto str = elemSS.str();
        result.data = std::vector<u8>(str.begin(), str.end());
        O o(os);
        o << result;
    }

    bool isSelected() const override { return !selectedBuf.empty(); }

    bool supportsKeysValues() const override { return _isMap; }

private:
    // 评估 SpoiCmpExpr 对元素的匹配结果
    static bool _evaluateCmpExpr(ElemT const& elem, SpoiCmpExpr const& cmp) {
        return _withMemberConst(elem, cmp.memberIdx, [&](auto& member) -> bool {
            using MType = std::decay_t<decltype(member)>;
            MType target{};
            std::string valStr(cmp.value.begin(), cmp.value.end());
            std::stringstream ss(valStr);
            I i(ss);
            i >> target;
            return evaluateCmp(member, static_cast<CmpOp>(cmp.cmpOp), target);
        });
    }

    // 比较两个元素指定成员的大小
    static bool _compareMember(ElemT const& a, ElemT const& b, u32 memberIdx, bool ascending) {
        // 序列化两个成员的值为字节，按字节序比较
        std::string valA, valB;
        _withMemberConst(a, memberIdx, [&](auto& ma) {
            std::stringstream ss;
            O o(ss);
            o << ma;
            valA = ss.str();
        });
        _withMemberConst(b, memberIdx, [&](auto& mb) {
            std::stringstream ss;
            O o(ss);
            o << mb;
            valB = ss.str();
        });
        int cmp = valA.compare(valB);
        return ascending ? (cmp < 0) : (cmp > 0);
    }
};

// =============================== 执行上下文 ===============================

struct SpoiContext {
    void*  rootObj = nullptr;
    std::ostream* result = nullptr;
    bool hasError = false;
    std::string errorMsg;

    // 读管道
    std::unique_ptr<IReadPipeline> pipeline;
    bool hasPipeline = false;
};

// =============================== 路径导航 + 操作应用 ===============================

// 按索引访问结构体成员
template<typename T>
void* _memberByIndex(T& obj, u32 memberIdx) {
    auto members = obj.memberTuple();
    constexpr size_t N = std::tuple_size_v<std::decay_t<decltype(members)>>;
    return _memberByIndexImpl(members, memberIdx, std::make_index_sequence<N - 1>{});
}

template<typename Tuple, size_t... Is>
void* _memberByIndexImpl(Tuple& members, u32 memberIdx, std::index_sequence<Is...>) {
    void* result = nullptr;
    auto tryMember = [&](auto memberIdxConst) {
        if (static_cast<u32>(memberIdxConst) == memberIdx) {
            result = static_cast<void*>(&std::get<memberIdxConst>(members));
        }
    };
    (tryMember(std::integral_constant<size_t, Is>{}), ...);
    return result;
}

// 递归路径导航 + 到达目标后调用回调（回调知道具体类型）
template<typename T, typename Fn>
void _navigateWith(T& obj, std::vector<u32> const& path, Fn&& fn, size_t depth = 0) {
    if (depth >= path.size()) {
        // 到达路径终点，直接传递目标对象（不自动解包 optional，让回调自行处理）
        fn(obj);
        return;
    }

    u32 seg = path[depth];

    if (seg == PATH_DEREF) {
        if constexpr (is_sptr_v<T>) {
            if (!obj) throw SpDataError("SPOI: deref null shared_ptr");
            _navigateWith(*obj, path, std::forward<Fn>(fn), depth + 1);
        } else if constexpr (is_uptr_v<T>) {
            if (!obj) throw SpDataError("SPOI: deref null unique_ptr");
            _navigateWith(*obj, path, std::forward<Fn>(fn), depth + 1);
        } else {
            throw SpDataError("SPOI: PATH_DEREF on unsupported pointer type");
        }
    } else if constexpr (is_ordered_container_v<T>) {
        size_t idx = static_cast<size_t>(seg);
        if constexpr (is_vector_v<T> || is_deque_v<T>) {
            if (idx >= obj.size()) obj.resize(idx + 1);
        }
        if constexpr (is_list_v<T> || is_flist_v<T>) {
            if (idx >= static_cast<size_t>(std::distance(obj.begin(), obj.end()))) {
                throw SpDataError("SPOI: index out of range for list/forward_list");
            }
        }
        auto it = obj.begin();
        std::advance(it, idx);
        _navigateWith(*it, path, std::forward<Fn>(fn), depth + 1);
    } else if constexpr (is_assoc_container_v<T>) {
        if (seg >= static_cast<size_t>(std::distance(obj.begin(), obj.end()))) {
            throw SpDataError("SPOI: index out of range for map");
        }
        auto it = obj.begin();
        std::advance(it, seg);
        _navigateWith(it->second, path, std::forward<Fn>(fn), depth + 1);
    } else if constexpr (is_set_container_v<T>) {
        if (seg >= static_cast<size_t>(std::distance(obj.begin(), obj.end()))) {
            throw SpDataError("SPOI: index out of range for set");
        }
        auto it = obj.begin();
        std::advance(it, seg);
        _navigateWith(const_cast<typename T::value_type&>(*it), path, std::forward<Fn>(fn), depth + 1);
    } else if constexpr (is_optional_v<T>) {
        if (!obj.has_value()) obj.emplace();
        _navigateWith(*obj, path, std::forward<Fn>(fn), depth + 1);
    } else if constexpr (is_flist_v<T>) {
        if (seg >= static_cast<size_t>(std::distance(obj.begin(), obj.end()))) {
            throw SpDataError("SPOI: index out of range for forward_list");
        }
        auto it = obj.begin();
        std::advance(it, seg);
        _navigateWith(*it, path, std::forward<Fn>(fn), depth + 1);
    } else if constexpr (has_type_list_v<T>) {
        _navigateMemberWith(obj, seg, path, std::forward<Fn>(fn), depth + 1);
    } else {
        throw SpDataError("SPOI: cannot navigate at depth " + std::to_string(depth));
    }
}

// 按成员索引继续导航
template<typename T, typename Fn>
void _navigateMemberWith(T& obj, u32 memberIdx, std::vector<u32> const& path, Fn&& fn, size_t depth) {
    auto members = obj.memberTuple();
    constexpr size_t N = std::tuple_size_v<std::decay_t<decltype(members)>>;
    _navigateMemberWithImpl(members, memberIdx, path, std::forward<Fn>(fn), depth, std::make_index_sequence<N - 1>{});
}

template<typename Tuple, typename Fn, size_t... Is>
void _navigateMemberWithImpl(Tuple& members, u32 memberIdx, std::vector<u32> const& path, Fn&& fn, size_t depth, std::index_sequence<Is...>) {
    bool found = false;
    auto tryMember = [&](auto memberIdxConst) {
        if (!found && static_cast<u32>(memberIdxConst) == memberIdx) {
            found = true;
            auto& member = std::get<memberIdxConst>(members);
            _navigateWith(member, path, std::forward<Fn>(fn), depth);
        }
    };
    (tryMember(std::integral_constant<size_t, Is>{}), ...);
    if (!found) {
        throw SpDataError("SPOI: member index " + std::to_string(memberIdx) + " not found");
    }
}

// =============================== 函数指针类型 ===============================

using SpoiHandler = void(*)(SpoiContext& ctx, SpoiInstruction const& inst);

// =============================== 读操作处理函数 ===============================

static void h_filter(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    auto cmp = SpoiCmpExpr::deserialize(inst.operand);
    ctx.pipeline->filter(cmp);
}

static void h_select(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    // 解析字段索引：[count: u32][idx0: u32][idx1: u32]...
    if (inst.operand.size() < sizeof(u32)) return;
    u32 count;
    std::memcpy(&count, inst.operand.data(), sizeof(u32));
    std::vector<u32> indices;
    for (u32 i = 0; i < count; ++i) {
        size_t offset = sizeof(u32) * (1 + i);
        if (offset + sizeof(u32) > inst.operand.size()) break;
        u32 idx;
        std::memcpy(&idx, inst.operand.data() + offset, sizeof(u32));
        indices.push_back(idx);
    }
    ctx.pipeline->select(indices);
}

static void h_sort(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    if (inst.operand.size() < sizeof(u32) + sizeof(u8)) return;
    u32 memberIdx;
    std::memcpy(&memberIdx, inst.operand.data(), sizeof(u32));
    bool ascending = inst.operand[sizeof(u32)] != 0;
    ctx.pipeline->sort(memberIdx, ascending);
}

static void h_reverse(SpoiContext& ctx, SpoiInstruction const&) {
    if (!ctx.pipeline) return;
    ctx.pipeline->reverse();
}

static void h_take(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    if (inst.operand.size() < sizeof(u32)) return;
    u32 n;
    std::memcpy(&n, inst.operand.data(), sizeof(u32));
    ctx.pipeline->take(n);
}

static void h_drop(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    if (inst.operand.size() < sizeof(u32)) return;
    u32 n;
    std::memcpy(&n, inst.operand.data(), sizeof(u32));
    ctx.pipeline->drop(n);
}

static void h_takewhile(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    auto cmp = SpoiCmpExpr::deserialize(inst.operand);
    ctx.pipeline->takeWhile(cmp);
}

static void h_dropwhile(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    auto cmp = SpoiCmpExpr::deserialize(inst.operand);
    ctx.pipeline->dropWhile(cmp);
}

static void h_distinct(SpoiContext& ctx, SpoiInstruction const&) {
    if (!ctx.pipeline) return;
    ctx.pipeline->distinct();
}

// =============================== 聚合操作处理函数 ===============================

static void h_count(SpoiContext& ctx, SpoiInstruction const&) {
    if (!ctx.pipeline || !ctx.result) return;
    SpoiResult result;
    result.resultType = static_cast<u8>(ResultType::e_count);
    u32 n = static_cast<u32>(ctx.pipeline->elementCount());
    std::stringstream dataSS;
    O o(dataSS);
    o << n;
    auto str = dataSS.str();
    result.data = std::vector<u8>(str.begin(), str.end());
    O o2(*ctx.result);
    o2 << result;
}

static void h_any(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline || !ctx.result) return;
    bool found;
    if (inst.operand.size() == 1) {
        // 常量谓词：true 且容器非空 → found；false 或空容器 → not found
        found = inst.operand[0] != 0 && ctx.pipeline->elementCount() > 0;
    } else {
        auto cmp = SpoiCmpExpr::deserialize(inst.operand);
        found = ctx.pipeline->anyMatch(cmp);
    }
    SpoiResult result;
    result.resultType = static_cast<u8>(ResultType::e_bool);
    result.data = { static_cast<u8>(found ? 1 : 0) };
    O o(*ctx.result);
    o << result;
}

static void h_all(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline || !ctx.result) return;
    bool allMatch;
    if (inst.operand.size() == 1) {
        // 常量谓词：true 或空容器 → allMatch（空集全称量化为真）；false 且非空 → not allMatch
        bool constVal = inst.operand[0] != 0;
        allMatch = constVal || ctx.pipeline->elementCount() == 0;
    } else {
        auto cmp = SpoiCmpExpr::deserialize(inst.operand);
        allMatch = ctx.pipeline->allMatch(cmp);
    }
    SpoiResult result;
    result.resultType = static_cast<u8>(ResultType::e_bool);
    result.data = { static_cast<u8>(allMatch ? 1 : 0) };
    O o(*ctx.result);
    o << result;
}

static void h_find(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline || !ctx.result) return;
    auto cmp = SpoiCmpExpr::deserialize(inst.operand);
    auto* ptr = ctx.pipeline->firstMatch(cmp);
    SpoiResult result;
    if (ptr) {
        result.resultType = static_cast<u8>(ResultType::e_single);
        // 序列化找到的元素：通过 pipeline 的 serializeOne 方法
        std::stringstream elemSS;
        ctx.pipeline->serializeFound(*ctx.result, ptr);
        return; // serializeFound 已直接写入 ctx.result
    } else {
        result.resultType = static_cast<u8>(ResultType::e_optional);
        result.data = {0}; // has_value = false
        O o(*ctx.result);
        o << result;
    }
}

// =============================== 容器操作处理函数 ===============================

static void h_keys(SpoiContext& ctx, SpoiInstruction const&) {
    if (!ctx.pipeline) return;
    if (ctx.pipeline->supportsKeysValues()) {
        ctx.pipeline->setOutputMode(OutputMode::e_keys);
    }
}

static void h_values(SpoiContext& ctx, SpoiInstruction const&) {
    if (!ctx.pipeline) return;
    if (ctx.pipeline->supportsKeysValues()) {
        ctx.pipeline->setOutputMode(OutputMode::e_values);
    }
}

static void h_join(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    if (inst.operand.size() < sizeof(u32)) return;
    u32 memberIdx;
    std::memcpy(&memberIdx, inst.operand.data(), sizeof(u32));
    auto newPipeline = ctx.pipeline->joinFlatten(memberIdx);
    if (newPipeline) {
        ctx.pipeline = std::move(newPipeline);
    }
}

// =============================== C++23 操作处理函数 ===============================

static void h_enumerate(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    u32 start = 0;
    if (inst.operand.size() >= sizeof(u32)) {
        std::memcpy(&start, inst.operand.data(), sizeof(u32));
    }
    ctx.pipeline->postEnumerate(start);
}

static void h_chunk(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    if (inst.operand.size() < sizeof(u32)) return;
    u32 size;
    std::memcpy(&size, inst.operand.data(), sizeof(u32));
    ctx.pipeline->postChunk(size);
}

static void h_slide(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    if (inst.operand.size() < sizeof(u32)) return;
    u32 size;
    std::memcpy(&size, inst.operand.data(), sizeof(u32));
    ctx.pipeline->postSlide(size);
}

static void h_stride(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    if (inst.operand.size() < sizeof(u32)) return;
    u32 step;
    std::memcpy(&step, inst.operand.data(), sizeof(u32));
    ctx.pipeline->postStride(step);
}

static void h_adjacent(SpoiContext& ctx, SpoiInstruction const& inst) {
    if (!ctx.pipeline) return;
    u32 n = 2; // 默认 adjacent<2>
    if (inst.operand.size() >= sizeof(u32)) {
        std::memcpy(&n, inst.operand.data(), sizeof(u32));
    }
    ctx.pipeline->postAdjacent(n);
}

// =============================== 控制操作处理函数 ===============================

static void h_exec(SpoiContext& ctx, SpoiInstruction const&) {
    if (!ctx.pipeline || !ctx.result) return;
    ctx.pipeline->serialize(*ctx.result);
}

static void h_pipe(SpoiContext&, SpoiInstruction const&) {
    // pipe 链式连接，在客户端处理，服务端无需操作
}

// =============================== 导航操作处理函数 ===============================

static void h_nav(SpoiContext&, SpoiInstruction const&) {
    // 导航已在主循环中处理，handler 为空
}

// =============================== 写操作处理函数（占位，实际由主循环处理） ===============================

static void h_set(SpoiContext&, SpoiInstruction const&) {}
static void h_add(SpoiContext&, SpoiInstruction const&) {}
static void h_append(SpoiContext&, SpoiInstruction const&) {}
static void h_remove(SpoiContext&, SpoiInstruction const&) {}
static void h_insert(SpoiContext&, SpoiInstruction const&) {}
static void h_replace(SpoiContext&, SpoiInstruction const&) {}
static void h_reset(SpoiContext&, SpoiInstruction const&) {}
static void h_setnull(SpoiContext&, SpoiInstruction const&) {}

static void h_unimpl(SpoiContext& ctx, SpoiInstruction const& inst) {
    ctx.hasError = true;
    ctx.errorMsg = std::string("SPOI: op not implemented: ") + kSpoiOpNames[inst.op];
}

// =============================== 函数指针表 ===============================

// X 宏：操作码 → handler 函数映射
// 新增 SPOI 操作时只需在此添加一行
#define Xt_SPOI_handler(X__) \
    X__(h_nav)          /* 0x00 e_nav */ \
    X__(h_unimpl)       /* 0x01 e_idx */ \
    X__(h_unimpl)       /* 0x02 e_deref */ \
    X__(h_unimpl)       /* 0x03 e_unwrap */ \
    X__(h_set)          /* 0x04 e_set */ \
    X__(h_add)          /* 0x05 e_add */ \
    X__(h_append)       /* 0x06 e_append */ \
    X__(h_remove)       /* 0x07 e_remove */ \
    X__(h_insert)       /* 0x08 e_insert */ \
    X__(h_replace)      /* 0x09 e_replace */ \
    X__(h_reset)        /* 0x0A e_reset */ \
    X__(h_setnull)      /* 0x0B e_setnull */ \
    X__(h_filter)       /* 0x0C e_filter */ \
    X__(h_select)       /* 0x0D e_select */ \
    X__(h_sort)         /* 0x0E e_sort */ \
    X__(h_reverse)      /* 0x0F e_reverse */ \
    X__(h_take)         /* 0x10 e_take */ \
    X__(h_drop)         /* 0x11 e_drop */ \
    X__(h_takewhile)    /* 0x12 e_takewhile */ \
    X__(h_dropwhile)    /* 0x13 e_dropwhile */ \
    X__(h_distinct)     /* 0x14 e_distinct */ \
    X__(h_count)        /* 0x15 e_count */ \
    X__(h_any)          /* 0x16 e_any */ \
    X__(h_all)          /* 0x17 e_all */ \
    X__(h_find)         /* 0x18 e_find */ \
    X__(h_keys)         /* 0x19 e_keys */ \
    X__(h_values)       /* 0x1A e_values */ \
    X__(h_join)         /* 0x1B e_join */ \
    X__(h_enumerate)    /* 0x1C e_enumerate */ \
    X__(h_chunk)        /* 0x1D e_chunk */ \
    X__(h_slide)        /* 0x1E e_slide */ \
    X__(h_stride)       /* 0x1F e_stride */ \
    X__(h_adjacent)     /* 0x20 e_adjacent */ \
    X__(h_exec)         /* 0x21 e_exec */ \
    X__(h_pipe)         /* 0x22 e_pipe */

#define X_h_entry(h) &h,
constexpr SpoiHandler kSpoiHandlers[kSpoiOpCount] = { Xt_SPOI_handler(X_h_entry) };
#undef X_h_entry

// =============================== SpoiExecutor ===============================

class SpoiExecutor {
    std::istream& _is;

public:
    explicit SpoiExecutor(std::istream& is) : _is(is) {}

    // operator>> 将 SPOI 指令流应用到根对象（无结果输出，用于纯写操作）
    template<typename RootT>
    SpoiExecutor& operator>>(RootT& root) {
        _executeImpl(root, nullptr);
        return *this;
    }

    // execute 将 SPOI 指令流应用到根对象，并将查询结果写入 result
    template<typename RootT>
    void execute(RootT& root, std::ostream& result) {
        _executeImpl(root, &result);
    }

private:
    template<typename RootT>
    void _executeImpl(RootT& root, std::ostream* result) {
        if (_is.peek() == std::istream::traits_type::eof()) return;

        SpoiStream stream;
        I i(_is);
        i >> stream;

        SpoiContext ctx;
        ctx.rootObj = static_cast<void*>(&root);
        ctx.result = result;

        for (auto& inst : stream.instructions) {
            SpoiOp op = static_cast<SpoiOp>(inst.op);
            u32 opIdx = static_cast<u32>(op);

            if (opIdx >= kSpoiOpCount) {
                std::cerr << "[SpoiExecutor] invalid opcode: " << static_cast<int>(inst.op) << "\n";
                continue;
            }

            // ── 写操作：SET / ADD ──
            if ((op == SpoiOp::e_set || op == SpoiOp::e_add) && !inst.path.empty()) {
                try {
                    auto& operand = inst.operand;
                    _navigateWith(root, inst.path, [&](auto& target) {
                        _applyWriteOpTyped(target, op, operand);
                    });
                } catch (std::exception const& e) {
                    std::cerr << "[SpoiExecutor] write error: " << e.what() << "\n";
                }
            }
            // ── 写操作：APPEND / REMOVE / INSERT / REPLACE（容器级操作）──
            else if ((op == SpoiOp::e_append || op == SpoiOp::e_remove
                   || op == SpoiOp::e_insert || op == SpoiOp::e_replace) && !inst.path.empty()) {
                try {
                    _applyContainerWriteOp(root, inst, op);
                } catch (std::exception const& e) {
                    std::cerr << "[SpoiExecutor] container write error: " << e.what() << "\n";
                }
            }
            // ── 写操作：RESET（清空 optional）──
            else if (op == SpoiOp::e_reset && !inst.path.empty()) {
                try {
                    _navigateWith(root, inst.path, [&](auto& target) {
                        _applyResetOp(target);
                    });
                } catch (std::exception const& e) {
                    std::cerr << "[SpoiExecutor] reset error: " << e.what() << "\n";
                }
            }
            // ── 写操作：SETNULL（指针置空）──
            else if (op == SpoiOp::e_setnull && !inst.path.empty()) {
                try {
                    _navigateWith(root, inst.path, [&](auto& target) {
                        _applySetNullOp(target);
                    });
                } catch (std::exception const& e) {
                    std::cerr << "[SpoiExecutor] setnull error: " << e.what() << "\n";
                }
            }
            // ── 读操作：首次遇到读操作时，根据路径导航到容器并初始化管道 ──
            else if (isReadOp(op) || isAggregateOp(op) || isContainerOp(op) || isCpp23Op(op) || op == SpoiOp::e_exec || op == SpoiOp::e_pipe) {
                // 首次遇到需要管道的操作时，用路径导航到容器并初始化管道
                if (!ctx.hasPipeline && !inst.path.empty()) {
                    try {
                        _initReadPipeline(root, inst.path, ctx);
                    } catch (std::exception const& e) {
                        std::cerr << "[SpoiExecutor] pipeline init error: " << e.what() << "\n";
                    }
                }
            }

            // ── 调用处理函数 ──
            kSpoiHandlers[opIdx](ctx, inst);

            if (ctx.hasError) {
                std::cerr << "[SpoiExecutor] Error: " << ctx.errorMsg << "\n";
                break;
            }
        }
    }

private:
    // ── 在导航终点，用具体类型反序列化 operand 并应用 SET/ADD 操作 ──
    template<typename T>
    static void _applyWriteOpTyped(T& target, SpoiOp op, std::vector<u8> const& operand) {
        if constexpr (is_optional_v<T>) {
            // 目标为 optional：operand 是内部值（不含 has_value 标志），自动 emplace 后写入
            if (op == SpoiOp::e_set) {
                if (!target.has_value()) target.emplace();
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                i >> *target;
            } else {
                std::cerr << "[SpoiExecutor] op not supported for optional target\n";
            }
        } else if (op == SpoiOp::e_set) {
            std::string valStr(operand.begin(), operand.end());
            std::stringstream ss(valStr);
            I i(ss);
            i >> target;
        } else if (op == SpoiOp::e_add) {
            if constexpr (requires(T& t, T d) { t += d; }) {
                T delta{};
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                i >> delta;
                target += delta;
            } else {
                std::cerr << "[SpoiExecutor] ADD not supported for this type\n";
            }
        }
    }

    // ── RESET：清空 optional 或指针 ──
    template<typename T>
    static void _applyResetOp(T& target) {
        if constexpr (is_optional_v<T>) {
            target.reset();
        } else if constexpr (is_sptr_v<T>) {
            target.reset();
        } else if constexpr (is_uptr_v<T>) {
            target.reset();
        } else {
            std::cerr << "[SpoiExecutor] RESET only supported for optional and pointer types\n";
        }
    }

    // ── SETNULL：指针置空 ──
    template<typename T>
    static void _applySetNullOp(T& target) {
        if constexpr (is_sptr_v<T>) {
            target.reset();
        } else if constexpr (is_uptr_v<T>) {
            target.reset();
        } else {
            std::cerr << "[SpoiExecutor] SETNULL only supported for pointer types\n";
        }
    }

    // ── 容器级写操作：APPEND / REMOVE / INSERT / REPLACE ──
    template<typename RootT>
    static void _applyContainerWriteOp(RootT& root, SpoiInstruction const& inst, SpoiOp op) {
        // 对于 APPEND，path 就是容器路径
        // 对于 REMOVE/INSERT/REPLACE，path 最后一个段是元素索引，前面是容器路径
        std::vector<u32> containerPath = inst.path;
        u32 elemIdx = 0;
        if (op != SpoiOp::e_append && !containerPath.empty()) {
            elemIdx = containerPath.back();
            containerPath.pop_back();
        }

        if (containerPath.empty()) {
            std::cerr << "[SpoiExecutor] empty container path for container write op\n";
            return;
        }

        _navigateWith(root, containerPath, [&](auto& container) {
            _applyContainerOp(container, op, elemIdx, inst.operand);
        });
    }

    template<typename ContainerT>
    static void _applyContainerOp(ContainerT& container, SpoiOp op, u32 elemIdx, std::vector<u8> const& operand) {
        if constexpr (is_ordered_container_v<ContainerT>) {
            using ElemT = typename ContainerT::value_type;
            size_t idx = static_cast<size_t>(elemIdx);

            if (op == SpoiOp::e_append) {
                ElemT elem{};
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                i >> elem;
                container.push_back(std::move(elem));
            } else if (op == SpoiOp::e_remove) {
                if (idx >= container.size()) {
                    std::cerr << "[SpoiExecutor] REMOVE index out of range\n";
                    return;
                }
                auto it = container.begin();
                std::advance(it, idx);
                container.erase(it);
            } else if (op == SpoiOp::e_insert) {
                ElemT elem{};
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                i >> elem;
                if (idx > container.size()) idx = container.size();
                auto it = container.begin();
                std::advance(it, idx);
                container.insert(it, std::move(elem));
            } else if (op == SpoiOp::e_replace) {
                if (idx >= container.size()) {
                    std::cerr << "[SpoiExecutor] REPLACE index out of range\n";
                    return;
                }
                ElemT elem{};
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                i >> elem;
                auto it = container.begin();
                std::advance(it, idx);
                *it = std::move(elem);
            }
        } else if constexpr (is_flist_v<ContainerT>) {
            using ElemT = typename ContainerT::value_type;
            size_t idx = static_cast<size_t>(elemIdx);
            size_t dist = static_cast<size_t>(std::distance(container.begin(), container.end()));

            if (op == SpoiOp::e_append) {
                ElemT elem{};
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                i >> elem;
                // forward_list: 追加到末尾
                if (container.empty()) {
                    container.push_front(std::move(elem));
                } else {
                    auto it = container.before_begin();
                    std::advance(it, dist);
                    container.insert_after(it, std::move(elem));
                }
            } else if (op == SpoiOp::e_remove) {
                if (idx >= dist) {
                    std::cerr << "[SpoiExecutor] REMOVE index out of range for forward_list\n";
                    return;
                }
                auto it = container.before_begin();
                std::advance(it, idx);
                container.erase_after(it);
            } else if (op == SpoiOp::e_insert) {
                ElemT elem{};
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                i >> elem;
                if (idx > dist) idx = dist;
                auto it = container.before_begin();
                std::advance(it, idx);
                container.insert_after(it, std::move(elem));
            } else if (op == SpoiOp::e_replace) {
                if (idx >= dist) {
                    std::cerr << "[SpoiExecutor] REPLACE index out of range for forward_list\n";
                    return;
                }
                ElemT elem{};
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                i >> elem;
                auto it = container.begin();
                std::advance(it, idx);
                *it = std::move(elem);
            }
        } else if constexpr (is_set_container_v<ContainerT>) {
            using ElemT = typename ContainerT::value_type;
            size_t idx = static_cast<size_t>(elemIdx);

            if (op == SpoiOp::e_append) {
                ElemT elem{};
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                i >> elem;
                container.insert(std::move(elem));
            } else if (op == SpoiOp::e_remove) {
                if (idx >= container.size()) {
                    std::cerr << "[SpoiExecutor] REMOVE index out of range for set\n";
                    return;
                }
                auto it = container.begin();
                std::advance(it, idx);
                container.erase(it);
            } else {
                std::cerr << "[SpoiExecutor] container write op not supported for this container type\n";
            }
        } else if constexpr (is_assoc_container_v<ContainerT>) {
            using KeyT = typename ContainerT::key_type;
            using ValT = typename ContainerT::mapped_type;
            size_t idx = static_cast<size_t>(elemIdx);

            if (op == SpoiOp::e_append) {
                // operand 包含序列化的 key + value
                std::string valStr(operand.begin(), operand.end());
                std::stringstream ss(valStr);
                I i(ss);
                KeyT key{};
                ValT val{};
                i >> key;
                i >> val;
                container.insert_or_assign(std::move(key), std::move(val));
            } else if (op == SpoiOp::e_remove) {
                if (idx >= container.size()) {
                    std::cerr << "[SpoiExecutor] REMOVE index out of range for map\n";
                    return;
                }
                auto it = container.begin();
                std::advance(it, idx);
                container.erase(it);
            } else {
                std::cerr << "[SpoiExecutor] container write op not supported for this container type\n";
            }
        } else {
            std::cerr << "[SpoiExecutor] container write op not supported for this container type\n";
        }
    }

    // ── 初始化读管道：从 root 导航到容器并创建 ReadPipeline ──
    template<typename RootT>
    static void _initReadPipeline(RootT& root, std::vector<u32> const& path, SpoiContext& ctx) {
        _navigateWith(root, path, [&](auto& container) {
            using ContainerT = std::decay_t<decltype(container)>;
            if constexpr (is_ordered_container_v<ContainerT>) {
                using ElemT = typename ContainerT::value_type;
                auto pipeline = std::make_unique<ReadPipeline<ElemT>>();
                pipeline->populate(container);
                ctx.pipeline = std::move(pipeline);
                ctx.hasPipeline = true;
            } else if constexpr (is_flist_v<ContainerT>) {
                using ElemT = typename ContainerT::value_type;
                auto pipeline = std::make_unique<ReadPipeline<ElemT>>();
                pipeline->populate(container);
                ctx.pipeline = std::move(pipeline);
                ctx.hasPipeline = true;
            } else if constexpr (is_assoc_container_v<ContainerT>) {
                // map 容器：管道元素是 map 的 value 类型，同时存储序列化后的 keys
                using ElemT = typename ContainerT::mapped_type;
                auto pipeline = std::make_unique<ReadPipeline<ElemT>>();
                pipeline->_isMap = true;
                pipeline->populateMap(container);
                ctx.pipeline = std::move(pipeline);
                ctx.hasPipeline = true;
            } else if constexpr (is_set_container_v<ContainerT>) {
                // set 容器：元素为 const，需 const_cast
                using ElemT = typename ContainerT::value_type;
                auto pipeline = std::make_unique<ReadPipeline<ElemT>>();
                pipeline->_isSet = true;
                pipeline->populateSet(container);
                ctx.pipeline = std::move(pipeline);
                ctx.hasPipeline = true;
            } else {
                std::cerr << "[SpoiExecutor] pipeline init: unsupported container type\n";
            }
        });
    }
};

} // namespace sp