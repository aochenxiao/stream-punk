#pragma once

#include "JavaMemberInfoImpl.hpp"
#include "GenClassCodeBase.hpp"
#include <sstream>
#include <string>

struct JavaGenTraits {
    static void resetVarCounter() { resetJavaVarCounter(); }

    template<typename T>
    static MemberInfo getMemberInfo() { return getJavaMemberInfo<T>(); }

    static void generate(std::stringstream& ss,
                         const std::string& className,
                         const std::string& baseName,
                         const auto& membersName,
                         const auto& typeStrs) {
        auto sz = std::size(membersName);
        ss << "class " << className << " extends " << baseName << " {\n";
        ss << "    public static final int typeID = E_StreamPunkType." << className << ";\n";
        ss << "\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "    public " << info.tname << " " << membersName[i] << " = " << info.ivalue << ";\n";
        }
        ss << "\n";
        ss << "    public " << className << "() {\n";
        ss << "        super();\n";
        ss << "    }\n";
        ss << "\n";
        ss << "    @Override\n";
        ss << "    public " << className << " from_(I i) {\n";
        ss << "        super.from_(i);\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            if (info.arraySize > 0) {
                ss << "        for (int _i = 0; _i < " << info.arraySize << "; _i++) this." << membersName[i] << "[_i] = " << info.deserializeCode << ";\n";
            } else {
                ss << "        this." << membersName[i] << " = " << info.deserializeCode << ";\n";
            }
        }
        ss << "        return this;\n";
        ss << "    }\n";
        ss << "\n";
        ss << "    @Override\n";
        ss << "    public void to(O o) {\n";
        ss << "        super.to(o);\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            auto code = info.serializeCode;
            replaceAll(code, VAL, std::string("this.") + membersName[i]);
            ss << "        " << code << ";\n";
        }
        ss << "    }\n";
        ss << "}\n";
        ss << "\n";
    }
};

template<typename CT>
std::string genJavaClassCode() {
    return genClassCodeImpl<CT, JavaGenTraits>();
}