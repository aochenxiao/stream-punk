// 示例 05：ORM SQL 生成
// 展示：从 C++ struct 自动生成 CREATE TABLE / INSERT / UPDATE / DELETE / SELECT

#include "../../include/stream-punk/StreamPunk.hpp"
#include "../00-demo-types/Data.hpp"
using namespace sp;
#include "../common/locale_init.hpp"
#include "../../include/stream-punk/StreamPunkOrmGen.hpp"
#include <iostream>

// 定义 ORM 映射的数据类型
struct Product : public Base {
    #define Xt_Product(X__) \
    X__(i32, id, 0, ORM_PRIMARY_KEY | ORM_AUTO_INCREMENT) \
    X__(std::string, name, "") \
    X__(f64, price, 0.0) \
    X__(i32, stock, 0) \
    X__(std::string, category, "")

    Product() = default;
    UseData(Product);
    UseDataOrm(Product);
};
REGISTER_JSON_TYPE(Product);

int main() {
    SpRegistry reg;
    INIT_StreamPunk(&reg);

    Product p;
    p.id = 1;
    p.name = "机械键盘";
    p.price = 299.99;
    p.stock = 100;
    p.category = "外设";

    // 自动生成 SQL
    std::cout << "=== CREATE TABLE ===" << std::endl;
    std::cout << sp::createTable<Product>().sql << std::endl;

    std::cout << std::endl << "=== INSERT ===" << std::endl;
    std::cout << sp::insert(p).sql << std::endl;

    std::cout << std::endl << "=== UPDATE ===" << std::endl;
    std::cout << sp::update(p).sql << std::endl;

    std::cout << std::endl << "=== DELETE ===" << std::endl;
    std::cout << sp::deleteById<Product>(1).sql << std::endl;

    std::cout << std::endl << "=== SELECT ===" << std::endl;
    std::cout << sp::selectAll<Product>().sql << std::endl;

    std::cout << std::endl;
    std::cout << "--- 说明 ---" << std::endl;
    std::cout << "ORM 生成 SQL 字符串，用户自行管理数据库连接。" << std::endl;
    std::cout << "支持的类型：基础类型、std::string、std::optional 等。" << std::endl;
    std::cout << "智能指针和容器类型暂不直接映射到 SQL 列。" << std::endl;

    return 0;
}