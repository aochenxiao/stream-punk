import { ServerState, Order, OrderItem } from '../stream-punk-data';
import { RestaurantWS } from '../ws';

const STATUS_NAMES = ['待处理','切配中','待烹饪','烹饪中','待出餐','送餐中','已送达','已完成','重做','已损坏','已取消','催单'];
const STATUS_COLORS: Record<number, string> = {
  0: 'bg-gray-200 text-gray-700',
  1: 'bg-yellow-100 text-yellow-800',
  2: 'bg-orange-100 text-orange-800',
  3: 'bg-red-100 text-red-800',
  4: 'bg-green-100 text-green-800',
  5: 'bg-blue-100 text-blue-800',
  6: 'bg-indigo-100 text-indigo-800',
  7: 'bg-gray-100 text-gray-500',
  8: 'bg-purple-100 text-purple-800',
  9: 'bg-red-200 text-red-900',
  10: 'bg-gray-300 text-gray-600',
  11: 'bg-red-500 text-white',
};

interface Props { state: ServerState; ws: RestaurantWS; tableId: number; }

function progressBar(status: number): number {
  // Map status to progress percentage (0-100)
  if (status === 0) return 5;
  if (status === 1) return 20;
  if (status === 2) return 35;
  if (status === 3) return 55;
  if (status === 4) return 75;
  if (status === 5) return 85;
  if (status === 6) return 95;
  if (status === 7) return 100;
  if (status === 8) return 10;
  if (status === 9) return 0;
  if (status === 10) return 0;
  if (status === 11) return 0;
  return 0;
}

export default function CustomerView({ state, ws, tableId }: Props) {
  const table = state.tables.find(t => t.id === tableId);
  const tableOrders = state.orders.filter(
    o => o.tableId === tableId && o.status !== 7 && o.status !== 10 && o.status !== 9
  );

  const handleUrge = (orderId: number) => {
    ws.urgeDish(orderId);
  };

  if (!table) {
    return (
      <div className="flex items-center justify-center h-[calc(100vh-52px)] text-gray-400">
        未找到对应餐桌，请联系服务员确认桌号
      </div>
    );
  }

  return (
    <div className="p-6 max-w-3xl mx-auto">
      {/* 餐桌信息 */}
      <div className="bg-white rounded-xl shadow-sm p-6 mb-6 border">
        <div className="flex items-center justify-between">
          <div>
            <h1 className="text-2xl font-bold text-gray-800">{table.name}</h1>
            <p className="text-sm text-gray-500 mt-1">
              {table.type === 1 ? '包间' : '散台'} · {table.capacity}人
            </p>
          </div>
          <div className="text-right">
            <div className="text-sm text-gray-500">当前订单</div>
            <div className="text-3xl font-bold text-blue-600">{tableOrders.length}</div>
          </div>
        </div>
      </div>

      {/* 订单列表 */}
      {tableOrders.length === 0 ? (
        <div className="text-center py-16 text-gray-400">
          <div className="text-5xl mb-4">🍽️</div>
          <p className="text-lg">暂无进行中的订单</p>
          <p className="text-sm mt-2">请呼叫服务员点菜</p>
        </div>
      ) : (
        <div className="space-y-6">
          {tableOrders.map(order => (
            <div key={order.id} className="bg-white rounded-xl shadow-sm border overflow-hidden">
              {/* 订单头部 */}
              <div className="px-6 py-4 bg-gray-50 border-b flex items-center justify-between">
                <div>
                  <span className="font-bold text-gray-800">订单 #{order.id}</span>
                  <span className="text-sm text-gray-500 ml-2">
                    共 {order.items.length} 个菜品
                  </span>
                </div>
                <div className="text-right">
                  <div className="text-xs text-gray-400">预估金额</div>
                  <div className="font-bold text-lg text-gray-800">¥{order.totalPrice.toFixed(0)}</div>
                </div>
              </div>

              {/* 菜品列表 */}
              <div className="divide-y">
                {order.items.map((item, idx) => {
                  const pct = progressBar(item.status);
                  const isUrgent = item.status === 11;
                  const canUrge = item.status > 0 && item.status < 6 && item.status !== 9 && item.status !== 10;
                  return (
                    <div key={idx} className="px-6 py-4">
                      <div className="flex items-start justify-between mb-2">
                        <div className="flex-1">
                          <div className="flex items-center gap-2">
                            <span className="font-semibold text-gray-800">{item.menuItemName}</span>
                            <span className="text-sm text-gray-500">x{item.quantity}</span>
                          </div>
                          {item.note && (
                            <p className="text-xs text-red-500 mt-1">备注：{item.note}</p>
                          )}
                        </div>
                        <div className="flex items-center gap-2">
                          <span className={`text-xs px-2 py-1 rounded-full font-medium ${STATUS_COLORS[item.status] || 'bg-gray-100'}`}>
                            {STATUS_NAMES[item.status] || '未知'}
                          </span>
                          {canUrge && (
                            <button
                              onClick={() => handleUrge(order.id)}
                              className="text-xs px-2 py-1 rounded bg-red-50 text-red-600 hover:bg-red-100 border border-red-200 transition-colors"
                            >
                              催单
                            </button>
                          )}
                        </div>
                      </div>

                      {/* 进度条 */}
                      <div className="w-full bg-gray-200 rounded-full h-2">
                        <div
                          className={`h-2 rounded-full transition-all duration-500 ${
                            item.status === 9 ? 'bg-red-500' :
                            item.status === 8 ? 'bg-purple-500' :
                            pct >= 100 ? 'bg-green-500' :
                            pct >= 75 ? 'bg-blue-500' :
                            pct >= 50 ? 'bg-yellow-500' :
                            'bg-gray-400'
                          }`}
                          style={{ width: `${pct}%` }}
                        />
                      </div>

                      {/* 进度节点 */}
                      <div className="flex justify-between mt-1 text-[10px] text-gray-400">
                        <span className={pct >= 5 ? 'text-blue-500 font-medium' : ''}>待处理</span>
                        <span className={pct >= 20 ? 'text-blue-500 font-medium' : ''}>切配</span>
                        <span className={pct >= 35 ? 'text-blue-500 font-medium' : ''}>待炒</span>
                        <span className={pct >= 55 ? 'text-blue-500 font-medium' : ''}>烹饪</span>
                        <span className={pct >= 75 ? 'text-blue-500 font-medium' : ''}>出餐</span>
                        <span className={pct >= 85 ? 'text-blue-500 font-medium' : ''}>送餐</span>
                        <span className={pct >= 95 ? 'text-green-500 font-medium' : ''}>上桌</span>
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}