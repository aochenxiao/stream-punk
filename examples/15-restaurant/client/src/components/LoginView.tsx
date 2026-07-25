import { useState } from 'react';

const ROLES = [
  { id: 7, name: '服务员', icon: '🍽️', desc: '餐桌服务与点菜' },
  { id: 5, name: '切配员', icon: '🔪', desc: '食材切配加工' },
  { id: 4, name: '厨师', icon: '👨‍🍳', desc: '菜品烹饪' },
  { id: 3, name: '主厨', icon: '👨‍🍳', desc: '厨房管理' },
  { id: 6, name: '传菜员', icon: '🏃', desc: '菜品传送' },
  { id: 2, name: '收银员', icon: '💰', desc: '结账收款' },
  { id: 1, name: '大堂经理', icon: '📋', desc: '厅面管理' },
  { id: 8, name: '仓库管理员', icon: '📦', desc: '库存管理' },
  { id: 9, name: '顾客', icon: '👤', desc: '查看菜品进度' },
  { id: 0, name: '老板', icon: '👔', desc: '经营报表与概览' },
];

interface Props { onLogin: (role: number, name: string) => void; status: string; }

export default function LoginView({ onLogin, status }: Props) {
  const [selectedRole, setSelectedRole] = useState<number | null>(null);
  const [inputName, setInputName] = useState('');

  const handleSubmit = () => {
    if (selectedRole !== null && inputName.trim()) {
      onLogin(selectedRole, inputName.trim());
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-blue-50 to-indigo-100">
      <div className="bg-white rounded-2xl shadow-xl p-8 w-full max-w-2xl">
        <h1 className="text-2xl font-bold text-center text-gray-800 mb-2">餐厅管理系统</h1>
        <p className="text-center text-gray-500 mb-6">选择角色开始</p>
        <div className={`text-center mb-4 text-sm ${status === 'Connected' ? 'text-green-600' : 'text-red-500'}`}>
          服务器: {status === 'Connected' ? '已连接' : status === 'Disconnected' ? '未连接' : status}
        </div>
        <div className="grid grid-cols-3 gap-3 mb-6">
          {ROLES.map(r => (
            <button
              key={r.id}
              onClick={() => setSelectedRole(r.id)}
              className={`p-4 rounded-xl border-2 transition-all text-center ${
                selectedRole === r.id ? 'border-blue-500 bg-blue-50 shadow-md' : 'border-gray-200 hover:border-gray-300'
              }`}
            >
              <div className="text-3xl mb-1">{r.icon}</div>
              <div className="font-semibold text-sm">{r.name}</div>
              <div className="text-xs text-gray-400">{r.desc}</div>
            </button>
          ))}
        </div>
        <div className="flex gap-3">
          <input
            type="text"
            placeholder={selectedRole === 9 ? '请输入桌号（如 A01）' : '请输入姓名'}
            value={inputName}
            onChange={e => setInputName(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && handleSubmit()}
            className="flex-1 px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
          <button
            onClick={handleSubmit}
            disabled={selectedRole === null || !inputName.trim() || status !== '已连接'}
            className="btn btn-primary px-8"
          >
            登录
          </button>
        </div>
      </div>
    </div>
  );
}