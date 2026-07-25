// Windows 平台控制台 UTF-8 区域设置初始化
// 确保中文等宽字符在控制台正确输出
#pragma once

#include <iostream>
#include <locale>

class LocaleInitializer {
public:
    static void Initialize() {
        static bool initialized = []() {
            try {
                std::locale::global(std::locale("en_US.UTF-8"));
                std::cout.imbue(std::locale());
                return true;
            }
            catch (const std::exception& e) {
                std::cerr << "Locale init failed: " << e.what() << '\n';
                return false;
            }
            }();
        (void)initialized;
    }
    LocaleInitializer() { Initialize(); }
};

inline LocaleInitializer __locale_initializer;