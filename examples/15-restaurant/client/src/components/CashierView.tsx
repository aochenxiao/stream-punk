import { useState } from 'react';
import { ServerState, Order, Table } from '../stream-punk-data';
import { RestaurantWS } from '../ws';

const STATUS_NAMES = ['待处理','切配中','待烹饪','烹饪中','待出餐','送餐中','已送达','已完成','重做','已损坏','已取消','催单'];

interface Props { state: ServerState; ws: RestaurantWS; }

function getOrderStatusClass(status: number): string {
  const names = ['Pending','Cutting','ReadyToCook','Cooking','ReadyToServe','InTransit','Delivered','Completed','Remake','Damaged','Cancelled','Urgent'];
  return `status-${names[status] || 'Pending'}`;
}

export default function CashierView({ state, ws }: Props) {
  const [selectedTableId, setSelectedTableId] = useState<number | null>(null);
  const [paymentMethod, setPaymentMethod] = useState('微信');
  const [discount, setDiscount] = useState(1.0);

  const activeTableIds = new Set<number>();
  for (const o of state.orders) {
    if (o.status !== 7 && o.status !== 10 && o.status !== 9) {
      activeTableIds.add(o.tableId);
    }
  }

  const occupiedTables = state.tables.filter(t => activeTableIds.has(t.id));

  const selectedOrders = state.orders.filter(
    o => o.tableId === selectedTableId && o.status !== 7 && o.status !== 10 && o.status !== 9
  );

  const totalPrice = selectedOrders.reduce((sum, o) => sum + o.totalPrice, 0);
  const finalPrice = totalPrice * discount;

  const handlePayment = () => {
    if (selectedTableId === null) return;
    ws.pay(selectedTableId, paymentMethod, discount);
    setSelectedTableId(null);
    setDiscount(1.0);
  };

  const handleMerge = (tableId2: number) => {
    if (selectedTableId === null) return;
    ws.mergeOrders(selectedTableId, tableId2);
  };

  const handleDirtyToAvailable = (tableId: number) => {
    const availableTables = state.tables.filter(t => t.status === 0 && t.id !== tableId);
    if (availableTables.length > 0) {
      ws.changeTable(tableId, availableTables[0].id);
    }
  };

  return (
    <div className="flex h-[calc(100vh-52px)]">
      {/* 左侧：有订单的餐桌 */}
      <div className="w-72 bg-white border-r overflow-y-auto p-3">
        <h3 className="font-bold text-sm text-gray-600 mb-2">在用餐桌</h3>
        <div className="space-y-2">
          {occupiedTables.map(t => (
            <button
              key={t.id}
              onClick={() => setSelectedTableId(t.id)}
              className={`w-full text-left p-3 rounded-lg border transition-all ${selectedTableId === t.id ? 'border-blue-500 bg-blue-50 shadow-md' : 'bg-white hover:shadow'}`}
            >
              <div className="font-bold text-sm">{t.name}</div>
              <div className="text-xs text-gray-500">
                {selectedOrders.filter(o => o.tableId === t.id).length} 个活跃订单
              </div>
            </button>
          ))}
          {occupiedTables.length === 0 && (
            <div className="text-gray-400 text-center py-8">暂无在用餐桌</div>
          )}
        </div>

        {/* 待清理餐桌 */}
        <h3 className="font-bold text-sm text-gray-600 mt-4 mb-2">待清理餐桌</h3>
        <div className="space-y-2">
          {state.tables.filter(t => t.status === 2).map(t => (
            <div key={t.id} className="p-2 rounded bg-yellow-50 border border-yellow-200">
              <div className="flex items-center justify-between">
                <span className="font-bold text-sm">{t.name}</span>
                <button onClick={() => handleDirtyToAvailable(t.id)} className="btn btn-sm btn-success">
                  清理
                </button>
              </div>
            </div>
          ))}
          {state.tables.filter(t => t.status === 2).length === 0 && (
            <div className="text-gray-400 text-xs text-center py-2">暂无待清理餐桌</div>
          )}
        </div>
      </div>

      {/* 右侧：结账 */}
      <div className="flex-1 p-4 overflow-y-auto">
        {selectedTableId === null ? (
          <div className="flex items-center justify-center h-full text-gray-400">请选择一张餐桌进行结账</div>
        ) : (
          <div>
            <h2 className="text-xl font-bold mb-4">
              餐桌 {state.tables.find(t => t.id === selectedTableId)?.name || selectedTableId}
            </h2>

            {/* 订单列表 */}
            <div className="space-y-3 mb-6">
              {selectedOrders.map(o => (
                <div key={o.id} className="bg-white rounded-lg shadow-sm p-3 border">
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
                        </div>
                      </div>
                    ))}
                  </div>
                  <div className="text-right text-sm font-bold mt-1">¥{o.totalPrice.toFixed(0)}</div>
                </div>
              ))}
            </div>

            {/* 结账控制 */}
            <div className="bg-white rounded-lg shadow-sm p-4 border">
              <div className="grid grid-cols-2 gap-4 mb-4">
                <div>
                  <label className="block text-sm font-medium text-gray-600 mb-1">支付方式</label>
                  <select value={paymentMethod} onChange={e => setPaymentMethod(e.target.value)} className="w-full border rounded px-3 py-2">
                    <option value="微信">微信</option>
                    <option value="支付宝">支付宝</option>
                    <option value="现金">现金</option>
                    <option value="刷卡">刷卡</option>
                  </select>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-600 mb-1">折扣</label>
                  <select value={discount} onChange={e => setDiscount(parseFloat(e.target.value))} className="w-full border rounded px-3 py-2">
                    <option value={1.0}>不打折 (100%)</option>
                    <option value={0.95}>95折</option>
                    <option value={0.9}>9折</option>
                    <option value={0.85}>85折</option>
                    <option value={0.8}>8折</option>
                    <option value={0.7}>7折</option>
                  </select>
                </div>
              </div>

              <div className="border-t pt-4">
                <div className="flex justify-between text-lg mb-1">
                  <span>小计</span>
                  <span>¥{totalPrice.toFixed(0)}</span>
                </div>
                {discount < 1.0 && (
                  <div className="flex justify-between text-sm text-red-500 mb-1">
                    <span>折扣 ({(discount * 100).toFixed(0)}%)</span>
                    <span>-¥{(totalPrice * (1 - discount)).toFixed(0)}</span>
                  </div>
                )}
                <div className="flex justify-between text-xl font-bold mt-2">
                  <span>合计</span>
                  <span className="text-blue-600">¥{finalPrice.toFixed(0)}</span>
                </div>
              </div>

              <div className="flex gap-3 mt-4">
                <button onClick={handlePayment} className="btn btn-primary flex-1 text-lg py-3">
                  确认结账
                </button>
              </div>
            </div>

            {/* 合并订单 */}
            <div className="mt-4 bg-white rounded-lg shadow-sm p-4 border">
              <h3 className="font-bold text-sm text-gray-600 mb-2">合并到其他餐桌</h3>
              <div className="flex gap-2 flex-wrap">
                {occupiedTables.filter(t => t.id !== selectedTableId).map(t => (
                  <button key={t.id} onClick={() => handleMerge(t.id)} className="btn btn-sm btn-warn">
                    合并到 {t.name}
                  </button>
                ))}
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}