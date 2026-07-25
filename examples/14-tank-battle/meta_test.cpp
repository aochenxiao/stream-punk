#include "Data_test.hpp"
#include <stream-punk/MetaData.hpp>
#include <fstream>

int main() {
    INIT_StreamPunk();
    sp_meta::MetaFile meta;
    #define X_extract(type, name) meta.types.push_back(sp_meta::extractTypeMeta<name>());
    Xt_CustomType(X_extract);
    #undef X_extract
    std::ofstream ofs("temp/stream-punk-meta.bin", std::ios::binary);
    sp_meta::writeMetaFile(ofs, meta);
    return 0;
}