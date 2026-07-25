#pragma once

#include "KotlinMemberInfoImpl.hpp"
#include "GenClassCodeBase.hpp"
#include <sstream>
#include <string>

struct KotlinGenTraits {
    static void resetVarCounter() { resetKotlinVarCounter(); }

    template<typename T>
    static MemberInfo getMemberInfo() { return getKotlinMemberInfo<T>(); }

    static void generate(std::stringstream& ss,
                         const std::string& className,
                         const std::string& baseName,
                         const auto& membersName,
                         const auto& typeStrs) {
        auto sz = std::size(membersName);
        // Kotlin 类默认是 final 的；为所有生成的类使用 "open"
        // 以便它们可以被子类继承
        bool isBase = (className == "Base");
        ss << "open class " << className;
        if (!isBase) {
            ss << " : " << baseName << "()";
        }
        ss << " {\n";
        ss << "    override val typeID: Int get() = E_StreamPunkType." << className << "\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "    var " << membersName[i] << ": " << info.tname << " = " << info.ivalue << "\n";
        }
        ss << "\n";
        ss << "    override fun from_(i: I): " << className << " {\n";
        if (baseName != "Base" || !isBase) {
            ss << "        super.from_(i)\n";
        }
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            if (info.arraySize > 0) {
                ss << "        for (_i in 0 until " << info.arraySize << ") this." << membersName[i] << "[_i] = " << info.deserializeCode << "\n";
            } else {
                ss << "        this." << membersName[i] << " = " << info.deserializeCode << "\n";
            }
        }
        ss << "        return this\n";
        ss << "    }\n\n";
        ss << "    override fun to(o: O) {\n";
        if (baseName != "Base" || !isBase) {
            ss << "        super.to(o)\n";
        }
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            if (!info.serializeCode.empty()) {
                auto code = info.serializeCode;
                replaceAll(code, VAL, std::string("this.") + membersName[i]);
                ss << "        " << code << "\n";
            }
        }
        ss << "    }\n";
        ss << "}\n\n";
    }
};

template<typename CT>
std::string genKotlinClassCode() {
    return genClassCodeImpl<CT, KotlinGenTraits>();
}