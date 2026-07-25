import { ServerState, Order } from '../stream-punk-data';

const STATUS_NAMES = ['待处理','切配中','待烹饪','烹饪中','待出餐','送餐中','已送达','已完成','重做','已损坏','已取消','催单'];

interface Props { state: ServerState; }

export default function BossView({ state }: Props) {
  const totalOrders = state.orders.length;
  const completedOrders = state.orders.filter(o => o.status === 7).length;
  const activeOrders = state.orders.filter(o => o.status !== 7 && o.status !== 10 && o.status !== 9).length;
  const cancelledOrders = state.orders.filter(o => o.status === 10).length;
  const damagedOrders = state.orders.filter(o => o.status === 9).length;

  const totalRevenue = state.orders
    .filter(o => o.status === 7)
    .reduce((sum, o) => sum + o.totalPrice * o.discount, 0);

  const pendingRevenue = state.orders
    .filter(o => o.status !== 7 && o.status !== 10 && o.status !== 9)
    .reduce((sum, o) => sum + o.totalPrice, 0);

  const paymentStats = new Map<string, { count: number; total: number }>();
  for (const o of state.orders) {
    if (o.status === 7 && o.paymentMethod) {
      const existing = paymentStats.get(o.paymentMethod) || { count: 0, total: 0 };
      existing.count++;
      existing.total += o.totalPrice * o.discount;
      paymentStats.set(o.paymentMethod, existing);
    }
  }

  const lowStockIngredients = state.ingredients.filter(ing => ing.stock <= ing.minStock);

  const occupiedTables = state.tables.filter(t => t.status === 1).length;
  const totalTables = state.tables.length;
  const utilization = totalTables > 0 ? (occupiedTables / totalTables * 100) : 0;

  return (
    <div className="p-4">
      <h2 className="text-xl font-bold mb-4">经营仪表盘</h2>

      {/* 统计卡片 */}
      <div className="grid grid-cols-4 gap-4 mb-6">
        <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
          <div className="text-3xl font-bold text-blue-600">{totalOrders}</div>
          <div className="text-sm text-gray-500">总订单</div>
        </div>
        <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
          <div className="text-3xl font-bold text-green-600">{completedOrders}</div>
          <div className="text-sm text-gray-500">已完成</div>
        </div>
        <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
          <div className="text-3xl font-bold text-yellow-600">{activeOrders}</div>
          <div className="text-sm text-gray-500">进行中</div>
        </div>
        <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
          <div className="text-3xl font-bold text-red-600">{cancelledOrders + damagedOrders}</div>
          <div className="text-sm text-gray-500">已取消/损坏</div>
        </div>
      </div>

      {/* 营收 */}
      <div className="grid grid-cols-3 gap-4 mb-6">
        <div className="bg-white rounded-lg shadow-sm p-4 border">
          <div className="text-sm text-gray-500">总营收</div>
          <div className="text-2xl font-bold text-green-600">¥{totalRevenue.toFixed(0)}</div>
        </div>
        <div className="bg-white rounded-lg shadow-sm p-4 border">
          <div className="text-sm text-gray-500">待收款项</div>
          <div className="text-2xl font-bold text-yellow-600">¥{pendingRevenue.toFixed(0)}</div>
        </div>
        <div className="bg-white rounded-lg shadow-sm p-4 border">
          <div className="text-sm text-gray-500">翻台率</div>
          <div className="text-2xl font-bold text-blue-600">{utilization.toFixed(0)}%</div>
          <div className="text-xs text-gray-400">{occupiedTables}/{totalTables} 桌</div>
        </div>
      </div>

      {/* 支付方式分布 */}
      <div className="mb-6">
        <h3 className="font-bold text-lg mb-2">各支付方式营收</h3>
        <div className="grid grid-cols-4 gap-4">
          {Array.from(paymentStats.entries()).map(([method, stats]) => (
            <div key={method} className="bg-white rounded-lg shadow-sm p-3 border text-center">
              <div className="font-bold text-sm">{method}</div>
              <div className="text-lg font-bold">¥{stats.total.toFixed(0)}</div>
              <div className="text-xs text-gray-500">{stats.count} 单</div>
            </div>
          ))}
          {paymentStats.size === 0 && (
            <div className="col-span-4 text-gray-400 text-center py-4">暂无支付数据</div>
          )}
        </div>
      </div>

      {/* 库存预警 */}
      <div className="mb-6">
        <h3 className="font-bold text-lg mb-2">库存预警</h3>
        <div className="bg-white rounded-lg shadow-sm border">
          {lowStockIngredients.length > 0 ? (
            <table className="w-full">
              <thead>
                <tr className="border-b text-left text-sm text-gray-500">
                  <th className="p-3">原材料</th>
                  <th className="p-3">库存</th>
                  <th className="p-3">最低库存</th>
                  <th className="p-3">单位</th>
                </tr>
              </thead>
              <tbody>
                {lowStockIngredients.map(ing => (
                  <tr key={ing.id} className="border-b last:border-0">
                    <td className="p-3 font-medium">{ing.name}</td>
                    <td className="p-3 text-red-600 font-bold">{ing.stock.toFixed(1)}</td>
                    <td className="p-3 text-gray-500">{ing.minStock.toFixed(1)}</td>
                    <td className="p-3 text-gray-500">{ing.unit}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            <div className="p-4 text-gray-400 text-center">库存充足</div>
          )}
        </div>
      </div>

      {/* 全部订单 */}
      <div>
        <h3 className="font-bold text-lg mb-2">全部订单 ({totalOrders})</h3>
        <div className="space-y-2">
          {state.orders.slice().reverse().map(o => (
            <div key={o.id} className="bg-white rounded-lg shadow-sm p-3 border">
              <div className="flex items-center justify-between">
                <div>
                  <span className="font-bold">订单 #{o.id}</span>
                  <span className="text-sm text-gray-500 ml-2">{o.tableName}</span>
                  <span className="text-sm text-gray-400 ml-2">{o.items.length} 道菜</span>
                </div>
                <div className="flex items-center gap-3">
                  <span className="text-sm font-bold">¥{(o.totalPrice * o.discount).toFixed(0)}</span>
                  {o.paymentMethod && <span className="text-xs text-gray-500">{o.paymentMethod}</span>}
                  <span className={`status-badge ${STATUS_NAMES[o.status] === '已完成' ? 'status-Completed' : STATUS_NAMES[o.status] === '已取消' ? 'status-Cancelled' : 'status-Pending'}`}>
                    {STATUS_NAMES[o.status]}
                  </span>
                </div>
              </div>
            </div>
          ))}
          {state.orders.length === 0 && (
            <div className="text-gray-400 text-center py-8">暂无订单</div>
          )}
        </div>
      </div>
    </div>
  );
}