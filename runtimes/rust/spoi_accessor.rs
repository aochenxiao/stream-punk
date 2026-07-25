// ============================================================
// SPOI Accessor — Rust 类型特化访问器（自动生成）
// 由 sp-gen spoi-rust-accessor 从 C++ 元数据生成
// 替代反射机制，直接通过字段索引访问/设置值
// ============================================================

use std::any::Any;
use std::collections::HashMap;
use super::*;

// ============================================================
// 基本类型 ID（与 C++ E_type 枚举值一致）
// ============================================================

pub const TYPE_ID_U8: u32 = 26;
pub const TYPE_ID_U16: u32 = 27;
pub const TYPE_ID_U32: u32 = 28;
pub const TYPE_ID_U64: u32 = 29;
pub const TYPE_ID_I8: u32 = 30;
pub const TYPE_ID_I16: u32 = 31;
pub const TYPE_ID_I32: u32 = 32;
pub const TYPE_ID_I64: u32 = 33;
pub const TYPE_ID_F32: u32 = 34;
pub const TYPE_ID_F64: u32 = 35;
pub const TYPE_ID_STRING: u32 = 9;
pub const TYPE_ID_BOOL: u32 = 40;

// ============================================================
// SpoiAccessor — 类型特化访问器 trait
// ============================================================

pub trait SpoiAccessor: Send + Sync {
    fn field_count(&self) -> usize;
    fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any>;
    fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>);
}

// ============================================================
// deserialize_value — 通用值反序列化（基于 type_id 前缀）
// 格式: [type_id(u32 LE) + value_bytes]
// ============================================================

pub fn deserialize_value(data: &[u8]) -> Box<dyn Any> {
    if data.len() < 4 {
        return Box::new(NullValue);
    }
    let type_id = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
    let value_bytes = &data[4..];
    match type_id {
        TYPE_ID_U8 => {
            Box::new(value_bytes.first().copied().unwrap_or(0))
        }
        TYPE_ID_U16 => {
            Box::new(if value_bytes.len() >= 2 { u16::from_le_bytes([value_bytes[0], value_bytes[1]]) } else { 0u16 })
        }
        TYPE_ID_U32 => {
            Box::new(if value_bytes.len() >= 4 { u32::from_le_bytes([value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3]]) } else { 0u32 })
        }
        TYPE_ID_U64 => {
            Box::new(if value_bytes.len() >= 8 {
                u64::from_le_bytes([
                    value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3],
                    value_bytes[4], value_bytes[5], value_bytes[6], value_bytes[7],
                ])
            } else { 0u64 })
        }
        TYPE_ID_I8 => {
            Box::new(value_bytes.first().copied().unwrap_or(0) as i8)
        }
        TYPE_ID_I16 => {
            Box::new(if value_bytes.len() >= 2 { i16::from_le_bytes([value_bytes[0], value_bytes[1]]) } else { 0i16 })
        }
        TYPE_ID_I32 => {
            Box::new(if value_bytes.len() >= 4 { i32::from_le_bytes([value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3]]) } else { 0i32 })
        }
        TYPE_ID_I64 => {
            Box::new(if value_bytes.len() >= 8 {
                i64::from_le_bytes([
                    value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3],
                    value_bytes[4], value_bytes[5], value_bytes[6], value_bytes[7],
                ])
            } else { 0i64 })
        }
        TYPE_ID_F32 => {
            Box::new(if value_bytes.len() >= 4 { f32::from_le_bytes([value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3]]) } else { 0.0f32 })
        }
        TYPE_ID_F64 => {
            Box::new(if value_bytes.len() >= 8 {
                f64::from_le_bytes([
                    value_bytes[0], value_bytes[1], value_bytes[2], value_bytes[3],
                    value_bytes[4], value_bytes[5], value_bytes[6], value_bytes[7],
                ])
            } else { 0.0f64 })
        }
        TYPE_ID_STRING => {
            match String::from_utf8(value_bytes.to_vec()) {
                Ok(s) => Box::new(s),
                Err(_) => Box::new(value_bytes.to_vec()),
            }
        }
        TYPE_ID_BOOL => {
            Box::new(value_bytes.first().copied().unwrap_or(0) != 0)
        }
        _ => Box::new(value_bytes.to_vec()),
    }
}

// ============================================================
// SpoiTestPlayerAccessor
// ============================================================

pub struct SpoiTestPlayerAccessor;

impl SpoiAccessor for SpoiTestPlayerAccessor {
    fn field_count(&self) -> usize {
        4
    }

    fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any> {
        let o = obj.downcast_ref::<SpoiTestPlayer>()?;
        match idx {
            0 => Some(&o.name),
            1 => Some(&o.hp),
            2 => Some(&o.level),
            3 => Some(&o.posX),
            _ => None,
        }
    }

    fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>) {
        let o = match obj.downcast_mut::<SpoiTestPlayer>() {
            Some(o) => o,
            None => return,
        };
        match idx {
            0 => {
                if let Ok(v) = val.downcast::<String>() {
                    o.name = *v;
                }
            }
            1 => {
                if let Ok(v) = val.downcast::<i32>() {
                    o.hp = *v;
                }
            }
            2 => {
                if let Ok(v) = val.downcast::<i32>() {
                    o.level = *v;
                }
            }
            3 => {
                if let Ok(v) = val.downcast::<f64>() {
                    o.posX = *v;
                }
            }
            _ => {}
        }
    }
}

// ============================================================
// SpoiTestStateAccessor
// ============================================================

pub struct SpoiTestStateAccessor;

impl SpoiAccessor for SpoiTestStateAccessor {
    fn field_count(&self) -> usize {
        3
    }

    fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any> {
        let o = obj.downcast_ref::<SpoiTestState>()?;
        match idx {
            0 => Some(&o.tick),
            1 => Some(&o.currentMap),
            2 => Some(&o.players),
            _ => None,
        }
    }

    fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>) {
        let o = match obj.downcast_mut::<SpoiTestState>() {
            Some(o) => o,
            None => return,
        };
        match idx {
            0 => {
                if let Ok(v) = val.downcast::<i32>() {
                    o.tick = *v;
                }
            }
            1 => {
                if let Ok(v) = val.downcast::<String>() {
                    o.currentMap = *v;
                }
            }
            2 => {
                if let Ok(v) = val.downcast::<Box<dyn Any>>() {
                    o.players = *v;
                }
            }
            _ => {}
        }
    }
}

// ============================================================
// SpoiItemAccessor
// ============================================================

pub struct SpoiItemAccessor;

impl SpoiAccessor for SpoiItemAccessor {
    fn field_count(&self) -> usize {
        2
    }

    fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any> {
        let o = obj.downcast_ref::<SpoiItem>()?;
        match idx {
            0 => Some(&o.name),
            1 => Some(&o.value),
            _ => None,
        }
    }

    fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>) {
        let o = match obj.downcast_mut::<SpoiItem>() {
            Some(o) => o,
            None => return,
        };
        match idx {
            0 => {
                if let Ok(v) = val.downcast::<String>() {
                    o.name = *v;
                }
            }
            1 => {
                if let Ok(v) = val.downcast::<i32>() {
                    o.value = *v;
                }
            }
            _ => {}
        }
    }
}

// ============================================================
// SpoiInventoryAccessor
// ============================================================

pub struct SpoiInventoryAccessor;

impl SpoiAccessor for SpoiInventoryAccessor {
    fn field_count(&self) -> usize {
        3
    }

    fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any> {
        let o = obj.downcast_ref::<SpoiInventory>()?;
        match idx {
            0 => Some(&o.items),
            1 => Some(&o.equipped),
            2 => Some(&o.gold),
            _ => None,
        }
    }

    fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>) {
        let o = match obj.downcast_mut::<SpoiInventory>() {
            Some(o) => o,
            None => return,
        };
        match idx {
            0 => {
                if let Ok(v) = val.downcast::<Box<dyn Any>>() {
                    o.items = *v;
                }
            }
            1 => {
                if let Ok(v) = val.downcast::<Box<dyn Any>>() {
                    o.equipped = *v;
                }
            }
            2 => {
                if let Ok(v) = val.downcast::<i32>() {
                    o.gold = *v;
                }
            }
            _ => {}
        }
    }
}

// ============================================================
// SpoiCharacterAccessor
// ============================================================

pub struct SpoiCharacterAccessor;

impl SpoiAccessor for SpoiCharacterAccessor {
    fn field_count(&self) -> usize {
        5
    }

    fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any> {
        let o = obj.downcast_ref::<SpoiCharacter>()?;
        match idx {
            0 => Some(&o.name),
            1 => Some(&o.hp),
            2 => Some(&o.inventory),
            3 => Some(&o.weapon),
            4 => Some(&o.petLevel),
            _ => None,
        }
    }

    fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>) {
        let o = match obj.downcast_mut::<SpoiCharacter>() {
            Some(o) => o,
            None => return,
        };
        match idx {
            0 => {
                if let Ok(v) = val.downcast::<String>() {
                    o.name = *v;
                }
            }
            1 => {
                if let Ok(v) = val.downcast::<i32>() {
                    o.hp = *v;
                }
            }
            2 => {
                if let Ok(v) = val.downcast::<Box<dyn Any>>() {
                    o.inventory = *v;
                }
            }
            3 => {
                if let Ok(v) = val.downcast::<Box<dyn Any>>() {
                    o.weapon = *v;
                }
            }
            4 => {
                if let Ok(v) = val.downcast::<i32>() {
                    o.petLevel = *v;
                }
            }
            _ => {}
        }
    }
}

// ============================================================
// SpoiWorldAccessor
// ============================================================

pub struct SpoiWorldAccessor;

impl SpoiAccessor for SpoiWorldAccessor {
    fn field_count(&self) -> usize {
        3
    }

    fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any> {
        let o = obj.downcast_ref::<SpoiWorld>()?;
        match idx {
            0 => Some(&o.worldName),
            1 => Some(&o.tick),
            2 => Some(&o.characters),
            _ => None,
        }
    }

    fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>) {
        let o = match obj.downcast_mut::<SpoiWorld>() {
            Some(o) => o,
            None => return,
        };
        match idx {
            0 => {
                if let Ok(v) = val.downcast::<String>() {
                    o.worldName = *v;
                }
            }
            1 => {
                if let Ok(v) = val.downcast::<i32>() {
                    o.tick = *v;
                }
            }
            2 => {
                if let Ok(v) = val.downcast::<Box<dyn Any>>() {
                    o.characters = *v;
                }
            }
            _ => {}
        }
    }
}

// ============================================================
// SpoiAccessorRegistry — 静态类型注册表
// 替代运行时 HashMap<String, Vec<String>>
// ============================================================

pub fn create_spoi_accessor_registry() -> HashMap<String, Box<dyn SpoiAccessor>> {
    let mut registry: HashMap<String, Box<dyn SpoiAccessor>> = HashMap::new();
    registry.insert("SpoiTestPlayer".to_string(), Box::new(SpoiTestPlayerAccessor));
    registry.insert("SpoiTestState".to_string(), Box::new(SpoiTestStateAccessor));
    registry.insert("SpoiItem".to_string(), Box::new(SpoiItemAccessor));
    registry.insert("SpoiInventory".to_string(), Box::new(SpoiInventoryAccessor));
    registry.insert("SpoiCharacter".to_string(), Box::new(SpoiCharacterAccessor));
    registry.insert("SpoiWorld".to_string(), Box::new(SpoiWorldAccessor));
    registry
}

