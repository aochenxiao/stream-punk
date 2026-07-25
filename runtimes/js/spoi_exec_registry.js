// ============================================================
// SPOI Executor 类型注册表（自动生成，请勿手动修改）
// 由 sp-gen spoi-js-exec 从 C++ 元数据生成
// ============================================================

// 格式: { TypeName: [field1, field2, ...] }
// 字段顺序与 UseData 宏中声明顺序一致

// 同时支持 CommonJS 和 ES Module
const TYPE_REGISTRY = {
  SpoiTestPlayer: ["name", "hp", "level", "posX"],
  SpoiTestState: ["tick", "currentMap", "players"],
  SpoiItem: ["name", "value"],
  SpoiInventory: ["items", "equipped", "gold"],
  SpoiCharacter: ["name", "hp", "inventory", "weapon", "petLevel"],
  SpoiWorld: ["worldName", "tick", "characters"]
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { TYPE_REGISTRY };
}
