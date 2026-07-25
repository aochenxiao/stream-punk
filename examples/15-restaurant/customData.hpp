#pragma once

namespace sp {
struct Ingredient;
struct MenuItem;
struct RecipeItem;
struct Recipe;
struct OrderItem;
struct Table;
struct Order;
struct ServerState;
struct PlaceOrderRequest;
struct UpdateStatusRequest;
struct PaymentRequest;
struct RoleLoginRequest;
struct Notification;
struct ChangeTableRequest;
struct UrgeDishRequest;
struct MergeOrdersRequest;
struct RoleAssigned;
}

#define Xt_CustomType(X__) \
X__(sp::Ingredient, Ingredient) \
X__(sp::MenuItem, MenuItem) \
X__(sp::RecipeItem, RecipeItem) \
X__(sp::Recipe, Recipe) \
X__(sp::OrderItem, OrderItem) \
X__(sp::Table, Table) \
X__(sp::Order, Order) \
X__(sp::ServerState, ServerState) \
X__(sp::PlaceOrderRequest, PlaceOrderRequest) \
X__(sp::UpdateStatusRequest, UpdateStatusRequest) \
X__(sp::PaymentRequest, PaymentRequest) \
X__(sp::RoleLoginRequest, RoleLoginRequest) \
X__(sp::Notification, Notification) \
X__(sp::ChangeTableRequest, ChangeTableRequest) \
X__(sp::UrgeDishRequest, UrgeDishRequest) \
X__(sp::MergeOrdersRequest, MergeOrdersRequest) \
X__(sp::RoleAssigned, RoleAssigned)