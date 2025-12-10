#include "unit_test.hpp"


REG_TEST(test_enum_numbers) {
    enum E_test {
        e0,
        e1,
        e2,
        e100 = 100,
        e101,
        e102,
    };

    ck_assert_int_eq(e100, 100);
    ck_assert_int_eq(e101, 101);
    ck_assert_int_eq(e102, 102);
}