#include "Data.hpp"
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

using namespace sp;

inline void writeU32(std::ostream& os, uint32_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
inline void writeU16(std::ostream& os, uint16_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
inline void writeStr(std::ostream& os, const std::string& s) {
    writeU16(os, static_cast<uint16_t>(s.size()));
    if (!s.empty()) os.write(s.data(), s.size());
}
inline void writeTypeMeta(std::ostream& os, std::uint32_t typeID,
    const std::string& className, const std::string& baseName,
    const std::vector<std::string>& memberNames,
    const std::vector<std::vector<Sz>>& memberDescs)
{
    writeU32(os, typeID);
    writeStr(os, className);
    writeStr(os, baseName);
    writeU16(os, static_cast<uint16_t>(memberNames.size()));
    for (size_t i = 0; i < memberNames.size(); ++i) {
        writeStr(os, memberNames[i]);
        writeU16(os, static_cast<uint16_t>(memberDescs[i].size()));
        if (!memberDescs[i].empty()) {
            os.write(reinterpret_cast<const char*>(memberDescs[i].data()),
                     memberDescs[i].size() * sizeof(Sz));
        }
    }
}

#define META_STRUCT(T, ...) \
    { \
        T obj; \
        auto names = obj.getMemberNames(); \
        auto descs = __VA_ARGS__; \
        writeTypeMeta(ofs, static_cast<uint32_t>(obj.typeID()), \
            obj.getClassName(), obj.getBaseName(), \
            std::vector<std::string>(names.begin(), names.end()), descs); \
    }

int main() {
    INIT_StreamPunk();

    constexpr uint32_t META_MAGIC = 0x53504D44;
    constexpr uint32_t META_VERSION = 1;

    std::filesystem::create_directories("temp");
    std::ofstream ofs("temp/stream-punk-meta.bin", std::ios::binary);

    writeU32(ofs, META_MAGIC);
    writeU32(ofs, META_VERSION);
    writeU32(ofs, 17); // 17 types

    // Ingredient: id(i32), name(string), unit(string), stock(f64), minStock(f64)
    META_STRUCT(Ingredient, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<double>::v.begin(), TypeDesc<double>::v.end()},
        {TypeDesc<double>::v.begin(), TypeDesc<double>::v.end()},
    });

    // MenuItem: id(i32), name(string), price(f64), category(string), available(bool)
    META_STRUCT(MenuItem, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<double>::v.begin(), TypeDesc<double>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<bool>::v.begin(), TypeDesc<bool>::v.end()},
    });

    // RecipeItem: ingredientId(i32), ingredientName(string), quantity(f64)
    META_STRUCT(RecipeItem, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<double>::v.begin(), TypeDesc<double>::v.end()},
    });

    // Recipe: menuItemId(i32), items(vector<RecipeItem>)
    META_STRUCT(Recipe, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::vector<RecipeItem>>::v.begin(), TypeDesc<std::vector<RecipeItem>>::v.end()},
    });

    // OrderItem: menuItemId(i32), menuItemName(string), quantity(i32), status(i32), note(string)
    META_STRUCT(OrderItem, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
    });

    // Table: id(i32), name(string), type(i32), status(i32), capacity(i32)
    META_STRUCT(Table, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
    });

    // Order: id(i32), tableId(i32), tableName(string), items(vector<OrderItem>), status(i32), totalPrice(f64), discount(f64), paymentMethod(string), note(string)
    META_STRUCT(Order, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<std::vector<OrderItem>>::v.begin(), TypeDesc<std::vector<OrderItem>>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<double>::v.begin(), TypeDesc<double>::v.end()},
        {TypeDesc<double>::v.begin(), TypeDesc<double>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
    });

    // ServerState: orders, tables, menu, ingredients, recipes
    META_STRUCT(ServerState, std::vector<std::vector<Sz>>{
        {TypeDesc<std::vector<Order>>::v.begin(), TypeDesc<std::vector<Order>>::v.end()},
        {TypeDesc<std::vector<Table>>::v.begin(), TypeDesc<std::vector<Table>>::v.end()},
        {TypeDesc<std::vector<MenuItem>>::v.begin(), TypeDesc<std::vector<MenuItem>>::v.end()},
        {TypeDesc<std::vector<Ingredient>>::v.begin(), TypeDesc<std::vector<Ingredient>>::v.end()},
        {TypeDesc<std::vector<Recipe>>::v.begin(), TypeDesc<std::vector<Recipe>>::v.end()},
    });

    // PlaceOrderRequest: tableId(i32), items(vector<OrderItem>)
    META_STRUCT(PlaceOrderRequest, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::vector<OrderItem>>::v.begin(), TypeDesc<std::vector<OrderItem>>::v.end()},
    });

    // UpdateStatusRequest: orderId(i32), itemIndex(i32), newStatus(i32), note(string)
    META_STRUCT(UpdateStatusRequest, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
    });

    // PaymentRequest: tableId(i32), paymentMethod(string), discount(f64), authCode(string)
    META_STRUCT(PaymentRequest, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<double>::v.begin(), TypeDesc<double>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
    });

    // RoleLoginRequest: role(i32), name(string)
    META_STRUCT(RoleLoginRequest, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
    });

    // Notification: message(string), type(i32), targetRole(i32)
    META_STRUCT(Notification, std::vector<std::vector<Sz>>{
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
    });

    // ChangeTableRequest: fromTableId(i32), toTableId(i32)
    META_STRUCT(ChangeTableRequest, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
    });

    // UrgeDishRequest: orderId(i32)
    META_STRUCT(UrgeDishRequest, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
    });

    // MergeOrdersRequest: tableId1(i32), tableId2(i32)
    META_STRUCT(MergeOrdersRequest, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
    });

    // RoleAssigned: role(i32), name(string)
    META_STRUCT(RoleAssigned, std::vector<std::vector<Sz>>{
        {TypeDesc<std::int32_t>::v.begin(), TypeDesc<std::int32_t>::v.end()},
        {TypeDesc<std::string>::v.begin(), TypeDesc<std::string>::v.end()},
    });

    return 0;
}