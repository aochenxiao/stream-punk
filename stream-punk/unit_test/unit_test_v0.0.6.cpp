#include "unit_test.hpp"
#include <optional>
#include <filesystem>
#include <atomic>
#include <thread>

REG_TEST(test_optional_basic) {
    // Arrange
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // Test cases for different optional states
    std::optional<int> opt_value(42);
    std::optional<std::string> opt_string("test");
    std::optional<double> empty_opt;

    // Act & Assert - value optional
    out << opt_value;
    std::optional<int> opt_value_in;
    in >> opt_value_in;
    ck_assert(opt_value_in.has_value());
    ck_assert_int_eq(*opt_value_in, 42);

    // Clear stream
    ss.str("");
    ss.clear();

    // Act & Assert - string optional
    out << opt_string;
    std::optional<std::string> opt_string_in;
    in >> opt_string_in;
    ck_assert(opt_string_in.has_value());
    ck_assert_str_eq(opt_string_in->c_str(), "test");

    // Clear stream
    ss.str("");
    ss.clear();

    // Act & Assert - empty optional
    out << empty_opt;
    std::optional<double> empty_opt_in(3.14);
    in >> empty_opt_in;
    ck_assert(!empty_opt_in.has_value());
}

REG_TEST(test_filesystem_path) {
    // Arrange
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // Test paths with various characteristics
    std::filesystem::path unix_path("/home/user/文档/测试.txt");
    std::filesystem::path windows_path("C:\\Users\\Admin\\Desktop\\répertoire");
    std::filesystem::path empty_path;

    // Act & Assert - unicode path
    out << unix_path;
    std::filesystem::path path_in1;
    in >> path_in1;
    ck_assert(unix_path == path_in1);

    // Clear stream
    ss.str("");
    ss.clear();

    // Act & Assert - windows-style path
    out << windows_path;
    std::filesystem::path path_in2;
    in >> path_in2;
    ck_assert(windows_path == path_in2);

    // Clear stream
    ss.str("");
    ss.clear();

    // Act & Assert - empty path
    out << empty_path;
    std::filesystem::path path_in3;
    in >> path_in3;
    ck_assert(path_in3.empty());
}

REG_TEST(test_atomic_basic) {
    // Arrange
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    int val = 42;
    std::string str = "atomic_test";
    std::atomic_ref<int> atomic_ref_int(val);
    //std::atomic_ref<std::string> atomic_ref_str(str);
    std::atomic<int> atomic_int(100);
    //std::atomic<std::string> atomic_str("hello");

    // Act & Assert - atomic_ref to integer
    out << atomic_ref_int;
    int int_val_in;
    std::atomic_ref<int> atomic_ref_in(int_val_in);
    in >> atomic_ref_in;
    ck_assert_int_eq(int_val_in, 42);

    // Clear stream
    ss.str("");
    ss.clear();

    // Act & Assert - atomic_ref to string
    //out << atomic_ref_str;
    //std::string str_val_in;
    //std::atomic_ref<std::string> atomic_ref_str_in(str_val_in);
    //in >> atomic_ref_str_in;
    //ck_assert_str_eq(str_val_in.c_str(), "atomic_test");

    // Clear stream
    ss.str("");
    ss.clear();

    // Act & Assert - atomic integer
    out << atomic_int;
    std::atomic<int> atomic_int_in(0);
    in >> atomic_int_in;
    ck_assert_int_eq(atomic_int_in.load(), 100);

    // Clear stream
    ss.str("");
    ss.clear();

    // Act & Assert - atomic string
    //out << atomic_str;
    //std::atomic<std::string> atomic_str_in("");
    //in >> atomic_str_in;
    //ck_assert_str_eq(atomic_str_in.load().c_str(), "hello");
}

REG_TEST(test_concurrent_atomic) {
    // Arrange
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    std::atomic<int> counter(0);

    // Simulate concurrent modifications
    std::thread t1([&]() { counter.fetch_add(5); });
    std::thread t2([&]() { counter.fetch_add(10); });
    t1.join();
    t2.join();

    const int original_value = counter.load();

    // Act
    out << counter;
    std::atomic<int> counter_in(0);
    in >> counter_in;

    // Assert
    ck_assert_int_eq(counter_in.load(), original_value);
}

REG_TEST(test_edge_cases) {
    // Arrange
    std::stringstream ss;
    O out{ ss };
    I in{ ss };

    // Edge case: optional with large value
    std::optional<i64> large_opt(1'000'000'000'000);
    // Act & Assert - large optional
    out << large_opt;
    std::optional<i64> large_opt_in;
    in >> large_opt_in;
    ck_assert(large_opt_in.has_value());
    ck_assert(*large_opt_in == 1'000'000'000'000);

    // Edge case: path with max possible characters
    std::filesystem::path max_path;
    for (int i = 0; i < 1000; ++i) {
        max_path /= "very_long_directory_name_";
    }

    // Edge case: atomic with zero value
    std::atomic<int> zero_atomic(0);

    // Clear stream
    ss.str("");
    ss.clear();

    // Act & Assert - max length path
    out << max_path;
    std::filesystem::path path_in;
    in >> path_in;
    ck_assert(max_path == path_in);

    // Clear stream
    ss.str("");
    ss.clear();

    // Act & Assert - zero atomic
    out << zero_atomic;
    std::atomic<int> zero_atomic_in(-1);
    in >> zero_atomic_in;
    ck_assert_int_eq(zero_atomic_in.load(), 0);
}