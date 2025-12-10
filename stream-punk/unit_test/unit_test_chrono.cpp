#include "unit_test.hpp"
#include <chrono>
#include <sstream>
#include <limits>

using namespace std::chrono;
using Rep = int64_t;
constexpr auto MAX_ATTO = 1'000'000'000'000'000'000LL;  // 10^18

// ===================== StreamPunkTime核心测试 =====================
REG_TEST(test_sp_time_zero_int) {
    StreamPunkTime sp;
    sp.set(std::chrono::seconds(0));
    ck_assert_int_eq(sp.sec, 0);
    ck_assert_int_eq(sp.attoSec, 0);
}

REG_TEST(test_sp_time_positive_int_seconds) {
    using namespace std::chrono;
    StreamPunkTime sp;

    sp.set(seconds(15));
    ck_assert_int_eq(sp.sec, 15);
    ck_assert_int_eq(sp.attoSec, 0);

    auto dur = seconds(0);
    sp.get(dur);
    ck_assert_int_eq(dur.count(), 15);
}

REG_TEST(test_sp_time_fractional_ns) {
    using namespace std::chrono;
    StreamPunkTime sp;

    // 1.5纳秒 = 1.5e-9秒
    nanoseconds ns_in(1);
    ns_in += nanoseconds(1) / 2;
    sp.set(ns_in);

    // 纳秒是截断取整 所以转成atto是这个值
    constexpr i64 expected_atto = 1000000000;
    ck_assert_int_eq(sp.sec, 0);
    ck_assert_int_eq(sp.attoSec, expected_atto);

    nanoseconds ns_out;
    sp.get(ns_out);
    // 整数纳秒会截断小数部分
    ck_assert_int_eq(ns_out.count(), 1);
}

// ===================== 进位/退位边界测试 =====================
REG_TEST(test_sp_time_positive_carry) {
    using Atto = StreamPunkTime::AttoSec;
    StreamPunkTime sp;

    // 模拟1秒 + 0.999...秒（刚好不到进位）
    sp.sec = 0;
    sp.attoSec = Atto::period::den - 1; // 1e18 - 1

    auto ns = std::chrono::nanoseconds(0);
    sp.get(ns);
    // 应精确到纳秒级
    ck_assert_uint_eq(ns.count(), 999'999'999);
}

REG_TEST(test_sp_time_negative_borrow) {
    using namespace std::chrono;
    StreamPunkTime sp;

    // -2.5 atto秒（需要退位）
    sp.sec = -1;
    sp.attoSec = -2;

    seconds sec_out;
    sp.get(sec_out);
    ck_assert_int_eq(sec_out.count(), -1);
}


// 辅助函数：测试时间类型的往返序列化
template<typename Duration>
void test_roundtrip(Duration input) {
    std::stringstream ss;
    O o(ss);
    I i(ss);

    o << input;
    Duration output;
    i >> output;

    ck_assert_msg(input == output,
        "往返序列化失败: %lld -> %lld",
        input.count(), output.count());
}

// 辅助函数：测试时间精度转换
template<typename From, typename To>
bool test_conversion(From input, To expected) {
    std::stringstream ss;
    O o(ss);
    I i(ss);
    o << input;
    To output;
    i >> output;
    if (output != expected) {
        return false;
    }
    return true;
}

// ===================== 基础时间类型测试 ====================
REG_TEST(test_seconds) {
    test_roundtrip(seconds(5));
    test_roundtrip(seconds(-10));
    test_roundtrip(seconds(0));
}

REG_TEST(test_milliseconds) {
    test_roundtrip(milliseconds(100));
    test_roundtrip(milliseconds(-500));
    test_roundtrip(milliseconds(0));
}

REG_TEST(test_microseconds) {
    test_roundtrip(microseconds(123'456));
    test_roundtrip(microseconds(-78'901));
}

REG_TEST(test_nanoseconds) {
    test_roundtrip(nanoseconds(999'999'999));
    test_roundtrip(nanoseconds(-123'456'789));
}


// ===================== 边界值测试 ======================
REG_TEST(test_max_seconds) {
    constexpr auto max_sec = seconds::max();
    test_roundtrip(max_sec);

    StreamPunkTime sp;
    sp.set(max_sec);
    ck_assert(sp.sec == max_sec.count());
    ck_assert(sp.attoSec == 0);
}

REG_TEST(test_min_seconds) {
    constexpr auto min_sec = seconds::min();
    test_roundtrip(min_sec);

    StreamPunkTime sp;
    sp.set(min_sec);
    ck_assert(sp.sec == min_sec.count());
    ck_assert(sp.attoSec == 0);
}

REG_TEST(test_attosecond_boundary) {
    // 1. 测试1秒以下的边界值（用atto秒单位构造）
    using AttoDuration = std::chrono::duration<int64_t, std::atto>;
    AttoDuration atto_val(MAX_ATTO - 1); // 0.999999999999999999秒
    StreamPunkTime sp;
    sp.set(atto_val);
    ck_assert_msg(sp.sec == 0 && sp.attoSec == MAX_ATTO - 1,
        "未正确处理临界值: sec=%lld, atto=%lld", sp.sec, sp.attoSec);

    // 2. 测试进位（用atto秒单位构造）
    AttoDuration atto_val2(MAX_ATTO * 2 + 5); // 2.000000000000000005秒
    sp.set(atto_val2);
    ck_assert_msg(sp.sec == 2 && sp.attoSec == 5,
        "进位处理失败: sec=%lld, atto=%lld", sp.sec, sp.attoSec);
}

REG_TEST(test_nanosecond_conversion) {
    // 1. 测试大整数纳秒（123456789秒）
    int64_t ns_val = 1000000000LL * 123456789; // 123,456,789,000,000,000纳秒
    nanoseconds ns_big(ns_val);
    StreamPunkTime sp;
    sp.set(ns_big);
    ck_assert_msg(sp.sec == 123456789 && sp.attoSec == 0,
        "大纳秒整数秒转换失败: sec=%lld, atto=%lld", sp.sec, sp.attoSec);

    // 2. 测试小数部分（1纳秒 = 1e9 atto秒）
    nanoseconds ns_fraction(1); // 0.000000001秒
    sp.set(ns_fraction);
    ck_assert_msg(sp.sec == 0 && sp.attoSec == 1000000000,
        "纳秒小数转换失败: sec=%lld, atto=%lld", sp.sec, sp.attoSec);
}


// ===================== 时间精度转换测试 ================== 
REG_TEST(test_ns_to_us_conversion) {
    // 纳秒→微秒转换测试
    using namespace std::chrono;

    // 正数测试
    ck_assert(test_conversion(nanoseconds(1500), microseconds(1)) );   // 1.5us → 截断至 1us
    ck_assert(test_conversion(nanoseconds(1499), microseconds(1)) );   // 1.499us → 截断至 1us
    ck_assert(test_conversion(nanoseconds(1), microseconds(0)) );      // 0.001us → 截断至 0us

    // 负数测试
    ck_assert(test_conversion(nanoseconds(-1500), microseconds(-1)) ); // -1.5us → 截断至 -1us
    ck_assert(test_conversion(nanoseconds(-1499), microseconds(-1)) );// -1.499us → 截断至 -1us
    ck_assert(test_conversion(nanoseconds(-1), microseconds(0)) );     // -0.001us → 截断至 0us

    // 边界值测试
    ck_assert(test_conversion(nanoseconds(999'999'999'500), seconds(999))); // 999.9999995s → 1000s
}

REG_TEST(test_us_to_ms_conversion) {
    // 微秒→毫秒转换测试
    using namespace std::chrono;

    // 正数测试
    ck_assert(test_conversion(microseconds(1500), milliseconds(1)));   // 1.5ms → 截断至 2ms
    ck_assert(test_conversion(microseconds(1499), milliseconds(1)));    // 1.499ms → 截断至 1ms
    ck_assert(test_conversion(microseconds(1), milliseconds(0)));       // 0.001ms → 截断至 0ms

    // 负数测试
    ck_assert(test_conversion(microseconds(-1500), milliseconds(-1))); // -1.5ms → 截断至 -2ms
    ck_assert(test_conversion(microseconds(-1499), milliseconds(-1)));  // -1.499ms → 截断至 -1ms
    ck_assert(test_conversion(microseconds(-1), milliseconds(0)));      // -0.001ms → 截断至 0ms

    // 边界值测试
    ck_assert(test_conversion(microseconds(999'500), seconds(0)));      // 0.9995s → 截断至 1s
}

// ===================== 流操作符测试 =====================
REG_TEST(test_stream_operator_int_ms_roundtrip) {
    using namespace std::chrono;
    milliseconds ms_in(123456);

    std::stringstream ss;
    O o(ss);
    o << ms_in;

    milliseconds ms_out;
    I i(ss);
    i >> ms_out;

    ck_assert_int_eq(ms_in.count(), ms_out.count());
}

REG_TEST(test_stream_operator_float_us_roundtrip) {
    using FpMicro = std::chrono::duration<double, std::micro>;
    FpMicro us_in(12345.6789);

    std::stringstream ss;
    O o(ss);
    o << us_in;

    FpMicro us_out;
    I i(ss);
    i >> us_out;

    //// 允许10个atto秒误差
    ck_assert_double_eq_tol(us_in.count(), us_out.count(), 1e-11);
}

REG_TEST(test_stream_time_point) {
    using namespace std::chrono;
    system_clock::time_point tp_in = system_clock::now();

    std::stringstream ss;
    O o(ss);
    o << tp_in;

    system_clock::time_point tp_out;
    I i(ss);
    i >> tp_out;

    // 系统时间精度一般为微秒级
    ck_assert_uint_eq(
        duration_cast<microseconds>(tp_in.time_since_epoch()).count(),
        duration_cast<microseconds>(tp_out.time_since_epoch()).count()
    );
}

// ===================== 错误处理测试 =====================
REG_TEST(test_invalid_stream_input) {
    using namespace std::chrono;
    std::stringstream ss("invalid_data");
    I i(ss);
    seconds sec;
    bool exception_thrown = false;
    try {
        i >> sec;
    }
    catch (const std::ios_base::failure&) {
        exception_thrown = true;
    }
    // 目前还没错误处理和发现机制
    ck_assert(exception_thrown);
}
// */