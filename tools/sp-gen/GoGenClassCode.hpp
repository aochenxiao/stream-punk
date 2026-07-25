#pragma once

#include "GoMemberInfoImpl.hpp"
#include "GenClassCodeBase.hpp"
#include <sstream>
#include <string>

struct GoGenTraits {
    static void resetVarCounter() { resetGoVarCounter(); }

    template<typename T>
    static MemberInfo getMemberInfo() { return getGoMemberInfo<T>(); }

    static void generate(std::stringstream& ss,
                         const std::string& className,
                         const std::string& baseName,
                         const auto& membersName,
                         const auto& typeStrs) {
        auto sz = std::size(membersName);
        ss << "type " << className << " struct {\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            ss << "    " << membersName[i] << " " << info.tname << "\n";
        }
        ss << "}\n\n";

        ss << "func (x *" << className << ") TypeID() uint32 {\n";
        ss << "    return E_StreamPunkType_" << className << "\n";
        ss << "}\n\n";

        ss << "func (x *" << className << ") From_(i *I) {\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            auto code = info.deserializeCode;
            replaceAll(code, VAL, std::string("x.") + membersName[i]);
            ss << "    x." << membersName[i] << " = " << code << "\n";
        }
        ss << "}\n\n";

        ss << "func (x *" << className << ") To(o *O) {\n";
        for (size_t i = 0; i < sz; ++i) {
            auto& info = typeStrs[i];
            if (!info.serializeCode.empty()) {
                auto code = info.serializeCode;
                replaceAll(code, VAL, std::string("x.") + membersName[i]);
                ss << "    " << code << "\n";
            }
        }
        ss << "}\n\n";
    }
};

template<typename CT>
std::string genGoClassCode() {
    return genClassCodeImpl<CT, GoGenTraits>();
}