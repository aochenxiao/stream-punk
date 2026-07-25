#pragma once

#include "../../../include/stream-punk/StreamPunk.hpp"
#include "../../00-demo-types/Data.hpp"
#include "../../00-demo-types/customData.hpp"
using namespace sp;
#include "../../../tools/sp-gen/TsMemberInfo.hpp"
#include <iostream>
#include <sstream>

// 不建议往里面塞处在栈中的对象,在经过释放-再分配,容易产生不同对象但地址重复的问题.
struct SpObjProtocolOutput {
    O o;
    SpObjProtocolOutput(std::ostream& o_) : o(o_) {}
    template<typename T> SpObjProtocolOutput& operator<<(T const& v) {
        if constexpr (std::is_pointer_v<T> || is_specialization_of_any_v<T, std::shared_ptr, std::unique_ptr>) {
            o << v;
        }
        else if constexpr (is_specialization_of_any_v<T, std::weak_ptr>) {
            auto var = v.lock();
            operator<<(var);
        }
        else {
            o << &v;
        }
        return *this;
    }
};

// 测试数据生成函数声明
std::string test1();
std::string genBasicTypes();
std::string genHomeSystem();
std::string genSensorUpdate(int tick);
std::string genRealtimeBatch(int tick);