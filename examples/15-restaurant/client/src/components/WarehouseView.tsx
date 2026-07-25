import { useState } from 'react';
import { ServerState, Ingredient, MenuItem } from '../stream-punk-data';

interface Props { state: ServerState; }

function getStockColor(stock: number, minStock: number): string {
  if (stock <= 0) return 'text-red-600';
  if (stock <= minStock) return 'text-yellow-600';
  return 'text-green-600';
}

export default function WarehouseView({ state }: Props) {
  const [tab, setTab] = useState<'stock' | 'menu' | 'recipes'>('stock');

  return (
    <div className="p-4">
      <div className="flex gap-3 mb-4">
        <button onClick={() => setTab('stock')} className={`btn ${tab === 'stock' ? 'btn-primary' : 'bg-gray-200'}`}>
          库存 ({state.ingredients.length})
        </button>
        <button onClick={() => setTab('menu')} className={`btn ${tab === 'menu' ? 'btn-primary' : 'bg-gray-200'}`}>
          菜品可用性 ({state.menu.length})
        </button>
        <button onClick={() => setTab('recipes')} className={`btn ${tab === 'recipes' ? 'btn-primary' : 'bg-gray-200'}`}>
          配方 ({state.recipes.length})
        </button>
      </div>

      {tab === 'stock' && (
        <div>
          <div className="grid grid-cols-3 gap-4 mb-4">
            <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
              <div className="text-3xl font-bold text-green-600">
                {state.ingredients.filter(i => i.stock > i.minStock).length}
              </div>
              <div className="text-sm text-gray-500">库存充足</div>
            </div>
            <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
              <div className="text-3xl font-bold text-yellow-600">
                {state.ingredients.filter(i => i.stock > 0 && i.stock <= i.minStock).length}
              </div>
              <div className="text-sm text-gray-500">库存偏低</div>
            </div>
            <div className="bg-white rounded-lg shadow-sm p-4 border text-center">
              <div className="text-3xl font-bold text-red-600">
                {state.ingredients.filter(i => i.stock <= 0).length}
              </div>
              <div className="text-sm text-gray-500">已售罄</div>
            </div>
          </div>

          <div className="bg-white rounded-lg shadow-sm border">
            <table className="w-full">
              <thead>
                <tr className="border-b text-left text-sm text-gray-500">
                  <th className="p-3">编号</th>
                  <th className="p-3">名称</th>
                  <th className="p-3">库存</th>
                  <th className="p-3">最低库存</th>
                  <th className="p-3">单位</th>
                  <th className="p-3">状态</th>
                </tr>
              </thead>
              <tbody>
                {state.ingredients.map(ing => {
                  const status = ing.stock <= 0 ? '已售罄' : ing.stock <= ing.minStock ? '库存偏低' : '正常';
                  const statusColor = ing.stock <= 0 ? 'bg-red-100 text-red-700' : ing.stock <= ing.minStock ? 'bg-yellow-100 text-yellow-700' : 'bg-green-100 text-green-700';
                  return (
                    <tr key={ing.id} className="border-b last:border-0 hover:bg-gray-50">
                      <td className="p-3 text-gray-500">{ing.id}</td>
                      <td className="p-3 font-medium">{ing.name}</td>
                      <td className={`p-3 font-bold ${getStockColor(ing.stock, ing.minStock)}`}>{ing.stock.toFixed(1)}</td>
                      <td className="p-3 text-gray-500">{ing.minStock.toFixed(1)}</td>
                      <td className="p-3 text-gray-500">{ing.unit}</td>
                      <td className="p-3">
                        <span className={`status-badge ${statusColor}`}>{status}</span>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        </div>
      )}

      {tab === 'menu' && (
        <div className="grid grid-cols-2 gap-3">
          {state.menu.map(m => (
            <div key={m.id} className={`bg-white rounded-lg shadow-sm p-3 border ${m.available ? 'border-green-200' : 'border-red-200 opacity-60'}`}>
              <div className="flex items-center justify-between">
                <div>
                  <div className="font-bold">{m.name}</div>
                  <div className="text-xs text-gray-500">{m.category}</div>
                </div>
                <div className="text-right">
                  <div className="font-bold text-lg">¥{m.price.toFixed(0)}</div>
                  <span className={`status-badge ${m.available ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'}`}>
                    {m.available ? '可售' : '售罄'}
                  </span>
                </div>
              </div>
              {/* 配方用料 */}
              {(() => {
                const recipe = state.recipes.find(r => r.menuItemId === m.id);
                if (!recipe) return null;
                return (
                  <div className="mt-2 pt-2 border-t text-xs text-gray-500">
                    {recipe.items.map(ri => (
                      <span key={ri.ingredientId} className="mr-2">
                        {ri.ingredientName}: {ri.quantity}{state.ingredients.find(i => i.id === ri.ingredientId)?.unit || ''}
                      </span>
                    ))}
                  </div>
                );
              })()}
            </div>
          ))}
        </div>
      )}

      {tab === 'recipes' && (
        <div className="space-y-3">
          {state.recipes.map(r => {
            const menuItem = state.menu.find(m => m.id === r.menuItemId);
            return (
              <div key={r.menuItemId} className="bg-white rounded-lg shadow-sm p-3 border">
                <div className="font-bold mb-2">
                  {menuItem?.name || `菜品 #${r.menuItemId}`} 配方
                </div>
                <div className="space-y-1">
                  {r.items.map(ri => {
                    const ing = state.ingredients.find(i => i.id === ri.ingredientId);
                    const sufficient = ing ? ing.stock >= ri.quantity : false;
                    return (
                      <div key={ri.ingredientId} className="flex items-center justify-between text-sm py-1 border-b border-gray-100 last:border-0">
                        <span>{ri.ingredientName}</span>
                        <div className="flex items-center gap-2">
                          <span className="text-gray-500">{ri.quantity} {ing?.unit || ''}</span>
                          <span className={`text-xs ${sufficient ? 'text-green-600' : 'text-red-600'}`}>
                            {ing ? `(库存: ${ing.stock.toFixed(1)})` : '无'}
                          </span>
                        </div>
                      </div>
                    );
                  })}
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}