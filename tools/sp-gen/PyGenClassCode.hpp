#pragma once

#include "PyMemberInfoImpl.hpp"
#include "GenClassCodeBase.hpp"
#include <sstream>
#include <string>
#include <ranges>

struct PyGenTraits {
    static void resetVarCounter() {}

    template<typename T>
    static MemberInfo getMemberInfo() { return getPyMemberInfo<T>(); }

    static void generate(std::stringstream& ss,
                         const std::string& className,
                         const std::string& baseName,
                         const auto& membersName,
                         const auto& typeStrs) {
        auto sz = std::size(membersName);
        ss << "class " << className << "(" << baseName << "):\n";
        ss << "    typeID = E_StreamPunkType." << className << "\n";
        ss << "\n";
        ss << "    def __init__(self):\n";
        ss << "        super().__init__()\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "        self." << membersName[i] << " = " << info.ivalue << "\n";
        }
        ss << "\n";
        ss << "    def from_(self, i: I):\n";
        ss << "        super().from_(i)\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "        self." << membersName[i] << " = " << info.deserializeCode << "\n";
        }
        ss << "        return self\n";
        ss << "\n";
        ss << "    def to(self, o: O):\n";
        ss << "        super().to(o)\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            auto code = info.serializeCode;
            replaceAll(code, VAL, std::string("self.") + membersName[i]);
            ss << "        " << code << "\n";
        }
        ss << "        return self\n";
        ss << "\n";
    }
};

template<typename CT>
std::string genPyClassCode() {
    return genClassCodeImpl<CT, PyGenTraits>();
}