#include "unit_test.hpp"

#include <iostream>
#include <typeinfo>

class A { public: virtual ~A() {} }; // 基类必须有虚函数
class B : public A {};
class C : public A {};
class D : public A {};
class E : public B {};

REG_TEST(test_typeid) {
    A* a = new E(); // 实际指向E对象
    // 获取类型信息
    const std::type_info& t = typeid(*a);
    // 比较已知类型
    if (t == typeid(B))
        std::cout << "Actual type: B" << std::endl;
    else if (t == typeid(C))
        std::cout << "Actual type: C" << std::endl;
    else if (t == typeid(D))
        std::cout << "Actual type: D" << std::endl;
    else if (t == typeid(E))
        std::cout << "Actual type: E" << std::endl;
    // 获取类型名称（编译器相关，可能需要demangle）
    std::cout << "Type name: " << t.name() << std::endl;
    delete a;
}


