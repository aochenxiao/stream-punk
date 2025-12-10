#include "unit_test.hpp"
# include "Data.hpp"

#include "unit_test.hpp"

// 测试辅助函数
inline bool operator==(const StreamPunkMachineInfo& lhs, const StreamPunkMachineInfo& rhs) {
    return memcmp(&lhs, &rhs, sizeof(StreamPunkMachineInfo)) == 0;
}

REG_TEST(test_machine_info_init) {
    StreamPunkMachineInfo info;
    info.init();

    // 验证字节序
#if __cplusplus >= 202002L
    if constexpr (std::endian::native == std::endian::little) {
        ck_assert_uint_eq(info.endian, 0);
    }
    else if constexpr (std::endian::native == std::endian::big) {
        ck_assert_uint_eq(info.endian, 1);
    }
#endif

    // 验证字长
    ck_assert_uint_eq(info.wordByteCount, sizeof(void*));

    // 验证数据版本
    ck_assert_uint_eq(info.dataVer, currDataVer);

    // 验证操作系统检测
#if BOOST_OS_WINDOWS
    ck_assert_uint_eq(info.os, StreamPunkMachineInfo::e_os_win);
#elif BOOST_OS_MACOS
    ck_assert_uint_eq(info.os, StreamPunkMachineInfo::e_os_mac);
#elif BOOST_OS_LINUX
    ck_assert_uint_eq(info.os, StreamPunkMachineInfo::e_os_linux);
#elif BOOST_OS_IOS
    ck_assert_uint_eq(info.os, StreamPunkMachineInfo::e_os_ios);
#elif BOOST_OS_ANDROID
    ck_assert_uint_eq(info.os, StreamPunkMachineInfo::e_os_android);
#elif BOOST_OS_BSD
    ck_assert_uint_eq(info.os, StreamPunkMachineInfo::e_os_bsd);
#else
    ck_assert_uint_eq(info.os, StreamPunkMachineInfo::e_os_unknow);
#endif

    // 验证CPU检测
#if BOOST_ARCH_X86_64
    ck_assert_uint_eq(info.cpu, StreamPunkMachineInfo::e_cpu_X86_64);
#elif BOOST_ARCH_X86
    ck_assert_uint_eq(info.cpu, StreamPunkMachineInfo::e_cpu_x86);
#elif BOOST_ARCH_ARM64
    ck_assert_uint_eq(info.cpu, StreamPunkMachineInfo::e_cpu_arm64);
#elif BOOST_ARCH_ARM
    ck_assert_uint_eq(info.cpu, StreamPunkMachineInfo::e_cpu_arm);
#elif BOOST_ARCH_PPC
    ck_assert_uint_eq(info.cpu, StreamPunkMachineInfo::e_cpu_ppc);
#else
    ck_assert_uint_eq(info.cpu, StreamPunkMachineInfo::e_cpu_unknow);
#endif
}

REG_TEST(test_machine_info_serialization_v0_1_1) {
    StreamPunkMachineInfo src;
    src.init();

    std::stringstream ss;
    {
        O out{ ss };
        out << src;
    }

    StreamPunkMachineInfo dest;
    {
        I in{ ss };
        in >> dest;
    }

    // 验证完整结构
    ck_assert(src == dest);
}

REG_TEST(test_machine_info_invalid_deserialize) {
    // 短数据测试 (比结构体少一个字节)
    std::string shortData(sizeof(StreamPunkMachineInfo) - 1, '\0');
    std::stringstream shortStream(shortData);
    I in_short{ shortStream };
    StreamPunkMachineInfo dest;

    // 是否会抛异常
    bool threwException = false;
    try {
        in_short >> dest;
    }
    catch (const std::ios_base::failure&) {
        threwException = true;
    }
    catch (...) {}

    ck_assert(threwException);
}

REG_TEST(test_machine_info_bitfield_boundaries) {
    // 测试位域最大值情况
    StreamPunkMachineInfo info;
    info.endian = 1;  // 最大位
    info.os = StreamPunkMachineInfo::e_os_bsd;  // OS最大值
    info.cpu = StreamPunkMachineInfo::e_cpu_ppc;  // CPU最大值
    info.compiler = StreamPunkMachineInfo::e_compiler_intel;  // 编译器最大值

    std::stringstream ss;
    {
        O out{ ss };
        out << info;
    }

    StreamPunkMachineInfo result;
    {
        I in{ ss };
        in >> result;
    }

    ck_assert_uint_eq(result.endian, 1);
    ck_assert_uint_eq(result.os, StreamPunkMachineInfo::e_os_bsd);
    ck_assert_uint_eq(result.cpu, StreamPunkMachineInfo::e_cpu_ppc);
    ck_assert_uint_eq(result.compiler, StreamPunkMachineInfo::e_compiler_intel);
}

REG_TEST(test_machine_info_compiler_version) {
    StreamPunkMachineInfo info;
    info.init();

    // 如果有编译器版本，检查格式是否正确
    if (info.compiler != StreamPunkMachineInfo::e_compiler_unknow) {
        uint32_t major = (info.compilerVer >> 24) & 0xFF;
        uint32_t minor = (info.compilerVer >> 16) & 0xFF;
        uint32_t patch = (info.compilerVer >> 8) & 0xFF;

#if BOOST_COMP_MSVC
        ck_assert_uint_ge(major, BOOST_VERSION_NUMBER_MAJOR(BOOST_COMP_MSVC));
#elif BOOST_COMP_CLANG
        ck_assert_uint_ge(major, BOOST_VERSION_NUMBER_MAJOR(BOOST_COMP_CLANG));
#elif BOOST_COMP_GNUC
        ck_assert_uint_ge(major, BOOST_VERSION_NUMBER_MAJOR(BOOST_COMP_GNUC));
#elif BOOST_COMP_INTEL
        ck_assert_uint_ge(major, BOOST_VERSION_NUMBER_MAJOR(BOOST_COMP_INTEL));
#endif
    }
}

REG_TEST(test_machine_info_zero_initialized) {
    // 确保初始化前为零
    StreamPunkMachineInfo info;
    memset(&info, 0xCD, sizeof(info));  // 用非零值填充

    // 检查是否全被初始化为0
    bool wasNonZero = false;
    for (size_t i = 0; i < sizeof(info); i++) {
        if (reinterpret_cast<uint8_t*>(&info)[i] != 0) {
            wasNonZero = true;
            break;
        }
    }
    ck_assert(wasNonZero);

    info.init();  // 执行初始化

    // 检查初始化后是否全零
    bool hasNonZero = false;
    for (size_t i = 0; i < sizeof(info); i++) {
        if (reinterpret_cast<uint8_t*>(&info)[i] != 0) {
            hasNonZero = true;
            break;
        }
    }

    // 只有endian和os位域可能有非零值
    if (info.endian == 0 && info.os == 0) {
        ck_assert(!hasNonZero);
    }
}