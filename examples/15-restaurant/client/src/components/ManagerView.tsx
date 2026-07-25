import { useState } from 'react';
import { ServerState, Order, Table } from '../stream-punk-data';
import { RestaurantWS } from '../ws';

const STATUS_NAMES = ['待处理','切配中','待烹饪','烹饪中','待出餐','送餐中','已送达','已完成','重做','已损坏','已取消','催单'];

interface Props { state: ServerState; ws: RestaurantWS; }

function getOrderStatusClass(status: number): string {
  const names = ['Pending','Cutting','ReadyToCook','Cooking','ReadyToServe','InTransit','Delivered','Completed','Remake','Damaged','Cancelled','Urgent'];
  return `status-${names[status] || 'Pending'}`;
}

export default function ManagerView({ state, ws }: Props) {
  const [tab, setTab] = useState<'overview' | 'change-table' | 'damage'>('overview');

  const [fromTableId, setFromTableId] = useState<number | null>(null);
  const [toTableId, setToTableId] = useState<number | null>(null);

  const occupiedTables = state.tables.filter(t => t.status === 1);
  const availableTables = state.tables.filter(t => t.status === 0);

  const allActiveOrders = state.orders.filter(o => o.status !== 7 && o.status !== 10 && o.status !== 9);

  const handleChangeTable = () => {
    if (fromTableId !== null && toTableId !== null) {
      ws.changeTable(fromTableId, toTableId);
      setFromTableId(null);
      setToTableId(null);
    }
  };

  const handleDamageReport = (orderId: number, itemIndex: number) => {
    ws.updateStatus(orderId, itemIndex, 9, '经理报损');
  };

  const handleRemake = (orderId: number, itemIndex: number) => {
    ws.updateStatus(orderId, itemIndex, 8, '经理要求重做');
  };

  const handleCancelOrder = (orderId: number, itemIndex: number) => {
    ws.updateStatus(orderId, itemIndex, 10, '经理取消');
  };

  return (
    <div className="p-4">
      <div className="flex gap-3 mb-4">
        <button onClick={() => setTab('overview')} className={`btn ${tab === 'overview' ? 'btn-primary' : 'bg-gray-200'}`}>
          厅面概览
        </button>
        <button onClick={() => setTab('change-table')} className={`btn ${tab === 'change-table' ? 'btn-primary' : 'bg-gray-200'}`}>
          换桌
        </button>
        <button onClick={() => setTab('damage')} className={`btn ${tab === 'damage' ? 'btn-primary' : 'bg-gray-200'}`}>
          报损/取消
        </button>
      </div>

      {tab === 'overview' && (
        <div>
          <div className="grid grid-cols-3 gap-4 mb-6">
            <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
              <div className="text-3xl font-bold text-green-600">{state.tables.filter(t => t.status === 0).length}</div>
              <div className="text-sm text-gray-500">空闲</div>
            </div>
            <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
              <div className="text-3xl font-bold text-red-600">{state.tables.filter(t => t.status === 1).length}</div>
              <div className="text-sm text-gray-500">占用</div>
            </div>
            <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
              <div className="text-3xl font-bold text-yellow-600">{state.tables.filter(t => t.status === 2).length}</div>
              <div className="text-sm text-gray-500">待清理</div>
            </div>
          </div>

          <h3 className="font-bold text-lg mb-3">全部活跃订单</h3>
          <div className="space-y-3">
            {allActiveOrders.map(o => (
              <div key={o.id} className="bg-white rounded-lg shadow-sm p-3 border">
                <div className="flex items-center justify-between mb-2">
                  <span className="font-bold">{o.tableName} - 订单 #{o.id}</span>
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
                      <div className="text-gray-500 text-xs">¥{(o.totalPrice / Math.max(o.items.length, 1)).toFixed(0)}</div>
                    </div>
                  ))}
                </div>
                <div className="text-right text-sm font-bold mt-1">合计: ¥{o.totalPrice.toFixed(0)}</div>
              </div>
            ))}
            {allActiveOrders.length === 0 && (
              <div className="text-gray-400 text-center py-8">暂无活跃订单</div>
            )}
          </div>
        </div>
      )}

      {tab === 'change-table' && (
        <div className="max-w-lg">
          <h3 className="font-bold text-lg mb-4">换桌</h3>
          <div className="space-y-4">
            <div>
              <label className="block text-sm font-medium text-gray-600 mb-1">从（已占用）</label>
              <div className="grid grid-cols-3 gap-2">
                {occupiedTables.map(t => (
                  <button
                    key={t.id}
                    onClick={() => setFromTableId(t.id)}
                    className={`p-2 rounded-lg border text-sm ${fromTableId === t.id ? 'border-red-500 bg-red-50' : 'bg-white'}`}
                  >
                    {t.name}
                  </button>
                ))}
              </div>
            </div>
            <div>
              <label className="block text-sm font-medium text-gray-600 mb-1">到（空闲）</label>
              <div className="grid grid-cols-3 gap-2">
                {availableTables.map(t => (
                  <button
                    key={t.id}
                    onClick={() => setToTableId(t.id)}
                    className={`p-2 rounded-lg border text-sm ${toTableId === t.id ? 'border-green-500 bg-green-50' : 'bg-white'}`}
                  >
                    {t.name}
                  </button>
                ))}
              </div>
            </div>
            <button
              onClick={handleChangeTable}
              disabled={fromTableId === null || toTableId === null}
              className="btn btn-primary"
            >
              将 {fromTableId ? state.tables.find(t => t.id === fromTableId)?.name : ''} 移到 {toTableId ? state.tables.find(t => t.id === toTableId)?.name : ''}
            </button>
          </div>
        </div>
      )}

      {tab === 'damage' && (
        <div>
          <h3 className="font-bold text-lg mb-4">报损/取消管理</h3>
          <div className="space-y-3">
            {allActiveOrders.map(o => (
              <div key={o.id} className="bg-white rounded-lg shadow-sm p-3 border">
                <div className="font-bold mb-2">{o.tableName} - 订单 #{o.id}</div>
                <div className="space-y-1">
                  {o.items.map((item, idx) => (
                    <div key={idx} className="flex items-center justify-between text-sm py-1 border-b border-gray-100 last:border-0">
                      <div className="flex items-center gap-2">
                        <span className={`status-badge ${getOrderStatusClass(item.status)}`}>{STATUS_NAMES[item.status]}</span>
                        <span>{item.menuItemName} x{item.quantity}</span>
                      </div>
                      <div className="flex gap-1">
                        {(item.status === 3 || item.status === 4) && (
                          <button onClick={() => handleRemake(o.id, idx)} className="btn btn-sm btn-warn">重做</button>
                        )}
                        {item.status !== 8 && item.status !== 9 && item.status !== 10 && item.status !== 7 && (
                          <>
                            <button onClick={() => handleDamageReport(o.id, idx)} className="btn btn-sm btn-danger">报损</button>
                            <button onClick={() => handleCancelOrder(o.id, idx)} className="btn btn-sm btn-danger">取消</button>
                          </>
                        )}
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            ))}
            {allActiveOrders.length === 0 && (
              <div className="text-gray-400 text-center py-8">暂无活跃订单</div>
            )}
          </div>
        </div>
      )}
    </div>
  );
}