import { useState } from 'react';
import { ServerState, Order, OrderItem } from '../stream-punk-data';
import { RestaurantWS } from '../ws';

const STATUS_NAMES = ['待处理','切配中','待烹饪','烹饪中','待出餐','送餐中','已送达','已完成','重做','已损坏','已取消','催单'];

interface Props { state: ServerState; ws: RestaurantWS; role: number; }

function getOrderStatusClass(status: number): string {
  const names = ['Pending','Cutting','ReadyToCook','Cooking','ReadyToServe','InTransit','Delivered','Completed','Remake','Damaged','Cancelled','Urgent'];
  return `status-${names[status] || 'Pending'}`;
}

export default function KitchenView({ state, ws, role }: Props) {
  const [tab, setTab] = useState<'cutting' | 'cooking' | 'all'>(
    role === 5 ? 'cutting' : role === 4 ? 'cooking' : 'all'
  );

  const isCutter = role === 5;
  const isChef = role === 4;
  const isHeadChef = role === 3;

  const cuttingItems: { order: Order; item: OrderItem; itemIndex: number }[] = [];
  const cookingItems: { order: Order; item: OrderItem; itemIndex: number }[] = [];
  const activeItems: { order: Order; item: OrderItem; itemIndex: number }[] = [];

  for (const order of state.orders) {
    if (order.status === 7 || order.status === 10 || order.status === 9) continue;
    for (let i = 0; i < order.items.length; i++) {
      const item = order.items[i];
      if (item.status === 10 || item.status === 9) continue;
      if (item.status === 0 || item.status === 1 || item.status === 8) {
        cuttingItems.push({ order, item, itemIndex: i });
      } else if (item.status === 2) {
        cookingItems.push({ order, item, itemIndex: i });
      } else if (item.status === 3) {
        activeItems.push({ order, item, itemIndex: i });
      }
    }
  }

  const handleStatus = (orderId: number, itemIndex: number, newStatus: number, note: string = '') => {
    ws.updateStatus(orderId, itemIndex, newStatus, note);
  };

  const CuttingBoard = () => (
    <div className="space-y-2">
      <h3 className="font-bold text-lg mb-2">切配台 ({cuttingItems.length})</h3>
      {cuttingItems.length === 0 && <div className="text-gray-400 text-center py-8">暂无需切配的菜品</div>}
      {cuttingItems.map(({ order, item, itemIndex }) => (
        <div key={`${order.id}-${itemIndex}`} className="bg-white rounded-lg shadow-sm p-3 border">
          <div className="flex items-center justify-between">
            <div>
              <span className="font-bold text-sm">{order.tableName}</span>
              <span className="text-xs text-gray-500 ml-2">#{order.id}</span>
              <div className="text-sm mt-1">{item.menuItemName} x{item.quantity}</div>
              <span className={`status-badge ${getOrderStatusClass(item.status)}`}>{STATUS_NAMES[item.status]}</span>
            </div>
            <div className="flex gap-2">
              {(item.status === 0 || item.status === 8) && (
                <button onClick={() => handleStatus(order.id, itemIndex, 1)} className="btn btn-sm btn-primary">
                  开始切配
                </button>
              )}
              {item.status === 1 && (
                <button onClick={() => handleStatus(order.id, itemIndex, 2)} className="btn btn-sm btn-success">
                  切配完成
                </button>
              )}
            </div>
          </div>
        </div>
      ))}
    </div>
  );

  const ChefQueue = () => (
    <div className="space-y-2">
      <h3 className="font-bold text-lg mb-2">待烹饪队列 ({cookingItems.length})</h3>
      {cookingItems.length === 0 && <div className="text-gray-400 text-center py-8">暂无待烹饪的菜品</div>}
      {cookingItems.map(({ order, item, itemIndex }) => {
        const isUrgent = item.status === 11;
        return (
          <div key={`${order.id}-${itemIndex}`} className={`bg-white rounded-lg shadow-sm p-3 border ${isUrgent ? 'border-red-500 animate-pulse' : ''}`}>
            <div className="flex items-center justify-between">
              <div>
                <span className="font-bold text-sm">{order.tableName}</span>
                <span className="text-xs text-gray-500 ml-2">#{order.id}</span>
                {isUrgent && <span className="status-badge status-Urgent ml-2">催单</span>}
                <div className="text-sm mt-1">{item.menuItemName} x{item.quantity}</div>
              </div>
              <div className="flex gap-2">
                <button onClick={() => handleStatus(order.id, itemIndex, 3)} className="btn btn-sm btn-primary">
                  开始烹饪
                </button>
              </div>
            </div>
          </div>
        );
      })}
    </div>
  );

  const ActiveCooking = () => (
    <div className="space-y-2">
      <h3 className="font-bold text-lg mb-2">烹饪中 ({activeItems.length})</h3>
      {activeItems.length === 0 && <div className="text-gray-400 text-center py-8">暂无烹饪中的菜品</div>}
      {activeItems.map(({ order, item, itemIndex }) => (
        <div key={`${order.id}-${itemIndex}`} className="bg-white rounded-lg shadow-sm p-3 border">
          <div className="flex items-center justify-between">
            <div>
              <span className="font-bold text-sm">{order.tableName}</span>
              <span className="text-xs text-gray-500 ml-2">#{order.id}</span>
              <div className="text-sm mt-1">{item.menuItemName} x{item.quantity}</div>
              <span className={`status-badge ${getOrderStatusClass(item.status)}`}>{STATUS_NAMES[item.status]}</span>
            </div>
            <div className="flex gap-2">
              <button onClick={() => handleStatus(order.id, itemIndex, 4)} className="btn btn-sm btn-success">
                出餐
              </button>
              {(isChef || isHeadChef) && (
                <button onClick={() => handleStatus(order.id, itemIndex, 8, '厨师要求重做')} className="btn btn-sm btn-warn">
                  重做
                </button>
              )}
            </div>
          </div>
        </div>
      ))}
    </div>
  );

  return (
    <div className="p-4">
      <div className="flex gap-3 mb-4">
        {(isCutter || isHeadChef) && (
          <button onClick={() => setTab('cutting')} className={`btn ${tab === 'cutting' ? 'btn-primary' : 'bg-gray-200'}`}>
            切配台 ({cuttingItems.length})
          </button>
        )}
        {(isChef || isHeadChef) && (
          <button onClick={() => setTab('cooking')} className={`btn ${tab === 'cooking' ? 'btn-primary' : 'bg-gray-200'}`}>
            烹饪队列 ({cookingItems.length})
          </button>
        )}
        {isHeadChef && (
          <button onClick={() => setTab('all')} className={`btn ${tab === 'all' ? 'btn-primary' : 'bg-gray-200'}`}>
            全部 ({activeItems.length})
          </button>
        )}
      </div>

      {tab === 'cutting' && <CuttingBoard />}
      {tab === 'cooking' && (
        <div className="space-y-6">
          <ChefQueue />
          <ActiveCooking />
        </div>
      )}
      {tab === 'all' && (
        <div className="space-y-6">
          <CuttingBoard />
          <ChefQueue />
          <ActiveCooking />
        </div>
      )}
    </div>
  );
}