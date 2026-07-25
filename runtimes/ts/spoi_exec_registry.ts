// ============================================================
// SPOI Executor 类型注册表（自动生成，请勿手动修改）
// 由 sp-gen spoi-ts-exec 从 C++ 元数据生成
// ============================================================

// 格式: { TypeName: [field1, field2, ...] }
// 字段顺序与 UseData 宏中声明顺序一致

import { TypeRegistry } from './spoi_executor';

export const TYPE_REGISTRY: TypeRegistry = {
  SpoiTestPlayer: ["name", "hp", "level", "posX"],
  SpoiTestState: ["tick", "currentMap", "players"],
  SpoiItem: ["name", "value"],
  SpoiInventory: ["items", "equipped", "gold"],
  SpoiCharacter: ["name", "hp", "inventory", "weapon", "petLevel"],
  SpoiWorld: ["worldName", "tick", "characters"]
};
