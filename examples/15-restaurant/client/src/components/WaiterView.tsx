import { useState } from 'react';
import { ServerState, Order, OrderItem, Table, MenuItem } from '../stream-punk-data';
import { RestaurantWS } from '../ws';

const STATUS_NAMES = ['待处理','切配中','待烹饪','烹饪中','待出餐','送餐中','已送达','已完成','重做','已损坏','已取消','催单'];

interface Props { state: ServerState; ws: RestaurantWS; }

function getOrderStatusClass(status: number): string {
  const names = ['Pending','Cutting','ReadyToCook','Cooking','ReadyToServe','InTransit','Delivered','Completed','Remake','Damaged','Cancelled','Urgent'];
  return `status-${names[status] || 'Pending'}`;
}

function getTableStatusClass(status: number): string {
  if (status === 0) return 'table-available';
  if (status === 1) return 'table-occupied';
  return 'table-dirty';
}

export default function WaiterView({ state, ws }: Props) {
  const [selectedTable, setSelectedTable] = useState<Table | null>(null);
  const [cart, setCart] = useState<Map<number, { item: MenuItem; quantity: number }>>(new Map());

  const tableOrders = (tableId: number) =>
    state.orders.filter(o => o.tableId === tableId && o.status !== 7 && o.status !== 10 && o.status !== 9);

  const addToCart = (menuItem: MenuItem) => {
    setCart(prev => {
      const next = new Map(prev);
      const existing = next.get(menuItem.id);
      if (existing) {
        next.set(menuItem.id, { item: menuItem, quantity: existing.quantity + 1 });
      } else {
        next.set(menuItem.id, { item: menuItem, quantity: 1 });
      }
      return next;
    });
  };

  const removeFromCart = (menuId: number) => {
    setCart(prev => {
      const next = new Map(prev);
      const existing = next.get(menuId);
      if (existing && existing.quantity > 1) {
        next.set(menuId, { ...existing, quantity: existing.quantity - 1 });
      } else {
        next.delete(menuId);
      }
      return next;
    });
  };

  const placeOrder = () => {
    if (!selectedTable || cart.size === 0) return;
    const items = Array.from(cart.values()).map(({ item, quantity }) => ({
      menuItemId: item.id,
      menuItemName: item.name,
      quantity,
    }));
    ws.placeOrder(selectedTable.id, items);
    setCart(new Map());
  };

  const handleUrge = (orderId: number) => ws.urgeDish(orderId);

  const handleDeliver = (orderId: number, itemIndex: number) => {
    ws.updateStatus(orderId, itemIndex, 6, '');
  };

  const handleDamage = (orderId: number, itemIndex: number) => {
    ws.updateStatus(orderId, itemIndex, 9, '已损坏');
  };

  const handleCancel = (orderId: number, itemIndex: number) => {
    ws.updateStatus(orderId, itemIndex, 10, '');
  };

  const cartTotal = Array.from(cart.values()).reduce((sum, { item, quantity }) => sum + item.price * quantity, 0);

  return (
    <div className="flex h-[calc(100vh-52px)]">
      {/* 左侧：餐桌列表 */}
      <div className="w-72 bg-white border-r overflow-y-auto p-3">
        <h3 className="font-bold text-sm text-gray-600 mb-2">餐桌</h3>
        <div className="grid grid-cols-2 gap-2">
          {state.tables.map(t => (
            <button
              key={t.id}
              onClick={() => { setSelectedTable(t); setCart(new Map()); }}
              className={`table-card ${getTableStatusClass(t.status)} text-left ${selectedTable?.id === t.id ? 'ring-2 ring-blue-500' : ''}`}
            >
              <div className="font-semibold text-sm">{t.name}</div>
              <div className="text-xs opacity-70">{t.status === 0 ? '空闲' : t.status === 1 ? '占用' : '待清理'}</div>
            </button>
          ))}
        </div>
      </div>

      {/* 中间：菜单 / 订单 */}
      <div className="flex-1 overflow-y-auto p-4">
        {selectedTable ? (
          <div>
            <div className="flex items-center justify-between mb-4">
              <div>
                <h2 className="text-xl font-bold">{selectedTable.name}</h2>
                <span className="text-sm text-gray-500">
                  {selectedTable.type === 1 ? '包间' : '散台'} · 容量 {selectedTable.capacity}人
                </span>
              </div>
              <button onClick={() => { setSelectedTable(null); setCart(new Map()); }} className="btn btn-sm btn-danger">
                关闭
              </button>
            </div>

            {/* 当前餐桌的订单 */}
            {tableOrders(selectedTable.id).map(o => (
              <div key={o.id} className="bg-white rounded-lg shadow-sm p-3 mb-3 border">
                <div className="flex items-center justify-between mb-2">
                  <span className="font-bold">订单 #{o.id}</span>
                  <span className={`status-badge ${getOrderStatusClass(o.status)}`}>{STATUS_NAMES[o.status]}</span>
                </div>
                <div className="space-y-1">
                  {o.items.map((item, idx) => (
                    <div key={idx} className="flex items-center justify-between text-sm py-1 border-b border-gray-100 last:border-0">
                      <div className="flex items-center gap-2">
                        <span className={`status-badge ${getOrderStatusClass(item.status)}`}>{STATUS_NAMES[item.status]}</span>
                        <span>{item.menuItemName} x{item.quantity}</span>
                        {item.note && <span className="text-red-500 text-xs">({item.note})</span>}
                      </div>
                      <div className="flex gap-1">
                        {item.status === 5 && (
                          <button onClick={() => handleDeliver(o.id, idx)} className="btn btn-sm btn-success">确认送达</button>
                        )}
                        {item.status < 3 && (
                          <button onClick={() => handleUrge(o.id)} className="btn btn-sm btn-warn">催单</button>
                        )}
                        {item.status < 6 && item.status !== 9 && item.status !== 10 && (
                          <button onClick={() => handleCancel(o.id, idx)} className="btn btn-sm btn-danger">取消</button>
                        )}
                        {item.status === 5 && (
                          <button onClick={() => handleDamage(o.id, idx)} className="btn btn-sm btn-danger">报损</button>
                        )}
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            ))}

            {/* 菜单点菜 */}
            <h3 className="font-bold text-sm text-gray-600 mb-2 mt-4">菜单</h3>
            <div className="grid grid-cols-2 gap-2">
              {state.menu.map(m => (
                <button
                  key={m.id}
                  onClick={() => m.available && addToCart(m)}
                  disabled={!m.available}
                  className={`p-3 rounded-lg border text-left transition-all ${m.available ? 'bg-white hover:shadow cursor-pointer' : 'bg-gray-100 opacity-50 cursor-not-allowed'}`}
                >
                  <div className="font-semibold text-sm">{m.name}</div>
                  <div className="flex justify-between text-xs text-gray-500">
                    <span>{m.category}</span>
                    <span>¥{m.price.toFixed(0)}</span>
                  </div>
                </button>
              ))}
            </div>
          </div>
        ) : (
          <div className="flex items-center justify-center h-full text-gray-400">请选择一张餐桌开始</div>
        )}
      </div>

      {/* 右侧：购物车 */}
      {selectedTable && (
        <div className="w-80 bg-white border-l p-4 flex flex-col">
          <h3 className="font-bold text-sm text-gray-600 mb-2">购物车</h3>
          <div className="flex-1 overflow-y-auto space-y-2">
            {Array.from(cart.values()).map(({ item, quantity }) => (
              <div key={item.id} className="flex items-center justify-between py-1 border-b border-gray-100">
                <div>
                  <div className="text-sm font-medium">{item.name}</div>
                  <div className="text-xs text-gray-500">¥{item.price.toFixed(0)}</div>
                </div>
                <div className="flex items-center gap-2">
                  <button onClick={() => removeFromCart(item.id)} className="w-6 h-6 rounded-full bg-gray-200 flex items-center justify-center text-sm hover:bg-gray-300">-</button>
                  <span className="w-6 text-center text-sm">{quantity}</span>
                  <button onClick={() => addToCart(item)} className="w-6 h-6 rounded-full bg-blue-100 flex items-center justify-center text-sm hover:bg-blue-200">+</button>
                </div>
              </div>
            ))}
          </div>
          <div className="border-t pt-3 mt-3">
            <div className="flex justify-between font-bold mb-3">
              <span>合计</span>
              <span>¥{cartTotal.toFixed(0)}</span>
            </div>
            <button onClick={placeOrder} disabled={cart.size === 0} className="btn btn-primary w-full">
              下单
            </button>
          </div>
        </div>
      )}
    </div>
  );
}