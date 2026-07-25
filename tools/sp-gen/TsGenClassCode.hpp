#pragma once

#include "TsMemberInfoImpl.hpp"
#include "GenClassCodeBase.hpp"
#include <sstream>
#include <string>
#include <ranges>

struct TsGenTraits {
    static void resetVarCounter() {}

    template<typename T>
    static MemberInfo getMemberInfo() { return getTsMemberInfo<T>(); }

    static void generate(std::stringstream& ss,
                         const std::string& className,
                         const std::string& baseName,
                         const auto& membersName,
                         const auto& typeStrs) {
        auto sz = std::size(membersName);
        // 类声明
        ss << "export class " << className << " extends " << baseName << " {\n";
        ss << "    static typeID = E_StreamPunkType." << className << ";\n";
        ss << "\n";
        // 成员声明
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "    " << membersName[i] << ": " << info.tname << ";\n";
        }
        ss << "\n";
        // 构造函数
        ss << "    constructor() {\n";
        ss << "        super();\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "        this." << membersName[i] << " = " << info.ivalue << ";\n";
        }
        ss << "    }\n";
        ss << "\n";
        // from_ 反序列化
        ss << "    static from_(i: I): " << className << " {\n";
        ss << "        const obj = new " << className << "();\n";
        ss << "        obj._baseFrom_(i);\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "        obj." << membersName[i] << " = " << info.deserializeCode << ";\n";
        }
        ss << "        return obj;\n";
        ss << "    }\n";
        ss << "\n";
        // to 序列化
        ss << "    to(o: O): this {\n";
        ss << "        super.to(o);\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            auto code = info.serializeCode;
            replaceAll(code, VAL, std::string("this.") + membersName[i]);
            ss << "        " << code << ";\n";
        }
        ss << "    }\n";
        ss << "}\n";
    }
};

template<typename CT>
std::string genTsClassCode() {
    return genClassCodeImpl<CT, TsGenTraits>();
}

template<typename CT>
std::string genClassCode() {
    return genTsClassCode<CT>();
}