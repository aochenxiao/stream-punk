#include "unit_test.hpp"


struct A {
    struct Maliu {

    };
};

struct B:public A {
    struct Maliu {

    };
};


REG_TEST(test_enum_numbers) {
    enum E_test {
        e0,
        e1,
        e2,
        e100 = 100,
        e101,
        e102,
        e300 = 300,
        e301,
        e302
    };

    ck_assert_int_eq(e100, 100);
    ck_assert_int_eq(e101, 101);
    ck_assert_int_eq(e102, 102);
    ck_assert_int_eq(e300, 300);
    ck_assert_int_eq(e301, 301);
    ck_assert_int_eq(e302, 302);
}


// 定义测试用模板类型
template <typename> struct TestTemplate1 {};
template <typename, typename> struct TestTemplate2 {};

REG_TEST(test_template_traits) {
    using namespace detail;

    // 测试 is_template_v 基本功能
    static_assert(is_template_v<TestTemplate1<int>, TestTemplate1>,
        "is_template_v should recognize template instances");
    static_assert(is_template_v<std::vector<int>, std::vector>,
        "is_template_v should work with std templates");
    static_assert(is_template_v<TestTemplate2<float, double>, TestTemplate2>,
        "is_template_v should work with multi-param templates");
    static_assert(!is_template_v<int, TestTemplate1>,
        "is_template_v should reject non-template types");
    static_assert(!is_template_v<TestTemplate1<int>, TestTemplate2>,
        "is_template_v should reject wrong template types");

    // 验证 constexpr 属性
    constexpr bool b1 = is_template_v<TestTemplate1<char>, TestTemplate1>;
    constexpr bool b2 = !is_template_v<float, std::vector>;
    ck_assert(b1);
    ck_assert(b2);

    // 测试 in_template_v 基本功能
    static_assert(in_template_v<std::vector<int>, std::vector, std::list>,
        "in_template_v should recognize matching templates");
    static_assert(in_template_v<TestTemplate1<double>, TestTemplate1, TestTemplate2>,
        "in_template_v should recognize custom templates");
    static_assert(!in_template_v<std::string, std::vector, std::list>,
        "in_template_v should reject non-template types");
    static_assert(!in_template_v<TestTemplate2<int, int>, std::vector, TestTemplate1>,
        "in_template_v should reject non-matching templates");

    // 测试复合模板类型
    using NestedType = std::vector<TestTemplate1<int>>;
    static_assert(is_template_v<NestedType, std::vector>,
        "is_template_v should handle nested templates");
    static_assert(in_template_v<NestedType, std::vector, std::list>,
        "in_template_v should handle nested templates");

    // 测试多重模板匹配
    static_assert(in_template_v<std::list<float>, std::list, std::vector, TestTemplate1>,
        "in_template_v should match any template in the list");
    static_assert(in_template_v<TestTemplate1<int>, std::vector, TestTemplate1>,
        "in_template_v should match custom templates in multi-list");
    static_assert(!in_template_v<int, std::vector, TestTemplate1, TestTemplate2>,
        "in_template_v should reject non-template in multi-list");

    // 验证运行时行为
    ck_assert(in_template_v<std::forward_list<char> DH std::forward_list DH std::list>);
    ck_assert(!in_template_v<double DH std::vector DH std::map>);

    // 边界测试：空模板列表
    static_assert(!in_template_v<std::vector<int>>,
        "in_template_v should return false for empty template list");
}

// ==================================================================

// 测试类型定义
struct Foo {};
struct Bar {};
using VoidPtr = void*;

REG_TEST(test_contains_true) {
    // 包含基本类型
    static_assert(detail::contains_v<int, std::tuple<int, double, char>>, "int should be found");
    static_assert(detail::contains_v<double, std::tuple<int, double, char>>, "double should be found");

    // 包含自定义类型
    static_assert(detail::contains_v<Foo, std::tuple<int, Foo, Bar>>, "Foo should be found");

    // 包含重复类型
    static_assert(detail::contains_v<char, std::tuple<char, int, char>>, "char should be found");

    // 包含别名类型
    static_assert(detail::contains_v<VoidPtr, std::tuple<void*, int, float>>, "VoidPtr should be found");

    // 包含自身
    static_assert(detail::contains_v<void, std::tuple<void>>, "void should be found in singleton");

    ck_assert(true); // 静态断言成功表示测试通过
}

REG_TEST(test_contains_false) {
    // 不包含基本类型
    static_assert(!detail::contains_v<float, std::tuple<int, double, char>>, "float should not be found");

    // 不包含自定义类型
    static_assert(!detail::contains_v<Bar, std::tuple<int, Foo, char>>, "Bar should not be found");

    // 空元组
    static_assert(!detail::contains_v<int, std::tuple<>>, "int should not be found in empty tuple");

    // 不包含别名类型
    using IntRef = int&;
    static_assert(!detail::contains_v<IntRef, std::tuple<int, double>>, "IntRef should not be found");

    // 类似但不完全相同的类型
    static_assert(!detail::contains_v<const int, std::tuple<int>>, "const int not same as int");

    ck_assert(true); // 静态断言成功表示测试通过
}

REG_TEST(test_contains_edge_cases) {
    // 包含 void 类型
    static_assert(detail::contains_v<void, std::tuple<int, void, double>>, "void should be found");

    // 包含函数指针类型
    using FuncPtr = void(*)();
    static_assert(detail::contains_v<FuncPtr, std::tuple<int, FuncPtr, double>>,
        "function pointer should be found");

    // 包含成员指针类型
    struct Test { int x; };
    using MemPtr = int Test::*;
    static_assert(detail::contains_v<MemPtr, std::tuple<int, MemPtr, double>>,
        "member pointer should be found");

    // 包含元组类型本身
    using TupleType = std::tuple<int, double>;
    static_assert(detail::contains_v<TupleType, std::tuple<char, TupleType, float>>,
        "tuple type itself should be found");

    // 包含 volatile 类型
    static_assert(!detail::contains_v<volatile int, std::tuple<int, double>>,
        "volatile int not same as int");

    ck_assert(true); // 静态断言成功表示测试通过
}



REG_TEST(test_contains_v_basic) {
    // 测试基本类型包含检测
    static_assert(detail::contains_v<int, std::tuple<int, float, double>>, "int should be found");
    static_assert(detail::contains_v<float, std::tuple<int, float, double>>, "float should be found");
    static_assert(!detail::contains_v<char, std::tuple<int, float, double>>, "char should not be found");
    static_assert(detail::contains_v<std::vector<int>, std::tuple<std::vector<int>, std::string>>, "vector should be found");

    // 运行时验证（辅助检查）
    ck_assert(true); // 编译时测试通过则运行至此
}

REG_TEST(test_contains_v_empty) {
    // 测试空tuple
    static_assert(!detail::contains_v<int, std::tuple<>>, "Empty tuple shouldn't contain int");
    static_assert(!detail::contains_v<void, std::tuple<>>, "Empty tuple shouldn't contain void");
    static_assert(!detail::contains_v<std::tuple<>, std::tuple<>>, "Empty tuple shouldn't contain itself");
}

REG_TEST(test_unique_tuple_basic) {
    // 基本类型去重
    using TestTypes = std::tuple<int, float, int, double, float>;
    using UniqueTypes = detail::unique_tuple_from_t<TestTypes>;

    static_assert(std::is_same_v<UniqueTypes, std::tuple<int, float, double>>,
        "Duplicates should be removed");
    static_assert(3 == std::tuple_size_v<UniqueTypes>,
        "Should have 3 unique types");
}

REG_TEST(test_unique_template_types) {
    // 测试模板类型去重
    using TestTypes = std::tuple<
        std::vector<int>,
        std::vector<double>,
        std::vector<int>, // duplicate
        std::vector<std::string>,
        std::vector<int>  // duplicate
    >;

    using UniqueTypes = detail::unique_tuple_from_t<TestTypes>;

    static_assert(3 == std::tuple_size_v<UniqueTypes>,
        "Should have 3 unique vector types");
    static_assert(detail::contains_v<std::vector<int>, UniqueTypes>,
        "vector<int> should be preserved");
    static_assert(detail::contains_v<std::vector<double>, UniqueTypes>,
        "vector<double> should be preserved");
    static_assert(detail::contains_v<std::vector<std::string>, UniqueTypes>,
        "vector<string> should be preserved");
}

REG_TEST(test_nested_tuples) {
    // 测试嵌套元组
    using InnerTuple = std::tuple<int, double>;
    using TestTypes = std::tuple<
        InnerTuple,
        std::vector<int>,
        InnerTuple  // duplicate
    >;

    using UniqueTypes = detail::unique_tuple_from_t<TestTypes>;

    static_assert(2 == std::tuple_size_v<UniqueTypes>,
        "Should have 2 unique types");
    static_assert(detail::contains_v<InnerTuple, UniqueTypes>,
        "InnerTuple should be preserved");
    static_assert(detail::contains_v<std::vector<int>, UniqueTypes>,
        "vector<int> should be preserved");
}

REG_TEST(test_alias_handling) {
    // 测试类型别名处理
    using IntAlias = int;
    using TestTypes = std::tuple<int, IntAlias, float>;
    using UniqueTypes = detail::unique_tuple_from_t<TestTypes>;

    static_assert(std::is_same_v<UniqueTypes, std::tuple<int, float>> ||
        std::is_same_v<UniqueTypes, std::tuple<IntAlias, float>>,
        "Should collapse alias to same type");
}

REG_TEST(test_preserve_order) {
    // 测试重复类型保留第一次出现顺序
    using TestTypes = std::tuple<
        int,          // 1st int
        float,        // 1st float
        int,          // duplicate int
        double,       // 1st double
        float         // duplicate float
    >;

    using UniqueTypes = detail::unique_tuple_from_t<TestTypes>;

    static_assert(std::is_same_v<UniqueTypes, std::tuple<int, float, double>>,
        "Should preserve first occurrence order: int -> float -> double");
}

REG_TEST(test_complex_order) {
    // 测试复杂类型顺序保留
    using TestTypes = std::tuple<
        std::vector<int>,
        std::tuple<char, double>,
        std::vector<double>,
        std::tuple<char, double>,  // duplicate
        std::vector<std::string>,
        std::vector<double>        // duplicate
    >;

    using UniqueTypes = detail::unique_tuple_from_t<TestTypes>;

    static_assert(std::is_same_v<
        UniqueTypes,
        std::tuple<
        std::vector<int>,
        std::tuple<char, double>,
        std::vector<double>,
        std::vector<std::string>
        >
    >, "Should preserve complex type order");
}

REG_TEST(test_single_type_duplicates) {
    // 测试单一类型重复
    using TestTypes = std::tuple<
        double,
        double,
        double,
        double,
        double
    >;

    using UniqueTypes = detail::unique_tuple_from_t<TestTypes>;

    static_assert(std::is_same_v<UniqueTypes, std::tuple<double>>,
        "Should reduce to single instance");
    static_assert(1 == std::tuple_size_v<UniqueTypes>,
        "Should have single element");
}

REG_TEST(test_order_with_mixed_qualifiers) {
    // 测试带修饰符的类型顺序（const/volatile）
    using TestTypes = std::tuple<
        int,
        const int,
        volatile int,
        int,          // duplicate
        const int,    // duplicate
        double
    >;

    using UniqueTypes = detail::unique_tuple_from_t<TestTypes>;

    static_assert(std::is_same_v<UniqueTypes, std::tuple<int, const int, volatile int, double>>,
        "Should preserve order with qualifiers (distinct types)");
}

REG_TEST(test_order_with_alias) {
    // 测试类型别名顺序保留
    using IntAlias = int;
    using TestTypes = std::tuple<
        IntAlias,   // 1st int
        float,
        int,        // duplicate (same as IntAlias)
        double
    >;

    using UniqueTypes = detail::unique_tuple_from_t<TestTypes>;

    static_assert(std::tuple_size_v<UniqueTypes> == 3,
        "Should have 3 unique types");

    // 验证顺序：IntAlias应保留为第一个元素
    static_assert(std::is_same_v<
        std::tuple_element_t<0, UniqueTypes>,
        IntAlias
    >, "First element should be IntAlias");

    static_assert(std::is_same_v<
        std::tuple_element_t<1, UniqueTypes>,
        float
    >, "Second element should be float");

    static_assert(std::is_same_v<
        std::tuple_element_t<2, UniqueTypes>,
        double
    >, "Second element should be double");
}



