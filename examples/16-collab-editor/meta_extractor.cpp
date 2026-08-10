#include "Data.hpp"
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

using namespace sp;

// Base::fromJsonStream stub
void sp::Base::fromJsonStream(sp::JsonStreamReader&) {}

inline void writeU32(std::ostream& os, uint32_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
inline void writeU16(std::ostream& os, uint16_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
inline void writeStr(std::ostream& os, const std::string& s) {
    writeU16(os, static_cast<uint16_t>(s.size()));
    if (!s.empty()) os.write(s.data(), s.size());
}
inline void writeTypeMeta(std::ostream& os, std::uint32_t typeID,
    const std::string& className, const std::string& baseName,
    const std::vector<std::string>& memberNames,
    const std::vector<std::vector<Sz>>& memberDescs)
{
    writeU32(os, typeID);
    writeStr(os, className);
    writeStr(os, baseName);
    writeU16(os, static_cast<uint16_t>(memberNames.size()));
    for (size_t i = 0; i < memberNames.size(); ++i) {
        writeStr(os, memberNames[i]);
        writeU16(os, static_cast<uint16_t>(memberDescs[i].size()));
        if (!memberDescs[i].empty()) {
            os.write(reinterpret_cast<const char*>(memberDescs[i].data()),
                     memberDescs[i].size() * sizeof(Sz));
        }
    }
}

#define META_STRUCT(T, ...) \
    { \
        T obj; \
        auto names = obj.getMemberNames(); \
        auto descs = __VA_ARGS__; \
        writeTypeMeta(ofs, static_cast<uint32_t>(obj.typeID()), \
            obj.getClassName(), obj.getBaseName(), \
            std::vector<std::string>(names.begin(), names.end()), descs); \
    }

int main() {
    INIT_StreamPunk();

    constexpr uint32_t META_MAGIC = 0x53504D44;
    constexpr uint32_t META_VERSION = 1;

    std::filesystem::create_directories("temp");
    std::ofstream ofs("temp/stream-punk-meta.bin", std::ios::binary);

    writeU32(ofs, META_MAGIC);
    writeU32(ofs, META_VERSION);
    writeU32(ofs, 5); // 5 types

    // TextOp: opType(i32), position(i32), text(string), userId(i32), version(i32)
    META_STRUCT(TextOp, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
    });

    // CursorInfo: userId(i32), userName(string), position(i32), color(string)
    META_STRUCT(CursorInfo, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
    });

    // JoinRequest: userName(string)
    META_STRUCT(JoinRequest, std::vector<std::vector<Sz>>{
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
    });

    // JoinResponse: userId(i32), document(string), users(vector<CursorInfo>)
    META_STRUCT(JoinResponse, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<std::vector<CursorInfo>>::v.begin(), TypeDesc<std::vector<CursorInfo>>::v.end()},
    });

    // UserListUpdate: users(vector<CursorInfo>)
    META_STRUCT(UserListUpdate, std::vector<std::vector<Sz>>{
        {TypeDesc<std::vector<CursorInfo>>::v.begin(), TypeDesc<std::vector<CursorInfo>>::v.end()},
    });

    return 0;
}