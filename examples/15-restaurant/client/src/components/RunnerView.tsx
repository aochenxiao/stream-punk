import { ServerState, Order, OrderItem } from '../stream-punk-data';
import { RestaurantWS } from '../ws';

const STATUS_NAMES = ['待处理','切配中','待烹饪','烹饪中','待出餐','送餐中','已送达','已完成','重做','已损坏','已取消','催单'];

interface Props { state: ServerState; ws: RestaurantWS; }

function getOrderStatusClass(status: number): string {
  const names = ['Pending','Cutting','ReadyToCook','Cooking','ReadyToServe','InTransit','Delivered','Completed','Remake','Damaged','Cancelled','Urgent'];
  return `status-${names[status] || 'Pending'}`;
}

export default function RunnerView({ state, ws }: Props) {
  const readyItems: { order: Order; item: OrderItem; itemIndex: number }[] = [];
  const transitItems: { order: Order; item: OrderItem; itemIndex: number }[] = [];

  for (const order of state.orders) {
    if (order.status === 7 || order.status === 10 || order.status === 9) continue;
    for (let i = 0; i < order.items.length; i++) {
      const item = order.items[i];
      if (item.status === 10 || item.status === 9) continue;
      if (item.status === 4) {
        readyItems.push({ order, item, itemIndex: i });
      } else if (item.status === 5) {
        transitItems.push({ order, item, itemIndex: i });
      }
    }
  }

  const handlePickUp = (orderId: number, itemIndex: number) => {
    ws.updateStatus(orderId, itemIndex, 5, '');
  };

  const handleDamaged = (orderId: number, itemIndex: number) => {
    ws.updateStatus(orderId, itemIndex, 9, '送餐途中损坏');
  };

  const handleWrongTable = (orderId: number, itemIndex: number) => {
    ws.updateStatus(orderId, itemIndex, 4, '送错桌号退回');
  };

  return (
    <div className="p-4">
      <div className="grid grid-cols-2 gap-4">
        {/* 待出餐 */}
        <div>
          <h2 className="font-bold text-lg mb-3 text-green-700">
            待出餐 ({readyItems.length})
          </h2>
          <div className="space-y-2">
            {readyItems.length === 0 && (
              <div className="text-gray-400 text-center py-8">暂无待出餐的菜品</div>
            )}
            {readyItems.map(({ order, item, itemIndex }) => (
              <div key={`${order.id}-${itemIndex}`} className="bg-green-50 rounded-lg shadow-sm p-3 border border-green-200">
                <div className="flex items-center justify-between">
                  <div>
                    <div className="font-bold text-lg">{order.tableName}</div>
                    <div className="text-xs text-gray-500">订单 #{order.id}</div>
                    <div className="text-sm mt-1">{item.menuItemName} x{item.quantity}</div>
                  </div>
                  <button onClick={() => handlePickUp(order.id, itemIndex)} className="btn btn-primary">
                    取餐
                  </button>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* 送餐中 */}
        <div>
          <h2 className="font-bold text-lg mb-3 text-blue-700">
            送餐中 ({transitItems.length})
          </h2>
          <div className="space-y-2">
            {transitItems.length === 0 && (
              <div className="text-gray-400 text-center py-8">暂无送餐中的菜品</div>
            )}
            {transitItems.map(({ order, item, itemIndex }) => (
              <div key={`${order.id}-${itemIndex}`} className="bg-blue-50 rounded-lg shadow-sm p-3 border border-blue-200">
                <div className="flex items-center justify-between">
                  <div>
                    <div className="font-bold text-lg">{order.tableName}</div>
                    <div className="text-xs text-gray-500">订单 #{order.id}</div>
                    <div className="text-sm mt-1">{item.menuItemName} x{item.quantity}</div>
                    <span className={`status-badge ${getOrderStatusClass(item.status)}`}>{STATUS_NAMES[item.status]}</span>
                  </div>
                  <div className="flex flex-col gap-1">
                    <button onClick={() => handleDamaged(order.id, itemIndex)} className="btn btn-sm btn-danger">
                      损坏
                    </button>
                    <button onClick={() => handleWrongTable(order.id, itemIndex)} className="btn btn-sm btn-warn">
                      送错桌
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}