#pragma once

#include "RustMemberInfoImpl.hpp"
#include "GenClassCodeBase.hpp"
#include <sstream>
#include <string>

struct RustGenTraits {
    static void resetVarCounter() { resetRustVarCounter(); }

    template<typename T>
    static MemberInfo getMemberInfo() { return getRustMemberInfo<T>(); }

    static void generate(std::stringstream& ss,
                         const std::string& className,
                         const std::string& baseName,
                         const auto& membersName,
                         const auto& typeStrs) {
        auto sz = std::size(membersName);
        ss << "#[derive(Debug, Clone)]\n";
        ss << "pub struct " << className << " {\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "    pub " << membersName[i] << ": " << info.tname << ",\n";
        }
        ss << "}\n\n";

        ss << "impl Default for " << className << " {\n";
        ss << "    fn default() -> Self {\n";
        ss << "        Self {\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "            " << membersName[i] << ": " << info.ivalue << ",\n";
        }
        ss << "        }\n";
        ss << "    }\n";
        ss << "}\n\n";

        ss << "impl SpBase for " << className << " {\n";
        ss << "    fn type_id(&self) -> u32 {\n";
        ss << "        E_StreamPunkType::" << className << "\n";
        ss << "    }\n\n";
        ss << "    fn from_(&mut self, i: &mut I) {\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            if (info.arraySize > 0) {
                ss << "        for _idx in 0.." << info.arraySize << " {\n";
                ss << "            self." << membersName[i] << "[_idx] = " << info.deserializeCode << ";\n";
                ss << "        }\n";
            } else {
                ss << "        self." << membersName[i] << " = " << info.deserializeCode << ";\n";
            }
        }
        ss << "    }\n\n";
        ss << "    fn to(&self, o: &mut O) {\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            if (!info.serializeCode.empty()) {
                auto code = info.serializeCode;
                replaceAll(code, VAL, std::string("self.") + membersName[i]);
                ss << "        " << code << ";\n";
            }
        }
        ss << "    }\n";
        ss << "}\n\n";

        ss << "impl " << className << " {\n";
        ss << "    pub fn from_i(i: &mut I) -> Self {\n";
        ss << "        let mut obj = Self::default();\n";
        ss << "        obj.from_(i);\n";
        ss << "        obj\n";
        ss << "    }\n";
        ss << "}\n\n";
    }
};

template<typename CT>
std::string genRustClassCode() {
    return genClassCodeImpl<CT, RustGenTraits>();
}