#pragma once

#include "MemberInfoBase.hpp"
#include <sstream>
#include <string>
#include <vector>
#include <utility>

template<typename CT, typename Traits>
std::string genClassCodeImpl() {
    Traits::resetVarCounter();
    using TupleType = typename CT::M::TypeList;
    constexpr size_t tupleSize = std::tuple_size<TupleType>::value;
    std::stringstream ss;
    auto getTypeStrs = []<size_t... Is>(std::index_sequence<Is...>) {
        return std::vector<MemberInfo>{Traits::template getMemberInfo<std::tuple_element_t<Is, TupleType>>()...};
    };
    auto typeStrs = getTypeStrs(std::make_index_sequence<tupleSize>{});
    Traits::generate(ss, CT::_className, CT::_baseName, CT::_membersName, typeStrs);
    return ss.str();
}