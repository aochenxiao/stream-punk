#pragma once
#include "customData.hpp"
#include <stream-punk/StreamPunk.hpp>

namespace sp {

// Order state machine:
// Pending -> Cutting -> ReadyToCook -> Cooking -> ReadyToServe -> InTransit -> Delivered -> Completed
// Exception: Remake, Cancelled, Damaged
namespace OrderStatus {
    constexpr i32 Pending = 0;
    constexpr i32 Cutting = 1;
    constexpr i32 ReadyToCook = 2;
    constexpr i32 Cooking = 3;
    constexpr i32 ReadyToServe = 4;
    constexpr i32 InTransit = 5;
    constexpr i32 Delivered = 6;
    constexpr i32 Completed = 7;
    constexpr i32 Remake = 8;
    constexpr i32 Damaged = 9;
    constexpr i32 Cancelled = 10;
    constexpr i32 Urgent = 11;
}

namespace TableStatus {
    constexpr i32 Available = 0;
    constexpr i32 Occupied = 1;
    constexpr i32 Dirty = 2;
    constexpr i32 Reserved = 3;
}

namespace Role {
    constexpr i32 Boss = 0;
    constexpr i32 Manager = 1;
    constexpr i32 Cashier = 2;
    constexpr i32 HeadChef = 3;
    constexpr i32 Chef = 4;
    constexpr i32 Cutter = 5;
    constexpr i32 Runner = 6;
    constexpr i32 Waiter = 7;
    constexpr i32 Warehouse = 8;
    constexpr i32 Customer = 9;
}

namespace TableType {
    constexpr i32 Table = 0;
    constexpr i32 PrivateRoom = 1;
}

// ==================== Data Structures ====================

struct Ingredient : public Base {
    #define Xt_Ingredient(X__) \
    X__(i32, id, 0) \
    X__(std::string, name, "") \
    X__(std::string, unit, "") \
    X__(f64, stock, 0.0) \
    X__(f64, minStock, 5.0)
    Ingredient() = default;
    UseData(Ingredient);
};

struct MenuItem : public Base {
    #define Xt_MenuItem(X__) \
    X__(i32, id, 0) \
    X__(std::string, name, "") \
    X__(f64, price, 0.0) \
    X__(std::string, category, "") \
    X__(bool, available, true)
    MenuItem() = default;
    UseData(MenuItem);
};

struct RecipeItem : public Base {
    #define Xt_RecipeItem(X__) \
    X__(i32, ingredientId, 0) \
    X__(std::string, ingredientName, "") \
    X__(f64, quantity, 0.0)
    RecipeItem() = default;
    UseData(RecipeItem);
};

struct Recipe : public Base {
    #define Xt_Recipe(X__) \
    X__(i32, menuItemId, 0) \
    X__(std::vector<RecipeItem>, items, std::vector<RecipeItem>{})
    Recipe() = default;
    UseData(Recipe);
};

struct OrderItem : public Base {
    #define Xt_OrderItem(X__) \
    X__(i32, menuItemId, 0) \
    X__(std::string, menuItemName, "") \
    X__(i32, quantity, 1) \
    X__(i32, status, 0) \
    X__(std::string, note, "")
    OrderItem() = default;
    UseData(OrderItem);
    };

struct Table : public Base {
    #define Xt_Table(X__) \
    X__(i32, id, 0) \
    X__(std::string, name, "") \
    X__(i32, type, 0) \
    X__(i32, status, 0) \
    X__(i32, capacity, 4)
    Table() = default;
    UseData(Table);
};

struct Order : public Base {
    #define Xt_Order(X__) \
    X__(i32, id, 0) \
    X__(i32, tableId, 0) \
    X__(std::string, tableName, "") \
    X__(std::vector<OrderItem>, items, std::vector<OrderItem>{}) \
    X__(i32, status, 0) \
    X__(f64, totalPrice, 0.0) \
    X__(f64, discount, 1.0) \
    X__(std::string, paymentMethod, "") \
    X__(std::string, note, "")
    Order() = default;
    UseData(Order);
};

struct ServerState : public Base {
    #define Xt_ServerState(X__) \
    X__(std::vector<Order>, orders, std::vector<Order>{}) \
    X__(std::vector<Table>, tables, std::vector<Table>{}) \
    X__(std::vector<MenuItem>, menu, std::vector<MenuItem>{}) \
    X__(std::vector<Ingredient>, ingredients, std::vector<Ingredient>{}) \
    X__(std::vector<Recipe>, recipes, std::vector<Recipe>{})
    ServerState() = default;
    UseData(ServerState);
};

// ==================== Client Request Messages ====================

struct PlaceOrderRequest : public Base {
    #define Xt_PlaceOrderRequest(X__) \
    X__(i32, tableId, 0) \
    X__(std::vector<OrderItem>, items, std::vector<OrderItem>{})
    PlaceOrderRequest() = default;
    UseData(PlaceOrderRequest);
};

struct UpdateStatusRequest : public Base {
    #define Xt_UpdateStatusRequest(X__) \
    X__(i32, orderId, 0) \
    X__(i32, itemIndex, 0) \
    X__(i32, newStatus, 0) \
    X__(std::string, note, "")
    UpdateStatusRequest() = default;
    UseData(UpdateStatusRequest);
};

struct PaymentRequest : public Base {
    #define Xt_PaymentRequest(X__) \
    X__(i32, tableId, 0) \
    X__(std::string, paymentMethod, "") \
    X__(f64, discount, 1.0) \
    X__(std::string, authCode, "")
    PaymentRequest() = default;
    UseData(PaymentRequest);
};

struct RoleLoginRequest : public Base {
    #define Xt_RoleLoginRequest(X__) \
    X__(i32, role, 0) \
    X__(std::string, name, "")
    RoleLoginRequest() = default;
    UseData(RoleLoginRequest);
};

struct Notification : public Base {
    #define Xt_Notification(X__) \
    X__(std::string, message, "") \
    X__(i32, type, 0) \
    X__(i32, targetRole, 0)
    Notification() = default;
    UseData(Notification);
};

struct ChangeTableRequest : public Base {
    #define Xt_ChangeTableRequest(X__) \
    X__(i32, fromTableId, 0) \
    X__(i32, toTableId, 0)
    ChangeTableRequest() = default;
    UseData(ChangeTableRequest);
};

// ==================== New Request Types (previously bypassed StreamPunk) ====================

struct UrgeDishRequest : public Base {
    #define Xt_UrgeDishRequest(X__) \
    X__(i32, orderId, 0)
    UrgeDishRequest() = default;
    UseData(UrgeDishRequest);
};

struct MergeOrdersRequest : public Base {
    #define Xt_MergeOrdersRequest(X__) \
    X__(i32, tableId1, 0) \
    X__(i32, tableId2, 0)
    MergeOrdersRequest() = default;
    UseData(MergeOrdersRequest);
};

struct RoleAssigned : public Base {
    #define Xt_RoleAssigned(X__) \
    X__(i32, role, 0) \
    X__(std::string, name, "") \
    X__(i32, tableId, 0)
    RoleAssigned() = default;
    UseData(RoleAssigned);
};

} // namespace sp