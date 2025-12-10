# include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

# include "StreamPunk.hpp"
# include "unit_test.hpp"

# include "Data.hpp"

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

int main(void) {
    INIT_StreamPunk();

    std::printf("streampunk begin...");

    int number_failed;

    tcase_set_timeout(tc_core, 0);
    suite_add_tcase(s, tc_core);

    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}