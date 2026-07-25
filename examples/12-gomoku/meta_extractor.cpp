#include "Data.hpp"
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

using namespace sp;

// Base::fromJsonStream 的 stub 定义
void sp::Base::fromJsonStream(sp::JsonStreamReader&) {}

// 写入辅助函数
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

int main() {
    INIT_StreamPunk();

    constexpr uint32_t META_MAGIC = 0x53504D44;
    constexpr uint32_t META_VERSION = 1;

    std::filesystem::create_directories("temp");
    std::ofstream ofs("temp/stream-punk-meta.bin", std::ios::binary);

    writeU32(ofs, META_MAGIC);
    writeU32(ofs, META_VERSION);
    writeU32(ofs, 3); // 3 types

    // ---- Stone: row(i32), col(i32), player(i32) ----
    {
        Stone obj;
        auto names = obj.getMemberNames();
        std::vector<std::vector<Sz>> descs = {
            {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
            {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
            {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        };
        writeTypeMeta(ofs, static_cast<uint32_t>(obj.typeID()),
            obj.getClassName(), obj.getBaseName(),
            {std::string(names[0]), std::string(names[1]), std::string(names[2])}, descs);
    }

    // ---- Move: row(i32), col(i32) ----
    {
        Move obj;
        auto names = obj.getMemberNames();
        std::vector<std::vector<Sz>> descs = {
            {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
            {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        };
        writeTypeMeta(ofs, static_cast<uint32_t>(obj.typeID()),
            obj.getClassName(), obj.getBaseName(),
            {std::string(names[0]), std::string(names[1])}, descs);
    }

    // ---- GameState: stones(vector<Stone>), currentPlayer(i32), winner(i32), player1Id(i32), player2Id(i32) ----
    {
        GameState obj;
        auto names = obj.getMemberNames();
        std::vector<std::vector<Sz>> descs = {
            {TypeDesc<std::vector<Stone>>::v.begin(), TypeDesc<std::vector<Stone>>::v.end()},
            {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
            {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
            {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
            {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        };
        writeTypeMeta(ofs, static_cast<uint32_t>(obj.typeID()),
            obj.getClassName(), obj.getBaseName(),
            {std::string(names[0]), std::string(names[1]), std::string(names[2]),
             std::string(names[3]), std::string(names[4])}, descs);
    }

    return 0;
}