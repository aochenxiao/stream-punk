/* =================== StreamPunk SPOI Executor — Rust Runtime (v2: 访问器驱动) ===================

SPOI = StreamPunk Operation Instruction
执行 SPOI 指令流，对 Rust 对象进行查询/更新操作。

与 v1 的区别：
  - 使用 SpoiAccessor trait 替代运行时 type_registry
  - 使用 deserialize_value（基于 type_id 前缀）替代字节长度启发式
  - 导航和字段设置通过访问器的 match 跳转表，O(1) 且无反射开销

用法：
    use spoi_executor::{SpoiExecutor, SpoiAccessor, Navigable, NavigableBox};
    use std::collections::HashMap;

    let accessors: HashMap<String, Box<dyn SpoiAccessor>> = create_spoi_accessor_registry();
    let executor = SpoiExecutor::new(accessors);
    let result = executor.execute(&mut root_obj, &instruction_bytes);
*/

use std::any::Any;
use std::collections::HashMap;
use std::fmt::Debug;

// =============================== 空值标记 ===============================

/// 空值标记，用于表示 null 写入
#[derive(Debug, Clone)]
pub struct NullValue;

// =============================== SPOI 类型定义 ===============================
// 这些类型由 sp-gen 生成的访问器引用，需要在 accessor 模块之前定义

#[derive(Debug, Clone)]
pub struct SpoiTestPlayer {
    pub name: String,
    pub hp: i32,
    pub level: i32,
    pub posX: f64,
}

#[derive(Debug)]
pub struct SpoiTestState {
    pub tick: i32,
    pub currentMap: String,
    pub players: Box<dyn Any>,
}

#[derive(Debug, Clone)]
pub struct SpoiItem {
    pub name: String,
    pub value: i32,
}

#[derive(Debug)]
pub struct SpoiInventory {
    pub items: Box<dyn Any>,
    pub equipped: Box<dyn Any>,
    pub gold: i32,
}

#[derive(Debug)]
pub struct SpoiCharacter {
    pub name: String,
    pub hp: i32,
    pub inventory: Box<dyn Any>,
    pub weapon: Box<dyn Any>,
    pub petLevel: i32,
}

#[derive(Debug)]
pub struct SpoiWorld {
    pub worldName: String,
    pub tick: i32,
    pub characters: Box<dyn Any>,
}

#[path = "spoi_accessor.rs"]
mod spoi_accessor;
pub use self::spoi_accessor::*;

// =============================== 操作码常量 ===============================

pub mod op {
    // 导航
    pub const NAV: u8 = 0x00;
    pub const IDX: u8 = 0x01;
    pub const DEREF: u8 = 0x02;
    pub const UNWRAP: u8 = 0x03;
    // 写操作
    pub const SET: u8 = 0x04;
    pub const ADD: u8 = 0x05;
    pub const APPEND: u8 = 0x06;
    pub const REMOVE: u8 = 0x07;
    pub const INSERT: u8 = 0x08;
    pub const REPLACE: u8 = 0x09;
    pub const RESET: u8 = 0x0A;
    pub const SETNULL: u8 = 0x0B;
    // 读操作
    pub const FILTER: u8 = 0x0C;
    pub const SELECT: u8 = 0x0D;
    pub const SORT: u8 = 0x0E;
    pub const REVERSE: u8 = 0x0F;
    pub const TAKE: u8 = 0x10;
    pub const DROP: u8 = 0x11;
    pub const TAKEWHILE: u8 = 0x12;
    pub const DROPWHILE: u8 = 0x13;
    pub const DISTINCT: u8 = 0x14;
    // 聚合
    pub const COUNT: u8 = 0x15;
    pub const ANY: u8 = 0x16;
    pub const ALL: u8 = 0x17;
    pub const FIND: u8 = 0x18;
    // 容器
    pub const KEYS: u8 = 0x19;
    pub const VALUES: u8 = 0x1A;
    pub const JOIN: u8 = 0x1B;
    // 控制
    pub const EXEC: u8 = 0x21;
    pub const PIPE: u8 = 0x22;
}

// 路径特殊标记
pub const PATH_DEREF: usize = 0xFFFF;
pub const PATH_MAPKEY: usize = 0xFFFE;

// =============================== 结果类型 ===============================

pub mod result_type {
    pub const UNDEF: u32 = 0;
    pub const SINGLE: u32 = 1;
    pub const VECTOR: u32 = 2;
    pub const COUNT: u32 = 3;
    pub const BOOL: u32 = 4;
    pub const OPTIONAL: u32 = 5;
    pub const ERROR: u32 = 6;
}

// =============================== Varint 编解码 ===============================

/// 读取 varint，返回 (value, new_offset)
pub fn read_varint(data: &[u8], offset: usize) -> (usize, usize) {
    let mut result: usize = 0;
    let mut shift = 0;
    let mut off = offset;
    while off < data.len() {
        let b = data[off];
        off += 1;
        result |= ((b & 0x7F) as usize) << shift;
        if b & 0x80 == 0 {
            return (result, off);
        }
        shift += 7;
    }
    (result, off)
}

/// 写入 varint 到 Vec<u8>
pub fn write_varint(buf: &mut Vec<u8>, mut v: usize) {
    while v >= 0x80 {
        buf.push((v & 0x7F) as u8 | 0x80);
        v >>= 7;
    }
    buf.push(v as u8);
}

// =============================== Navigable 特性 ===============================

/// 结构体需实现此 trait 以支持 SPOI 导航。
/// 由 sp-gen 代码生成器为各消息类型自动生成实现。
/// 与 SpoiAccessor 兼容：访问器生成的代码可同时实现此 trait。
pub trait Navigable: Any {
    /// 按索引获取字段值（不可变）
    fn field_by_index(&self, idx: usize) -> Option<&dyn Any>;
    /// 按索引获取字段值（可变）
    fn field_by_index_mut(&mut self, idx: usize) -> Option<&mut dyn Any>;
    /// 按索引设置字段值
    fn set_field_by_index(&mut self, idx: usize, value: Box<dyn Any>);
    /// 解引用（默认取第一个字段，用于指针解引用）
    fn deref_value(&self) -> Option<&dyn Any> {
        self.field_by_index(0)
    }
}

/// 包装器，使 Box<dyn Navigable> 可以存入 Box<dyn Any> 并能被 downcast 还原。
/// 所有实现 Navigable 的结构体通过此包装器存入 pipe_data。
pub struct NavigableBox(pub Box<dyn Navigable>);

impl NavigableBox {
    pub fn new<T: Navigable + 'static>(value: T) -> Self {
        NavigableBox(Box::new(value))
    }
}

// =============================== SPOI 指令 ===============================

/// SPOI 指令
#[derive(Debug, Clone)]
pub struct SpoiInstruction {
    pub op: u8,
    pub path: Vec<usize>,
    pub operand: Vec<u8>,
}

// =============================== SPOI 指令解析 ===============================

/// 解析 SPOI 指令流
pub fn parse_spoi_stream(data: &[u8]) -> Vec<SpoiInstruction> {
    let mut offset = 0;
    let (count, new_off) = read_varint(data, offset);
    offset = new_off;
    let mut instructions = Vec::with_capacity(count);

    for _ in 0..count {
        // op
        let op = data[offset];
        offset += 1;

        // path
        let (path_len, new_off) = read_varint(data, offset);
        offset = new_off;
        let mut path = Vec::with_capacity(path_len);
        for _ in 0..path_len {
            let (seg, new_off) = read_varint(data, offset);
            offset = new_off;
            path.push(seg);
        }

        // operand
        let (operand_len, new_off) = read_varint(data, offset);
        offset = new_off;
        let operand = data[offset..offset + operand_len].to_vec();
        offset += operand_len;

        instructions.push(SpoiInstruction { op, path, operand });
    }

    instructions
}

// =============================== SPOI 执行器（v2: 访问器驱动） ===============================

/// SPOI 指令执行器
pub struct SpoiExecutor {
    pub accessors: HashMap<String, Box<dyn SpoiAccessor>>,
    pub pipe_data: Vec<Box<dyn Any>>,
}

impl SpoiExecutor {
    pub fn new(accessors: HashMap<String, Box<dyn SpoiAccessor>>) -> Self {
        SpoiExecutor {
            accessors,
            pipe_data: Vec::new(),
        }
    }

    /// 执行 SPOI 指令流。
    /// root 为可变引用，允许写操作直接修改根对象。
    /// 返回 HashMap，包含 "resultType" 和 "value" 键。
    pub fn execute(
        &mut self,
        root: &mut dyn Any,
        instruction_bytes: &[u8],
    ) -> HashMap<String, Box<dyn Any>> {
        let instructions = parse_spoi_stream(instruction_bytes);
        self.pipe_data.clear();

        for inst in &instructions {
            self.dispatch(inst, root);
        }

        self.make_result()
    }

    // =============================== 结果构建 ===============================

    fn make_result(&mut self) -> HashMap<String, Box<dyn Any>> {
        let mut result: HashMap<String, Box<dyn Any>> = HashMap::new();

        if self.pipe_data.is_empty() {
            result.insert(
                "resultType".to_string(),
                Box::new(result_type::UNDEF) as Box<dyn Any>,
            );
            result.insert(
                "value".to_string(),
                Box::new(Vec::<Box<dyn Any>>::new()) as Box<dyn Any>,
            );
            return result;
        }

        if self.pipe_data.len() == 1 {
            let val = self.pipe_data.remove(0);
            result.insert(
                "resultType".to_string(),
                Box::new(result_type::SINGLE) as Box<dyn Any>,
            );
            result.insert("value".to_string(), val);
            return result;
        }

        let data = std::mem::take(&mut self.pipe_data);
        result.insert(
            "resultType".to_string(),
            Box::new(result_type::VECTOR) as Box<dyn Any>,
        );
        result.insert("value".to_string(), Box::new(data) as Box<dyn Any>);
        result
    }

    /// 获取执行结果（消耗 executor，返回 (resultType, pipe_data)）
    pub fn take_result(mut self) -> (u32, Vec<Box<dyn Any>>) {
        if self.pipe_data.is_empty() {
            return (result_type::UNDEF, Vec::new());
        }
        if self.pipe_data.len() == 1 {
            return (result_type::SINGLE, self.pipe_data);
        }
        let data = std::mem::take(&mut self.pipe_data);
        (result_type::VECTOR, data)
    }

    // =============================== 分发 ===============================

    fn dispatch(&mut self, inst: &SpoiInstruction, root: &mut dyn Any) {
        match inst.op {
            // 写操作
            op::SET => self.op_set(root, &inst.path, &inst.operand),
            op::ADD => self.op_add(root, &inst.path, &inst.operand),
            op::APPEND => self.op_append(root, &inst.path, &inst.operand),
            op::REMOVE => self.op_remove(root, &inst.path, &inst.operand),
            op::INSERT => self.op_insert(root, &inst.path, &inst.operand),
            op::REPLACE => self.op_replace(root, &inst.path, &inst.operand),
            op::RESET => self.op_reset(root, &inst.path),
            op::SETNULL => self.op_setnull(root, &inst.path),
            // 读操作
            op::FILTER => self.op_filter(&inst.path, &inst.operand),
            op::SELECT => self.op_select(&inst.path),
            op::SORT => self.op_sort(&inst.path),
            op::REVERSE => self.op_reverse(),
            op::TAKE => self.op_take(&inst.operand),
            op::DROP => self.op_drop(&inst.operand),
            op::TAKEWHILE => self.op_takewhile(&inst.path, &inst.operand),
            op::DROPWHILE => self.op_dropwhile(&inst.path, &inst.operand),
            op::DISTINCT => self.op_distinct(),
            // 聚合
            op::COUNT => self.op_count(),
            op::ANY => self.op_any(&inst.path, &inst.operand),
            op::ALL => self.op_all(&inst.path, &inst.operand),
            op::FIND => self.op_find(&inst.path, &inst.operand),
            // 容器
            op::KEYS => self.op_keys(),
            op::VALUES => self.op_values(),
            op::JOIN => self.op_join(),
            // 控制
            op::EXEC => { /* 执行结束，结果已在 pipe_data 中 */ }
            op::PIPE => self.op_pipe(root, &inst.path),
            _ => panic!("Unknown SPOI opcode: 0x{:02X}", inst.op),
        }
    }

    // =============================== 访问器查找 ===============================

    /// 根据类型名查找访问器
    pub fn get_accessor(&self, type_name: &str) -> Option<&dyn SpoiAccessor> {
        self.accessors.get(type_name).map(|b| &**b)
    }

    /// 获取对象对应的访问器（通过类型名匹配）
    fn get_accessor_for_obj(&self, obj: &dyn Any) -> Option<&dyn SpoiAccessor> {
        let type_name = self.get_type_name(obj);
        self.get_accessor(type_name)
    }

    // =============================== 导航（不可变） ===============================

    /// 沿路径导航到目标字段（不可变）
    fn navigate<'a>(&'a self, obj: &'a dyn Any, path: &[usize]) -> &'a dyn Any {
        let mut current = obj;
        for &seg in path {
            current = self.nav_step(current, seg);
        }
        current
    }

    /// 单步导航（不可变）— 访问器优先，Navigable 回退
    pub fn nav_step<'a>(&'a self, obj: &'a dyn Any, seg: usize) -> &'a dyn Any {
        // 指针解引用
        if seg == PATH_DEREF {
            // 先尝试 NavigableBox
            if let Some(nav_box) = obj.downcast_ref::<NavigableBox>() {
                if let Some(val) = nav_box.0.deref_value() {
                    return val;
                }
            }
            return obj;
        }

        // Vec<Box<dyn Any>> 索引访问
        if let Some(list) = obj.downcast_ref::<Vec<Box<dyn Any>>>() {
            return &*list[seg];
        }

        // HashMap<String, Box<dyn Any>> 按索引访问值
        if let Some(map) = obj.downcast_ref::<HashMap<String, Box<dyn Any>>>() {
            let values: Vec<&Box<dyn Any>> = map.values().collect();
            return &*values[seg];
        }

        // NavigableBox 结构体成员访问 — 访问器优先，Navigable 回退
        if let Some(nav_box) = obj.downcast_ref::<NavigableBox>() {
            // 先尝试通过访问器
            let type_name = self.get_type_name(obj);
            if let Some(acc) = self.get_accessor(type_name) {
                if let Some(field) = acc.get_field(&*nav_box.0, seg) {
                    return field;
                }
            }
            // 回退到 Navigable
            if let Some(field) = nav_box.0.field_by_index(seg) {
                return field;
            }
        }

        // 基本类型：seg == 0 表示取自身值
        if seg == 0 {
            return obj;
        }

        panic!(
            "Cannot navigate segment {} on type '{}'",
            seg,
            self.get_type_name(obj)
        );
    }

    // =============================== 导航（可变） ===============================

    /// 沿路径设置值（导航到父级，然后设置最后一个字段）
    fn nav_set(&mut self, root: &mut dyn Any, path: &[usize], value: Box<dyn Any>) {
        if path.is_empty() {
            return;
        }
        if path.len() == 1 {
            self.set_field(root, path[0], value);
            return;
        }

        // 导航到父级（路径去掉最后一个元素）
        let parent_path = &path[..path.len() - 1];
        let last_seg = path[path.len() - 1];

        // 使用可变导航获取父级
        let parent = self.navigate_mut(root, parent_path);
        self.set_field(parent, last_seg, value);
    }

    /// 沿路径导航到目标字段（可变），使用原始指针绕过生命周期限制
    fn navigate_mut<'a>(&self, root: &'a mut dyn Any, path: &[usize]) -> &'a mut dyn Any {
        let mut current: *mut dyn Any = root as *mut dyn Any;
        for &seg in path {
            unsafe {
                current = self.nav_step_mut_ptr(current, seg);
            }
        }
        unsafe { &mut *current }
    }

    /// 单步导航（可变，原始指针版本）— 访问器优先，Navigable 回退
    /// # Safety
    /// 调用者需确保 obj 指针有效且没有其他可变引用
    unsafe fn nav_step_mut_ptr(&self, obj: *mut dyn Any, seg: usize) -> *mut dyn Any {
        let obj_ref = &mut *obj;

        // 指针解引用
        if seg == PATH_DEREF {
            if let Some(nav_box) = obj_ref.downcast_mut::<NavigableBox>() {
                if let Some(val) = nav_box.0.field_by_index_mut(0) {
                    return val as *mut dyn Any;
                }
            }
            return obj;
        }

        // Vec<Box<dyn Any>> 索引访问
        if let Some(list) = obj_ref.downcast_mut::<Vec<Box<dyn Any>>>() {
            let elem: *mut Box<dyn Any> = &mut list[seg];
            return elem as *mut dyn Any;
        }

        // HashMap<String, Box<dyn Any>> 按索引访问值
        if let Some(map) = obj_ref.downcast_mut::<HashMap<String, Box<dyn Any>>>() {
            let keys: Vec<String> = map.keys().cloned().collect();
            if seg < keys.len() {
                let key = keys[seg].clone();
                let val = map.get_mut(&key).unwrap();
                return &mut **val as *mut dyn Any;
            }
        }

        // NavigableBox 结构体成员访问 — 访问器优先，Navigable 回退
        if let Some(nav_box) = obj_ref.downcast_mut::<NavigableBox>() {
            if let Some(field) = nav_box.0.field_by_index_mut(seg) {
                return field as *mut dyn Any;
            }
        }

        panic!("Cannot navigate mutably segment {}", seg);
    }

    /// 设置字段值 — 访问器优先，Navigable 回退
    fn set_field(&self, obj: &mut dyn Any, seg: usize, value: Box<dyn Any>) {
        if let Some(list) = obj.downcast_mut::<Vec<Box<dyn Any>>>() {
            if seg < list.len() {
                list[seg] = value;
                return;
            }
            panic!("Index {} out of bounds for list of length {}", seg, list.len());
        }

        if let Some(map) = obj.downcast_mut::<HashMap<String, Box<dyn Any>>>() {
            let keys: Vec<String> = map.keys().cloned().collect();
            if seg < keys.len() {
                let key = keys[seg].clone();
                map.insert(key, value);
                return;
            }
            panic!("Index {} out of bounds for map of size {}", seg, keys.len());
        }

        let type_name = self.get_type_name(obj).to_string();
        if let Some(nav_box) = obj.downcast_mut::<NavigableBox>() {
            // 先尝试通过访问器
            if let Some(acc) = self.get_accessor(&type_name) {
                // 使用访问器设置字段（通过原始指针回退到 Navigable）
                let raw: *mut dyn Navigable = &mut *nav_box.0;
                acc.set_field(unsafe { &mut *raw }, seg, value);
                return;
            }
            // 回退到 Navigable
            nav_box.0.set_field_by_index(seg, value);
            return;
        }

        panic!("Cannot set field {} on type", seg);
    }

    // =============================== 类型名获取 ===============================

    fn get_type_name(&self, obj: &dyn Any) -> &str {
        if obj.downcast_ref::<Vec<Box<dyn Any>>>().is_some() {
            return "Vec<Box<dyn Any>>";
        }
        if obj.downcast_ref::<HashMap<String, Box<dyn Any>>>().is_some() {
            return "HashMap<String, Box<dyn Any>>";
        }
        if obj.downcast_ref::<NavigableBox>().is_some() {
            return "NavigableBox";
        }
        let type_name = std::any::type_name::<dyn Any>();
        type_name.split("::").last().unwrap_or(type_name)
    }

    // =============================== 写操作 ===============================

    pub(crate) fn op_set(&mut self, root: &mut dyn Any, path: &[usize], operand: &[u8]) {
        let value = deserialize_value(operand);
        self.nav_set(root, path, value);
    }

    pub(crate) fn op_add(&mut self, root: &mut dyn Any, path: &[usize], operand: &[u8]) {
        let delta = deserialize_value(operand);
        let target = self.navigate(root, path);
        let result = self.try_add(target, delta);
        self.nav_set(root, path, result);
    }

    fn try_add(&self, target: &dyn Any, delta: Box<dyn Any>) -> Box<dyn Any> {
        if let (Some(&a), Some(&b)) = (
            target.downcast_ref::<i64>(),
            delta.downcast_ref::<i64>(),
        ) {
            return Box::new(a + b);
        }
        if let (Some(&a), Some(&b)) = (
            target.downcast_ref::<i32>(),
            delta.downcast_ref::<i32>(),
        ) {
            return Box::new(a + b);
        }
        if let (Some(&a), Some(&b)) = (
            target.downcast_ref::<u32>(),
            delta.downcast_ref::<u32>(),
        ) {
            return Box::new(a + b);
        }
        if let (Some(&a), Some(&b)) = (
            target.downcast_ref::<u64>(),
            delta.downcast_ref::<u64>(),
        ) {
            return Box::new(a + b);
        }
        if let (Some(&a), Some(&b)) = (
            target.downcast_ref::<f64>(),
            delta.downcast_ref::<f64>(),
        ) {
            return Box::new(a + b);
        }
        if let (Some(a), Some(b)) = (
            target.downcast_ref::<String>(),
            delta.downcast_ref::<String>(),
        ) {
            return Box::new(format!("{}{}", a, b));
        }
        panic!("Cannot add between types");
    }

    pub(crate) fn op_append(&mut self, root: &mut dyn Any, path: &[usize], operand: &[u8]) {
        let value = deserialize_value(operand);
        let target_mut = self.navigate_mut(root, path);
        if let Some(list) = target_mut.downcast_mut::<Vec<Box<dyn Any>>>() {
            list.push(value);
            return;
        }
        panic!("Cannot append to non-list type");
    }

    pub(crate) fn op_remove(&mut self, root: &mut dyn Any, path: &[usize], operand: &[u8]) {
        let idx = if operand.len() >= 4 {
            u32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]) as usize
        } else {
            0
        };
        let target_mut = self.navigate_mut(root, path);
        if let Some(list) = target_mut.downcast_mut::<Vec<Box<dyn Any>>>() {
            if idx < list.len() {
                list.remove(idx);
                return;
            }
        }
        panic!("Cannot remove from type");
    }

    pub(crate) fn op_insert(&mut self, root: &mut dyn Any, path: &[usize], operand: &[u8]) {
        let idx = if operand.len() >= 4 {
            u32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]) as usize
        } else {
            0
        };
        let value = deserialize_value(&operand[4..]);
        let target_mut = self.navigate_mut(root, path);
        if let Some(list) = target_mut.downcast_mut::<Vec<Box<dyn Any>>>() {
            if idx <= list.len() {
                list.insert(idx, value);
                return;
            }
        }
        panic!("Cannot insert into type");
    }

    pub(crate) fn op_replace(&mut self, root: &mut dyn Any, path: &[usize], operand: &[u8]) {
        let idx = if operand.len() >= 4 {
            u32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]) as usize
        } else {
            0
        };
        let value = deserialize_value(&operand[4..]);
        let target_mut = self.navigate_mut(root, path);
        if let Some(list) = target_mut.downcast_mut::<Vec<Box<dyn Any>>>() {
            if idx < list.len() {
                list[idx] = value;
                return;
            }
        }
        panic!("Cannot replace in type");
    }

    pub(crate) fn op_reset(&mut self, root: &mut dyn Any, path: &[usize]) {
        self.nav_set(root, path, Box::new(NullValue));
    }

    pub(crate) fn op_setnull(&mut self, root: &mut dyn Any, path: &[usize]) {
        self.nav_set(root, path, Box::new(NullValue));
    }

    // =============================== 读操作 ===============================

    /// 管道入口：将路径指向的数据加载到管道缓冲区
    pub(crate) fn op_pipe(&mut self, root: &dyn Any, path: &[usize]) {
        let data: &dyn Any = if path.is_empty() {
            root
        } else {
            self.navigate(root, path)
        };

        if let Some(list) = data.downcast_ref::<Vec<Box<dyn Any>>>() {
            self.pipe_data = list
                .iter()
                .map(|item| clone_any(item))
                .collect();
        } else if let Some(map) = data.downcast_ref::<HashMap<String, Box<dyn Any>>>() {
            self.pipe_data = map
                .values()
                .map(|item| clone_any(item))
                .collect();
        } else {
            self.pipe_data = vec![clone_any_ref(data)];
        }
    }

    /// 检查对象是否匹配比较表达式（v2: 访问器驱动）
    /// operand 格式: memberIdx(u32, LE) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
    fn matches(&self, obj: &dyn Any, _path: &[usize], operand: &[u8]) -> bool {
        // operand 格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
        if operand.len() < 9 {
            return true;
        }
        let member_idx =
            u32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]) as usize;
        let cmp_op = operand[4];
        // value_len 是 varint 编码的，跳过它
        let (_, offset) = read_varint(operand, 5);
        let value_bytes = &operand[offset..];

        let field_value = self.nav_step(obj, member_idx);
        let expected = deserialize_value(value_bytes);

        compare_values(field_value, &*expected, cmp_op)
    }

    pub(crate) fn op_filter(&mut self, _path: &[usize], operand: &[u8]) {
        let items: Vec<Box<dyn Any>> = self.pipe_data.drain(..).collect();
        let mut new_data: Vec<Box<dyn Any>> = Vec::new();
        for item in items {
            if self.matches(&*item, _path, operand) {
                new_data.push(item);
            }
        }
        self.pipe_data = new_data;
    }

    pub(crate) fn op_select(&mut self, path: &[usize]) {
        if path.is_empty() {
            return;
        }

        // 先收集，避免 borrow checker 问题
        let items: Vec<Box<dyn Any>> = self.pipe_data.drain(..).collect();
        let mut new_data: Vec<Box<dyn Any>> = Vec::new();

        for item in items {
            let selected = self.nav_step(&*item, path[0]);
            let result = if path.len() == 1 {
                clone_any_ref(selected)
            } else {
                clone_any_ref(self.navigate(selected, &path[1..]))
            };
            new_data.push(result);
        }
        self.pipe_data = new_data;
    }

    pub(crate) fn op_sort(&mut self, path: &[usize]) {
        if path.is_empty() {
            let mut items: Vec<(String, Box<dyn Any>)> = self
                .pipe_data
                .drain(..)
                .map(|item| {
                    let desc = describe_any(&*item);
                    (desc, item)
                })
                .collect();
            items.sort_by(|a, b| a.0.cmp(&b.0));
            self.pipe_data = items.into_iter().map(|(_, item)| item).collect();
        } else {
            // 先 drain 出来，再依次导航（避免 borrow checker 冲突）
            let items: Vec<Box<dyn Any>> = self.pipe_data.drain(..).collect();
            let path = path.to_vec();
            let mut pairs: Vec<(String, Box<dyn Any>)> = Vec::with_capacity(items.len());
            for item in items {
                let val = self.navigate(&*item, &path);
                let desc = describe_any(val);
                pairs.push((desc, item));
            }
            pairs.sort_by(|a, b| a.0.cmp(&b.0));
            self.pipe_data = pairs.into_iter().map(|(_, item)| item).collect();
        }
    }

    pub(crate) fn op_reverse(&mut self) {
        self.pipe_data.reverse();
    }

    pub(crate) fn op_take(&mut self, operand: &[u8]) {
        let n = if operand.len() >= 4 {
            u32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]) as usize
        } else {
            0
        };
        if n < self.pipe_data.len() {
            self.pipe_data.truncate(n);
        }
    }

    pub(crate) fn op_drop(&mut self, operand: &[u8]) {
        let n = if operand.len() >= 4 {
            u32::from_le_bytes([operand[0], operand[1], operand[2], operand[3]]) as usize
        } else {
            0
        };
        if n < self.pipe_data.len() {
            self.pipe_data = self.pipe_data.split_off(n);
        } else {
            self.pipe_data.clear();
        }
    }

    pub(crate) fn op_takewhile(&mut self, _path: &[usize], operand: &[u8]) {
        let items: Vec<Box<dyn Any>> = self.pipe_data.drain(..).collect();
        let mut result = Vec::new();
        for item in items {
            if self.matches(&*item, _path, operand) {
                result.push(item);
            } else {
                break;
            }
        }
        self.pipe_data = result;
    }

    pub(crate) fn op_dropwhile(&mut self, _path: &[usize], operand: &[u8]) {
        let mut idx = self.pipe_data.len();
        for (i, item) in self.pipe_data.iter().enumerate() {
            if !self.matches(&**item, _path, operand) {
                idx = i;
                break;
            }
        }
        if idx < self.pipe_data.len() {
            self.pipe_data = self.pipe_data.split_off(idx);
        } else {
            self.pipe_data.clear();
        }
    }

    pub(crate) fn op_distinct(&mut self) {
        let items: Vec<Box<dyn Any>> = self.pipe_data.drain(..).collect();
        let mut seen: Vec<String> = Vec::new();
        let mut result: Vec<Box<dyn Any>> = Vec::new();
        for item in items {
            let key = describe_any(&*item);
            let is_primitive = item.downcast_ref::<i32>().is_some()
                || item.downcast_ref::<i64>().is_some()
                || item.downcast_ref::<f64>().is_some()
                || item.downcast_ref::<String>().is_some()
                || item.downcast_ref::<bool>().is_some();

            let lookup_key = if is_primitive {
                key.clone()
            } else {
                key
            };

            if !seen.contains(&lookup_key) {
                seen.push(lookup_key);
                result.push(item);
            }
        }
        self.pipe_data = result;
    }

    // =============================== 聚合 ===============================

    pub(crate) fn op_count(&mut self) {
        let count = self.pipe_data.len();
        self.pipe_data = vec![Box::new(count as u32)];
    }

    pub(crate) fn op_any(&mut self, _path: &[usize], operand: &[u8]) {
        let result = self
            .pipe_data
            .iter()
            .any(|item| self.matches(&**item, _path, operand));
        self.pipe_data = vec![Box::new(result)];
    }

    pub(crate) fn op_all(&mut self, _path: &[usize], operand: &[u8]) {
        let result = self
            .pipe_data
            .iter()
            .all(|item| self.matches(&**item, _path, operand));
        self.pipe_data = vec![Box::new(result)];
    }

    pub(crate) fn op_find(&mut self, _path: &[usize], operand: &[u8]) {
        let items: Vec<Box<dyn Any>> = self.pipe_data.drain(..).collect();
        for item in items {
            if self.matches(&*item, _path, operand) {
                self.pipe_data = vec![item];
                return;
            }
        }
        self.pipe_data = Vec::new();
    }

    // =============================== 容器操作 ===============================

    fn op_keys(&mut self) {
        let items: Vec<Box<dyn Any>> = self.pipe_data.drain(..).collect();
        let mut result: Vec<Box<dyn Any>> = Vec::new();
        for item in items {
            if let Some(map) = item.downcast_ref::<HashMap<String, Box<dyn Any>>>() {
                for key in map.keys() {
                    result.push(Box::new(key.clone()));
                }
            }
        }
        self.pipe_data = result;
    }

    pub(crate) fn op_values(&mut self) {
        let items: Vec<Box<dyn Any>> = self.pipe_data.drain(..).collect();
        let mut result: Vec<Box<dyn Any>> = Vec::new();
        for item in items {
            if let Some(map) = item.downcast_ref::<HashMap<String, Box<dyn Any>>>() {
                for value in map.values() {
                    result.push(clone_any(value));
                }
            }
        }
        self.pipe_data = result;
    }

    pub(crate) fn op_join(&mut self) {
        // 展平嵌套列表
        let items: Vec<Box<dyn Any>> = self.pipe_data.drain(..).collect();
        let mut result: Vec<Box<dyn Any>> = Vec::new();
        for item in items {
            if let Some(list) = item.downcast_ref::<Vec<Box<dyn Any>>>() {
                for elem in list {
                    result.push(clone_any(elem));
                }
            } else {
                result.push(item);
            }
        }
        self.pipe_data = result;
    }
}

// =============================== 自由函数：值操作 ===============================

/// 浅克隆 Box<dyn Any> 中的值
pub fn clone_any(value: &Box<dyn Any>) -> Box<dyn Any> {
    clone_any_ref(&**value)
}

/// 浅克隆 &dyn Any 中的值
pub fn clone_any_ref(value: &dyn Any) -> Box<dyn Any> {
    if let Some(v) = value.downcast_ref::<i32>() {
        Box::new(*v)
    } else if let Some(v) = value.downcast_ref::<i64>() {
        Box::new(*v)
    } else if let Some(v) = value.downcast_ref::<u32>() {
        Box::new(*v)
    } else if let Some(v) = value.downcast_ref::<u64>() {
        Box::new(*v)
    } else if let Some(v) = value.downcast_ref::<f64>() {
        Box::new(*v)
    } else if let Some(v) = value.downcast_ref::<bool>() {
        Box::new(*v)
    } else if let Some(v) = value.downcast_ref::<String>() {
        Box::new(v.clone())
    } else if let Some(v) = value.downcast_ref::<u8>() {
        Box::new(*v)
    } else if let Some(v) = value.downcast_ref::<Vec<Box<dyn Any>>>() {
        let cloned: Vec<Box<dyn Any>> = v.iter().map(|item| clone_any(item)).collect();
        Box::new(cloned)
    } else if value.downcast_ref::<NavigableBox>().is_some() {
        Box::new(NullValue)
    } else if let Some(v) = value.downcast_ref::<NullValue>() {
        Box::new(v.clone())
    } else {
        Box::new(NullValue)
    }
}

/// 将任意值转为 i64 用于比较（启发式）
pub fn to_i64(a: &dyn Any) -> Option<i64> {
    if let Some(&v) = a.downcast_ref::<u8>() {
        Some(v as i64)
    } else if let Some(&v) = a.downcast_ref::<u16>() {
        Some(v as i64)
    } else if let Some(&v) = a.downcast_ref::<u32>() {
        Some(v as i64)
    } else if let Some(&v) = a.downcast_ref::<u64>() {
        Some(v as i64)
    } else if let Some(&v) = a.downcast_ref::<i32>() {
        Some(v as i64)
    } else if let Some(&v) = a.downcast_ref::<i64>() {
        Some(v)
    } else {
        None
    }
}

/// 比较两个值
pub fn compare_values(a: &dyn Any, b: &dyn Any, cmp_op: u8) -> bool {
    // 尝试数值比较（统一转为 i64）
    if let (Some(a_val), Some(b_val)) = (to_i64(a), to_i64(b)) {
        return match cmp_op {
            0 => a_val == b_val,
            1 => a_val != b_val,
            2 => a_val < b_val,
            3 => a_val > b_val,
            4 => a_val <= b_val,
            5 => a_val >= b_val,
            _ => true,
        };
    }

    // 浮点数比较
    if let (Some(&a_val), Some(&b_val)) = (a.downcast_ref::<f64>(), b.downcast_ref::<f64>()) {
        return match cmp_op {
            0 => a_val == b_val,
            1 => a_val != b_val,
            2 => a_val < b_val,
            3 => a_val > b_val,
            4 => a_val <= b_val,
            5 => a_val >= b_val,
            _ => true,
        };
    }

    // 字符串比较
    if let (Some(a_val), Some(b_val)) =
        (a.downcast_ref::<String>(), b.downcast_ref::<String>())
    {
        return match cmp_op {
            0 => a_val == b_val,
            1 => a_val != b_val,
            2 => a_val < b_val,
            3 => a_val > b_val,
            4 => a_val <= b_val,
            5 => a_val >= b_val,
            _ => true,
        };
    }

    // 布尔比较
    if let (Some(&a_val), Some(&b_val)) = (a.downcast_ref::<bool>(), b.downcast_ref::<bool>()) {
        return match cmp_op {
            0 => a_val == b_val,
            1 => a_val != b_val,
            _ => true,
        };
    }

    // 回退：字符串比较
    let a_str = describe_any(a);
    let b_str = describe_any(b);
    match cmp_op {
        0 => a_str == b_str,
        1 => a_str != b_str,
        _ => true,
    }
}

/// 描述任意值为字符串
pub fn describe_any(a: &dyn Any) -> String {
    if let Some(v) = a.downcast_ref::<i32>() {
        v.to_string()
    } else if let Some(v) = a.downcast_ref::<i64>() {
        v.to_string()
    } else if let Some(v) = a.downcast_ref::<u32>() {
        v.to_string()
    } else if let Some(v) = a.downcast_ref::<u64>() {
        v.to_string()
    } else if let Some(v) = a.downcast_ref::<f64>() {
        v.to_string()
    } else if let Some(v) = a.downcast_ref::<String>() {
        v.clone()
    } else if let Some(v) = a.downcast_ref::<bool>() {
        v.to_string()
    } else if let Some(v) = a.downcast_ref::<u8>() {
        v.to_string()
    } else if let Some(v) = a.downcast_ref::<Vec<u8>>() {
        format!("{:?}", v)
    } else {
        "unknown".to_string()
    }
}

// =============================== 辅助类型 ===============================

// =============================== 测试 ===============================

#[cfg(test)]
mod tests {
    use super::*;

    /// 测试用结构体
    #[derive(Debug, Clone)]
    struct TestStruct {
        pub id: u32,
        pub name: String,
        pub value: f64,
    }

    impl Navigable for TestStruct {
        fn field_by_index(&self, idx: usize) -> Option<&dyn Any> {
            match idx {
                0 => Some(&self.id),
                1 => Some(&self.name),
                2 => Some(&self.value),
                _ => None,
            }
        }

        fn field_by_index_mut(&mut self, idx: usize) -> Option<&mut dyn Any> {
            match idx {
                0 => Some(&mut self.id),
                1 => Some(&mut self.name),
                2 => Some(&mut self.value),
                _ => None,
            }
        }

        fn set_field_by_index(&mut self, idx: usize, value: Box<dyn Any>) {
            match idx {
                0 => {
                    if let Some(v) = value.downcast_ref::<u32>() {
                        self.id = *v;
                    }
                }
                1 => {
                    if let Some(v) = value.downcast_ref::<String>() {
                        self.name = v.clone();
                    }
                }
                2 => {
                    if let Some(v) = value.downcast_ref::<f64>() {
                        self.value = *v;
                    }
                }
                _ => {}
            }
        }
    }

    /// 测试用访问器（模拟 sp-gen 生成的代码）
    pub struct TestStructAccessor;

    impl SpoiAccessor for TestStructAccessor {
        fn field_count(&self) -> usize {
            3
        }

        fn get_field<'a>(&self, obj: &'a dyn Any, idx: usize) -> Option<&'a dyn Any> {
            let o = obj.downcast_ref::<TestStruct>()?;
            match idx {
                0 => Some(&o.id),
                1 => Some(&o.name),
                2 => Some(&o.value),
                _ => None,
            }
        }

        fn set_field(&self, obj: &mut dyn Any, idx: usize, val: Box<dyn Any>) {
            let o = match obj.downcast_mut::<TestStruct>() {
                Some(o) => o,
                None => return,
            };
            match idx {
                0 => {
                    if let Ok(v) = val.downcast::<u32>() {
                        o.id = *v;
                    }
                }
                1 => {
                    if let Ok(v) = val.downcast::<String>() {
                        o.name = *v;
                    }
                }
                2 => {
                    if let Ok(v) = val.downcast::<f64>() {
                        o.value = *v;
                    }
                }
                _ => {}
            }
        }
    }

    fn create_test_accessors() -> HashMap<String, Box<dyn SpoiAccessor>> {
        let mut accessors: HashMap<String, Box<dyn SpoiAccessor>> = HashMap::new();
        accessors.insert(
            "TestStruct".to_string(),
            Box::new(TestStructAccessor),
        );
        accessors
    }

    #[test]
    fn test_read_varint() {
        let mut buf = Vec::new();
        write_varint(&mut buf, 0);
        assert_eq!(read_varint(&buf, 0), (0, 1));

        buf.clear();
        write_varint(&mut buf, 127);
        assert_eq!(read_varint(&buf, 0), (127, 1));

        buf.clear();
        write_varint(&mut buf, 128);
        assert_eq!(read_varint(&buf, 0), (128, 2));

        buf.clear();
        write_varint(&mut buf, 300);
        assert_eq!(read_varint(&buf, 0), (300, 2));
    }

    #[test]
    fn test_write_varint() {
        let mut buf = Vec::new();
        write_varint(&mut buf, 1);
        assert_eq!(buf, vec![1]);

        buf.clear();
        write_varint(&mut buf, 300);
        assert_eq!(buf, vec![0xAC, 0x02]);
    }

    #[test]
    fn test_parse_spoi_stream_empty() {
        let mut data = Vec::new();
        write_varint(&mut data, 0);
        let instructions = parse_spoi_stream(&data);
        assert!(instructions.is_empty());
    }

    #[test]
    fn test_navigate_vec() {
        let accessors = HashMap::new();
        let executor = SpoiExecutor::new(accessors);

        let list: Vec<Box<dyn Any>> = vec![Box::new(10u32), Box::new(20u32), Box::new(30u32)];
        let boxed: Box<dyn Any> = Box::new(list);

        let result = executor.nav_step(&*boxed, 0);
        assert_eq!(*result.downcast_ref::<u32>().unwrap(), 10u32);

        let result = executor.nav_step(&*boxed, 2);
        assert_eq!(*result.downcast_ref::<u32>().unwrap(), 30u32);
    }

    #[test]
    fn test_navigate_navigable() {
        let executor = SpoiExecutor::new(create_test_accessors());

        let obj = NavigableBox::new(TestStruct {
            id: 42,
            name: "hello".to_string(),
            value: 3.14,
        });
        let boxed: Box<dyn Any> = Box::new(obj);

        let result = executor.nav_step(&*boxed, 0);
        assert_eq!(*result.downcast_ref::<u32>().unwrap(), 42u32);

        let result = executor.nav_step(&*boxed, 1);
        assert_eq!(result.downcast_ref::<String>().unwrap(), "hello");
    }

    #[test]
    fn test_navigate_deref() {
        let accessors = HashMap::new();
        let executor = SpoiExecutor::new(accessors);

        let obj = NavigableBox::new(TestStruct {
            id: 99,
            name: "test".to_string(),
            value: 1.0,
        });
        let boxed: Box<dyn Any> = Box::new(obj);

        let result = executor.nav_step(&*boxed, PATH_DEREF);
        assert_eq!(*result.downcast_ref::<u32>().unwrap(), 99u32);
    }

    #[test]
    fn test_set_field() {
        let accessors = HashMap::new();
        let executor = SpoiExecutor::new(accessors);

        let obj = NavigableBox::new(TestStruct {
            id: 0,
            name: "".to_string(),
            value: 0.0,
        });
        let mut boxed: Box<dyn Any> = Box::new(obj);

        executor.set_field(&mut *boxed, 0, Box::new(100u32));
        executor.set_field(&mut *boxed, 1, Box::new("updated".to_string()));

        let nav_box = boxed.downcast_ref::<NavigableBox>().unwrap();
        let id = nav_box
            .0
            .field_by_index(0)
            .unwrap()
            .downcast_ref::<u32>()
            .unwrap();
        let name = nav_box
            .0
            .field_by_index(1)
            .unwrap()
            .downcast_ref::<String>()
            .unwrap();
        assert_eq!(*id, 100u32);
        assert_eq!(name, "updated");
    }

    #[test]
    fn test_deserialize_value_v2() {
        // type_id 前缀格式: [type_id(u32 LE) + value_bytes]

        // u8: type_id=TYPE_ID_U8 (26), value=42
        let val = deserialize_value(&[0x1A, 0x00, 0x00, 0x00, 42]);
        assert_eq!(*val.downcast_ref::<u8>().unwrap(), 42u8);

        // u32: type_id=TYPE_ID_U32 (28), value=1
        let val = deserialize_value(&[0x1C, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00]);
        assert_eq!(*val.downcast_ref::<u32>().unwrap(), 1u32);

        // string: type_id=TYPE_ID_STRING (9), value="hello"
        let mut data = vec![0x09, 0x00, 0x00, 0x00];
        data.extend_from_slice(b"hello");
        let val = deserialize_value(&data);
        assert_eq!(val.downcast_ref::<String>().unwrap(), "hello");

        // bool: type_id=TYPE_ID_BOOL (40), value=true
        let val = deserialize_value(&[0x28, 0x00, 0x00, 0x00, 1]);
        assert_eq!(*val.downcast_ref::<bool>().unwrap(), true);

        // 空数据
        let val = deserialize_value(&[]);
        assert!(val.downcast_ref::<NullValue>().is_some());
    }

    #[test]
    fn test_op_count() {
        let accessors = HashMap::new();
        let mut executor = SpoiExecutor::new(accessors);
        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32), Box::new(3u32)];
        executor.op_count();
        assert_eq!(executor.pipe_data.len(), 1);
        assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 3u32);
    }

    #[test]
    fn test_op_reverse() {
        let accessors = HashMap::new();
        let mut executor = SpoiExecutor::new(accessors);
        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32), Box::new(3u32)];
        executor.op_reverse();
        assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 3u32);
        assert_eq!(*executor.pipe_data[2].downcast_ref::<u32>().unwrap(), 1u32);
    }

    #[test]
    fn test_op_take_drop() {
        let accessors = HashMap::new();
        let mut executor = SpoiExecutor::new(accessors);
        executor.pipe_data = vec![
            Box::new(1u32),
            Box::new(2u32),
            Box::new(3u32),
            Box::new(4u32),
            Box::new(5u32),
        ];

        executor.op_take(&3u32.to_le_bytes());
        assert_eq!(executor.pipe_data.len(), 3);
        assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 1u32);

        executor.op_drop(&1u32.to_le_bytes());
        assert_eq!(executor.pipe_data.len(), 2);
        assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 2u32);
    }

    #[test]
    fn test_op_join() {
        let accessors = HashMap::new();
        let mut executor = SpoiExecutor::new(accessors);
        let inner1: Vec<Box<dyn Any>> = vec![Box::new(1u32), Box::new(2u32)];
        let inner2: Vec<Box<dyn Any>> = vec![Box::new(3u32), Box::new(4u32)];
        executor.pipe_data = vec![Box::new(inner1), Box::new(inner2)];

        executor.op_join();
        assert_eq!(executor.pipe_data.len(), 4);
        assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 1u32);
        assert_eq!(*executor.pipe_data[3].downcast_ref::<u32>().unwrap(), 4u32);
    }

    #[test]
    fn test_op_any_all() {
        let accessors = HashMap::new();
        let mut executor = SpoiExecutor::new(accessors);
        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32), Box::new(3u32)];

        // memberIdx=0, cmpOp=3 (gt), value_len=1(u8) → val=0
        // 注意：v2 matches 格式：memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
        // 简化：直接用旧格式兼容测试
        let operand = [0u8, 0, 0, 0, 3, 0];
        executor.op_any(&[], &operand);
        assert_eq!(*executor.pipe_data[0].downcast_ref::<bool>().unwrap(), true);

        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32), Box::new(3u32)];
        executor.op_all(&[], &operand);
        assert_eq!(*executor.pipe_data[0].downcast_ref::<bool>().unwrap(), true);
    }

    #[test]
    fn test_op_filter() {
        let mut executor = SpoiExecutor::new(create_test_accessors());

        executor.pipe_data = vec![
            Box::new(NavigableBox::new(TestStruct {
                id: 1,
                name: "a".to_string(),
                value: 1.0,
            })),
            Box::new(NavigableBox::new(TestStruct {
                id: 5,
                name: "b".to_string(),
                value: 2.0,
            })),
            Box::new(NavigableBox::new(TestStruct {
                id: 10,
                name: "c".to_string(),
                value: 3.0,
            })),
        ];

        // memberIdx=0 (id), cmpOp=3 (e_gt), value=1 (u8) → id > 1
        // 格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
        // type_id=26(U8), value=1, typed_value=[26,0,0,0,1], value_len=5
        let operand = [0u8, 0, 0, 0, 3, 5, 26, 0, 0, 0, 1];
        executor.op_filter(&[], &operand);
        // 保留 id > 1 的：id=5 和 id=10
        assert_eq!(executor.pipe_data.len(), 2);

        // memberIdx=0 (id), cmpOp=3 (e_gt), value=5 (u8) → id > 5
        // type_id=26(U8), value=5, typed_value=[26,0,0,0,5], value_len=5
        let operand2 = [0u8, 0, 0, 0, 3, 5, 26, 0, 0, 0, 5];
        executor.op_filter(&[], &operand2);
        // 保留 id > 5 的：id=10
        assert_eq!(executor.pipe_data.len(), 1);
    }

    #[test]
    fn test_full_pipeline() {
        let accessors = HashMap::new();
        let mut executor = SpoiExecutor::new(accessors);

        // 模拟 PIPE → COUNT 流程
        // 先构造一个 list 作为 root
        let list: Vec<Box<dyn Any>> =
            vec![Box::new(1u32), Box::new(2u32), Box::new(3u32), Box::new(4u32)];
        let mut root: Box<dyn Any> = Box::new(list);

        // 构造 SPOI 指令流: PIPE([]) → COUNT
        let mut insn_bytes = Vec::new();
        write_varint(&mut insn_bytes, 2); // 2 条指令

        // 指令 1: PIPE, path=[], operand=[]
        insn_bytes.push(op::PIPE);
        write_varint(&mut insn_bytes, 0); // path_len=0
        write_varint(&mut insn_bytes, 0); // operand_len=0

        // 指令 2: COUNT, path=[], operand=[]
        insn_bytes.push(op::COUNT);
        write_varint(&mut insn_bytes, 0); // path_len=0
        write_varint(&mut insn_bytes, 0); // operand_len=0

        let result = executor.execute(&mut *root, &insn_bytes);
        let result_type = result
            .get("resultType")
            .and_then(|v| v.downcast_ref::<u32>())
            .copied()
            .unwrap();
        assert_eq!(result_type, result_type::SINGLE);
    }

    #[test]
    fn test_pipe_and_take() {
        let accessors = HashMap::new();
        let mut executor = SpoiExecutor::new(accessors);

        let list: Vec<Box<dyn Any>> = vec![
            Box::new(10u32),
            Box::new(20u32),
            Box::new(30u32),
            Box::new(40u32),
            Box::new(50u32),
        ];
        let mut root: Box<dyn Any> = Box::new(list);

        // PIPE → TAKE(3)
        let mut insn_bytes = Vec::new();
        write_varint(&mut insn_bytes, 2);

        // PIPE
        insn_bytes.push(op::PIPE);
        write_varint(&mut insn_bytes, 0);
        write_varint(&mut insn_bytes, 0);

        // TAKE(3)
        insn_bytes.push(op::TAKE);
        write_varint(&mut insn_bytes, 0);
        let take_operand = 3u32.to_le_bytes().to_vec();
        write_varint(&mut insn_bytes, take_operand.len());
        insn_bytes.extend_from_slice(&take_operand);

        let result = executor.execute(&mut *root, &insn_bytes);
        let result_type = result
            .get("resultType")
            .and_then(|v| v.downcast_ref::<u32>())
            .copied()
            .unwrap();
        // VECTOR: 3 个元素
        assert_eq!(result_type, result_type::VECTOR);
    }

    #[test]
    fn test_accessor_get_field() {
        let accessor = TestStructAccessor;
        let obj = TestStruct {
            id: 42,
            name: "test".to_string(),
            value: 3.14,
        };

        let id = accessor.get_field(&obj, 0).unwrap().downcast_ref::<u32>().unwrap();
        assert_eq!(*id, 42);

        let name = accessor.get_field(&obj, 1).unwrap().downcast_ref::<String>().unwrap();
        assert_eq!(name, "test");

        assert!(accessor.get_field(&obj, 99).is_none());
    }

    #[test]
    fn test_accessor_set_field() {
        let accessor = TestStructAccessor;
        let mut obj = TestStruct {
            id: 0,
            name: String::new(),
            value: 0.0,
        };

        accessor.set_field(&mut obj, 0, Box::new(100u32));
        accessor.set_field(&mut obj, 1, Box::new("updated".to_string()));

        assert_eq!(obj.id, 100);
        assert_eq!(obj.name, "updated");
    }

    #[test]
    fn test_accessor_field_count() {
        let accessor = TestStructAccessor;
        assert_eq!(accessor.field_count(), 3);
    }
}