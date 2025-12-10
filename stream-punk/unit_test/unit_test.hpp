#pragma once
#include "StreamPunk.hpp"
#include <check.h>
#include <sstream>
#include <iostream>
#include <climits>
#include <cmath>
#include <limits>
#include <cctype>
#include <cstring>
#include <typeinfo>
#include <memory>

inline Suite* s = suite_create("StreamPunk");
inline TCase* tc_core = tcase_create("Core");


// 辅助函数（兼容 GCC/Clang/MSVC）
inline std::string demangle(const char* mangled) {
#ifdef _MSC_VER
    return mangled; // MSVC 直接返回可读名称
#else
    int status;
    char* name = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    std::string result(status == 0 ? name : mangled);
    free(name);
    return result;
#endif
}

template <typename T>
inline void printType() {
    std::cout << demangle(typeid(T).name()) << std::endl;
}

# define REG_TEST(test_name) START_TEST(test_name); \
    int test_name##askdlfjasldkfj = []() { tcase_add_test(tc_core, test_name); return 0; }();\
static void test_name ## _fn (int _i CK_ATTRIBUTE_UNUSED)
