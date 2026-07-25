// =================== StreamPunk SPOI Executor 测试 ===================
// 编译: rustc --edition 2021 --test test_spoi_executor.rs
// 运行: ./test_spoi_executor.exe

mod spoi_executor;

use spoi_executor::{
    clone_any,
    Navigable, NavigableBox, SpoiExecutor, SpoiInstruction,
    read_varint, write_varint, parse_spoi_stream,
    op, result_type, PATH_DEREF,
};
use std::any::Any;
use std::collections::HashMap;

// =============================== 测试数据类型 ===============================

struct Item {
    name: String,
    price: u32,
}

impl Navigable for Item {
    fn field_by_index(&self, idx: usize) -> Option<&dyn Any> {
        match idx {
            0 => Some(&self.name),
            1 => Some(&self.price),
            _ => None,
        }
    }

    fn field_by_index_mut(&mut self, idx: usize) -> Option<&mut dyn Any> {
        match idx {
            0 => Some(&mut self.name),
            1 => Some(&mut self.price),
            _ => None,
        }
    }

    fn set_field_by_index(&mut self, idx: usize, value: Box<dyn Any>) {
        match idx {
            0 => {
                if let Some(v) = value.downcast_ref::<String>() {
                    self.name = v.clone();
                }
            }
            1 => {
                if let Some(v) = value.downcast_ref::<u32>() {
                    self.price = *v;
                }
            }
            _ => {}
        }
    }
}

struct Player {
    name: String,
    level: u32,
    health: u32,
    items: Vec<Box<dyn Any>>,
    metadata: HashMap<String, Box<dyn Any>>,
}

impl Navigable for Player {
    fn field_by_index(&self, idx: usize) -> Option<&dyn Any> {
        match idx {
            0 => Some(&self.name),
            1 => Some(&self.level),
            2 => Some(&self.health),
            3 => Some(&self.items),
            4 => Some(&self.metadata),
            _ => None,
        }
    }

    fn field_by_index_mut(&mut self, idx: usize) -> Option<&mut dyn Any> {
        match idx {
            0 => Some(&mut self.name),
            1 => Some(&mut self.level),
            2 => Some(&mut self.health),
            3 => Some(&mut self.items),
            4 => Some(&mut self.metadata),
            _ => None,
        }
    }

    fn set_field_by_index(&mut self, idx: usize, value: Box<dyn Any>) {
        match idx {
            0 => {
                if let Some(v) = value.downcast_ref::<String>() {
                    self.name = v.clone();
                }
            }
            1 => {
                if let Some(v) = value.downcast_ref::<u32>() {
                    self.level = *v;
                }
            }
            2 => {
                if let Some(v) = value.downcast_ref::<u32>() {
                    self.health = *v;
                }
            }
            3 => {
                if let Some(v) = value.downcast_ref::<Vec<Box<dyn Any>>>() {
                    self.items = v.iter().map(|item| clone_any(item)).collect();
                }
            }
            4 => {
                if let Some(v) = value.downcast_ref::<HashMap<String, Box<dyn Any>>>() {
                    self.metadata = v
                        .iter()
                        .map(|(k, val)| (k.clone(), clone_any(val)))
                        .collect();
                }
            }
            _ => {}
        }
    }
}

// =============================== 辅助函数 ===============================

/// 将 SpoiInstruction 切片序列化为 SPOI 字节流
fn build_spoi_stream(instructions: &[SpoiInstruction]) -> Vec<u8> {
    let mut buf = Vec::new();
    write_varint(&mut buf, instructions.len());
    for inst in instructions {
        buf.push(inst.op);
        write_varint(&mut buf, inst.path.len());
        for seg in &inst.path {
            write_varint(&mut buf, *seg);
        }
        write_varint(&mut buf, inst.operand.len());
        buf.extend_from_slice(&inst.operand);
    }
    buf
}

#[allow(dead_code)]
fn make_inst(op: u8, path: Vec<usize>, operand: Vec<u8>) -> SpoiInstruction {
    SpoiInstruction { op, path, operand }
}

#[allow(dead_code)]
fn set_int(path: Vec<usize>, value: u32) -> SpoiInstruction {
    SpoiInstruction {
        op: op::SET,
        path,
        operand: value.to_le_bytes().to_vec(),
    }
}

#[allow(dead_code)]
fn set_str(path: Vec<usize>, value: &str) -> SpoiInstruction {
    SpoiInstruction {
        op: op::SET,
        path,
        operand: value.as_bytes().to_vec(),
    }
}

fn pipe_inst(path: Vec<usize>) -> SpoiInstruction {
    SpoiInstruction {
        op: op::PIPE,
        path,
        operand: Vec::new(),
    }
}

fn filter_gt(path: Vec<usize>, member_idx: u32, value: u32) -> SpoiInstruction {
    let mut operand = Vec::new();
    operand.extend_from_slice(&member_idx.to_le_bytes());
    operand.push(3); // cmp_op: >
    operand.extend_from_slice(&value.to_le_bytes());
    SpoiInstruction {
        op: op::FILTER,
        path,
        operand,
    }
}

#[allow(dead_code)]
fn filter_eq(path: Vec<usize>, member_idx: u32, value: u32) -> SpoiInstruction {
    let mut operand = Vec::new();
    operand.extend_from_slice(&member_idx.to_le_bytes());
    operand.push(0); // cmp_op: ==
    operand.extend_from_slice(&value.to_le_bytes());
    SpoiInstruction {
        op: op::FILTER,
        path,
        operand,
    }
}

fn select_inst(path: Vec<usize>) -> SpoiInstruction {
    SpoiInstruction {
        op: op::SELECT,
        path,
        operand: Vec::new(),
    }
}

fn sort_inst(path: Vec<usize>) -> SpoiInstruction {
    SpoiInstruction {
        op: op::SORT,
        path,
        operand: Vec::new(),
    }
}

fn take_inst(n: u32) -> SpoiInstruction {
    SpoiInstruction {
        op: op::TAKE,
        path: Vec::new(),
        operand: n.to_le_bytes().to_vec(),
    }
}

#[allow(dead_code)]
fn drop_inst(n: u32) -> SpoiInstruction {
    SpoiInstruction {
        op: op::DROP,
        path: Vec::new(),
        operand: n.to_le_bytes().to_vec(),
    }
}

#[allow(dead_code)]
fn reverse_inst() -> SpoiInstruction {
    SpoiInstruction {
        op: op::REVERSE,
        path: Vec::new(),
        operand: Vec::new(),
    }
}

#[allow(dead_code)]
fn distinct_inst() -> SpoiInstruction {
    SpoiInstruction {
        op: op::DISTINCT,
        path: Vec::new(),
        operand: Vec::new(),
    }
}

fn count_inst() -> SpoiInstruction {
    SpoiInstruction {
        op: op::COUNT,
        path: Vec::new(),
        operand: Vec::new(),
    }
}

#[allow(dead_code)]
fn any_inst(member_idx: u32, cmp_op: u8, value: u32) -> SpoiInstruction {
    let mut operand = Vec::new();
    operand.extend_from_slice(&member_idx.to_le_bytes());
    operand.push(cmp_op);
    operand.extend_from_slice(&value.to_le_bytes());
    SpoiInstruction {
        op: op::ANY,
        path: Vec::new(),
        operand,
    }
}

#[allow(dead_code)]
fn all_inst(member_idx: u32, cmp_op: u8, value: u32) -> SpoiInstruction {
    let mut operand = Vec::new();
    operand.extend_from_slice(&member_idx.to_le_bytes());
    operand.push(cmp_op);
    operand.extend_from_slice(&value.to_le_bytes());
    SpoiInstruction {
        op: op::ALL,
        path: Vec::new(),
        operand,
    }
}

#[allow(dead_code)]
fn find_inst(member_idx: u32, cmp_op: u8, value: u32) -> SpoiInstruction {
    let mut operand = Vec::new();
    operand.extend_from_slice(&member_idx.to_le_bytes());
    operand.push(cmp_op);
    operand.extend_from_slice(&value.to_le_bytes());
    SpoiInstruction {
        op: op::FIND,
        path: Vec::new(),
        operand,
    }
}

fn exec_inst() -> SpoiInstruction {
    SpoiInstruction {
        op: op::EXEC,
        path: Vec::new(),
        operand: Vec::new(),
    }
}

/// 创建带类型注册表的 SpoiExecutor
fn new_executor() -> SpoiExecutor {
    let mut reg = HashMap::new();
    reg.insert(
        "Item".to_string(),
        vec!["name".to_string(), "price".to_string()],
    );
    reg.insert(
        "Player".to_string(),
        vec![
            "name".to_string(),
            "level".to_string(),
            "health".to_string(),
            "items".to_string(),
            "metadata".to_string(),
        ],
    );
    SpoiExecutor::new(reg)
}

/// 构造 Item 包装为 NavigableBox 再放入 Box<dyn Any>
fn box_item(name: &str, price: u32) -> Box<dyn Any> {
    Box::new(NavigableBox::new(Item {
        name: name.to_string(),
        price,
    }))
}

/// 构造 Player 包装为 NavigableBox 再放入 Box<dyn Any>
fn box_player(name: &str, level: u32, health: u32) -> Box<dyn Any> {
    Box::new(NavigableBox::new(Player {
        name: name.to_string(),
        level,
        health,
        items: Vec::new(),
        metadata: HashMap::new(),
    }))
}

// =============================== 测试用例 ===============================

// -----------------------------------------------------------------------
// 1. varint roundtrip
// -----------------------------------------------------------------------
#[test]
fn test_varint_roundtrip() {
    let test_values = vec![0usize, 1, 127, 128, 255, 256, 300, 16383, 16384, 65535, 1_000_000];
    for v in &test_values {
        let mut buf = Vec::new();
        write_varint(&mut buf, *v);
        let (decoded, offset) = read_varint(&buf, 0);
        assert_eq!(decoded, *v, "varint roundtrip failed for value {}", v);
        assert!(offset <= buf.len());
    }
}

// -----------------------------------------------------------------------
// 2. parse instruction stream
// -----------------------------------------------------------------------
#[test]
fn test_parse_instruction_stream() {
    let insts = vec![
        pipe_inst(vec![]),
        filter_gt(vec![], 1, 50),
        select_inst(vec![0]),
        take_inst(3),
        exec_inst(),
    ];
    let stream = build_spoi_stream(&insts);
    let parsed = parse_spoi_stream(&stream);

    assert_eq!(parsed.len(), insts.len());
    assert_eq!(parsed[0].op, op::PIPE);
    assert_eq!(parsed[1].op, op::FILTER);
    assert_eq!(parsed[2].op, op::SELECT);
    assert_eq!(parsed[3].op, op::TAKE);
    assert_eq!(parsed[4].op, op::EXEC);

    assert_eq!(parsed[1].path.len(), 0);
    assert_eq!(parsed[1].operand.len(), 9); // 4(memberIdx) + 1(cmpOp) + 4(value)
    assert_eq!(parsed[1].operand[4], 3); // cmpOp=>

    assert_eq!(parsed[2].path, vec![0]);

    assert_eq!(parsed[3].operand, 3u32.to_le_bytes().to_vec());
}

// -----------------------------------------------------------------------
// 3. basic navigation
// -----------------------------------------------------------------------
#[test]
fn test_basic_navigation_item() {
    let executor = new_executor();
    let item = NavigableBox::new(Item {
        name: "Sword".to_string(),
        price: 100,
    });
    let boxed: Box<dyn Any> = Box::new(item);

    let name_val = executor.nav_step(&*boxed, 0);
    assert_eq!(name_val.downcast_ref::<String>().unwrap(), "Sword");

    let price_val = executor.nav_step(&*boxed, 1);
    assert_eq!(*price_val.downcast_ref::<u32>().unwrap(), 100);
}

#[test]
fn test_basic_navigation_player() {
    let executor = new_executor();
    let player = box_player("Hero", 10, 100);
    let boxed: Box<dyn Any> = player;

    assert_eq!(
        executor.nav_step(&*boxed, 0).downcast_ref::<String>().unwrap(),
        "Hero"
    );
    assert_eq!(
        *executor.nav_step(&*boxed, 1).downcast_ref::<u32>().unwrap(),
        10
    );
    assert_eq!(
        *executor.nav_step(&*boxed, 2).downcast_ref::<u32>().unwrap(),
        100
    );
}

#[test]
fn test_basic_navigation_deref() {
    let executor = new_executor();
    let item = NavigableBox::new(Item {
        name: "Potion".to_string(),
        price: 10,
    });
    let boxed: Box<dyn Any> = Box::new(item);

    let result = executor.nav_step(&*boxed, PATH_DEREF);
    assert_eq!(result.downcast_ref::<String>().unwrap(), "Potion");
}

// -----------------------------------------------------------------------
// 4. SET operation
// -----------------------------------------------------------------------
#[test]
fn test_op_set() {
    let mut executor = new_executor();
    let item = NavigableBox::new(Item {
        name: "Old".to_string(),
        price: 0,
    });
    let mut root: Box<dyn Any> = Box::new(item);

    executor.op_set(&mut *root, &[0], "NewName".as_bytes());
    let nav_box = root.downcast_ref::<NavigableBox>().unwrap();
    let name = nav_box.0.field_by_index(0).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(name, "NewName");

    executor.op_set(&mut *root, &[1], &500u32.to_le_bytes());
    let nav_box2 = root.downcast_ref::<NavigableBox>().unwrap();
    let price = nav_box2.0.field_by_index(1).unwrap().downcast_ref::<u32>().unwrap();
    assert_eq!(*price, 500);
}

// -----------------------------------------------------------------------
// 5. ADD operation
// -----------------------------------------------------------------------
#[test]
fn test_op_add() {
    let mut executor = new_executor();
    let item = NavigableBox::new(Item {
        name: "Test".to_string(),
        price: 100,
    });
    let mut root: Box<dyn Any> = Box::new(item);

    executor.op_add(&mut *root, &[1], &50u32.to_le_bytes());

    let nav_box = root.downcast_ref::<NavigableBox>().unwrap();
    let price = nav_box.0.field_by_index(1).unwrap().downcast_ref::<u32>().unwrap();
    assert_eq!(*price, 150);
}

// -----------------------------------------------------------------------
// 6. PIPE operation
// -----------------------------------------------------------------------
#[test]
fn test_op_pipe() {
    let mut executor = new_executor();

    let list: Vec<Box<dyn Any>> = vec![Box::new(10u32), Box::new(20u32), Box::new(30u32)];
    let root: Box<dyn Any> = Box::new(list);

    executor.op_pipe(&*root, &[]);
    assert_eq!(executor.pipe_data.len(), 3);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 10u32);
    assert_eq!(*executor.pipe_data[2].downcast_ref::<u32>().unwrap(), 30u32);
}

// -----------------------------------------------------------------------
// 7. FILTER operation
// -----------------------------------------------------------------------
#[test]
fn test_op_filter() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        box_item("Sword", 100),
        box_item("Shield", 50),
        box_item("Potion", 10),
        box_item("Armor", 200),
        box_item("Arrow", 5),
    ];

    // memberIdx=1(price), cmpOp=3(>), value=50 → price > 50
    let mut operand = Vec::new();
    operand.extend_from_slice(&1u32.to_le_bytes());
    operand.push(3); // cmp_op: >
    operand.extend_from_slice(&50u32.to_le_bytes());
    executor.op_filter(&[], &operand);

    assert_eq!(executor.pipe_data.len(), 2); // Sword(100), Armor(200)

    let first = executor.pipe_data[0]
        .downcast_ref::<NavigableBox>()
        .unwrap();
    let fname = first.0.field_by_index(0).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(fname, "Sword");
}

#[test]
fn test_op_filter_eq() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        box_item("Sword", 100),
        box_item("Sword", 100),
        box_item("Potion", 10),
    ];

    // memberIdx=1(price), cmpOp=0(==), value=100
    let mut operand = Vec::new();
    operand.extend_from_slice(&1u32.to_le_bytes());
    operand.push(0); // cmp_op: ==
    operand.extend_from_slice(&100u32.to_le_bytes());
    executor.op_filter(&[], &operand);

    assert_eq!(executor.pipe_data.len(), 2);
}

// -----------------------------------------------------------------------
// 8. SELECT operation
// -----------------------------------------------------------------------
#[test]
fn test_op_select() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        box_item("Sword", 100),
        box_item("Shield", 50),
        box_item("Potion", 10),
    ];

    executor.op_select(&[0]);
    assert_eq!(executor.pipe_data.len(), 3);
    assert_eq!(
        executor.pipe_data[0].downcast_ref::<String>().unwrap(),
        "Sword"
    );
    assert_eq!(
        executor.pipe_data[1].downcast_ref::<String>().unwrap(),
        "Shield"
    );
    assert_eq!(
        executor.pipe_data[2].downcast_ref::<String>().unwrap(),
        "Potion"
    );

    // SELECT price (field index 1)
    executor.pipe_data = vec![
        box_item("Sword", 100),
        box_item("Shield", 50),
    ];
    executor.op_select(&[1]);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 100);
    assert_eq!(*executor.pipe_data[1].downcast_ref::<u32>().unwrap(), 50);
}

// -----------------------------------------------------------------------
// 9. SORT operation
// -----------------------------------------------------------------------
#[test]
fn test_op_sort() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new(30u32),
        Box::new(10u32),
        Box::new(20u32),
    ];

    executor.op_sort(&[]);
    assert_eq!(executor.pipe_data.len(), 3);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 10u32);
    assert_eq!(*executor.pipe_data[1].downcast_ref::<u32>().unwrap(), 20u32);
    assert_eq!(*executor.pipe_data[2].downcast_ref::<u32>().unwrap(), 30u32);
}

#[test]
fn test_op_sort_by_field() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        box_item("C", 300),
        box_item("A", 100),
        box_item("B", 200),
    ];

    executor.op_sort(&[0]);
    assert_eq!(executor.pipe_data.len(), 3);
    let first = executor.pipe_data[0]
        .downcast_ref::<NavigableBox>()
        .unwrap();
    let fname = first.0.field_by_index(0).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(fname, "A");
}

// -----------------------------------------------------------------------
// 10. TAKE operation
// -----------------------------------------------------------------------
#[test]
fn test_op_take() {
    let mut executor = new_executor();

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
    assert_eq!(*executor.pipe_data[2].downcast_ref::<u32>().unwrap(), 3u32);
}

#[test]
fn test_op_take_zero() {
    let mut executor = new_executor();
    executor.pipe_data = vec![Box::new(1u32), Box::new(2u32)];

    executor.op_take(&0u32.to_le_bytes());
    assert!(executor.pipe_data.is_empty());
}

// -----------------------------------------------------------------------
// 11. DROP operation
// -----------------------------------------------------------------------
#[test]
fn test_op_drop() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new(1u32),
        Box::new(2u32),
        Box::new(3u32),
        Box::new(4u32),
        Box::new(5u32),
    ];

    executor.op_drop(&2u32.to_le_bytes());
    assert_eq!(executor.pipe_data.len(), 3);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 3u32);
    assert_eq!(*executor.pipe_data[2].downcast_ref::<u32>().unwrap(), 5u32);
}

#[test]
fn test_op_drop_all() {
    let mut executor = new_executor();
    executor.pipe_data = vec![Box::new(1u32), Box::new(2u32)];

    executor.op_drop(&5u32.to_le_bytes()); // n > len
    assert!(executor.pipe_data.is_empty());
}

// -----------------------------------------------------------------------
// 12. REVERSE operation
// -----------------------------------------------------------------------
#[test]
fn test_op_reverse() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new(1u32),
        Box::new(2u32),
        Box::new(3u32),
    ];

    executor.op_reverse();
    assert_eq!(executor.pipe_data.len(), 3);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 3u32);
    assert_eq!(*executor.pipe_data[1].downcast_ref::<u32>().unwrap(), 2u32);
    assert_eq!(*executor.pipe_data[2].downcast_ref::<u32>().unwrap(), 1u32);
}

// -----------------------------------------------------------------------
// 13. DISTINCT operation
// -----------------------------------------------------------------------
#[test]
fn test_op_distinct() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new(1u32),
        Box::new(2u32),
        Box::new(2u32),
        Box::new(3u32),
        Box::new(1u32),
    ];

    executor.op_distinct();
    assert_eq!(executor.pipe_data.len(), 3);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 1u32);
    assert_eq!(*executor.pipe_data[1].downcast_ref::<u32>().unwrap(), 2u32);
    assert_eq!(*executor.pipe_data[2].downcast_ref::<u32>().unwrap(), 3u32);
}

#[test]
fn test_op_distinct_strings() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new("a".to_string()),
        Box::new("b".to_string()),
        Box::new("a".to_string()),
        Box::new("c".to_string()),
        Box::new("b".to_string()),
    ];

    executor.op_distinct();
    assert_eq!(executor.pipe_data.len(), 3);
}

// -----------------------------------------------------------------------
// 14. COUNT operation
// -----------------------------------------------------------------------
#[test]
fn test_op_count() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new(1u32),
        Box::new(2u32),
        Box::new(3u32),
        Box::new(4u32),
    ];

    executor.op_count();
    assert_eq!(executor.pipe_data.len(), 1);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 4u32);
}

#[test]
fn test_op_count_empty() {
    let mut executor = new_executor();

    executor.op_count();
    assert_eq!(executor.pipe_data.len(), 1);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap(), 0u32);
}

// -----------------------------------------------------------------------
// 15. ANY operation
// -----------------------------------------------------------------------
#[test]
fn test_op_any_true() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new(1u32),
        Box::new(5u32),
        Box::new(10u32),
    ];

    // memberIdx=0, cmpOp=3(>), value=5 → any element > 5?
    let mut operand = Vec::new();
    operand.extend_from_slice(&0u32.to_le_bytes());
    operand.push(3); // >
    operand.extend_from_slice(&5u32.to_le_bytes());
    executor.op_any(&[], &operand);

    assert_eq!(executor.pipe_data.len(), 1);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<bool>().unwrap(), true);
}

#[test]
fn test_op_any_false() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new(1u32),
        Box::new(2u32),
        Box::new(3u32),
    ];

    // memberIdx=0, cmpOp=3(>), value=10 → any element > 10?
    let mut operand = Vec::new();
    operand.extend_from_slice(&0u32.to_le_bytes());
    operand.push(3); // >
    operand.extend_from_slice(&10u32.to_le_bytes());
    executor.op_any(&[], &operand);

    assert_eq!(executor.pipe_data.len(), 1);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<bool>().unwrap(), false);
}

// -----------------------------------------------------------------------
// 16. ALL operation
// -----------------------------------------------------------------------
#[test]
fn test_op_all_true() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new(10u32),
        Box::new(20u32),
        Box::new(30u32),
    ];

    // memberIdx=0, cmpOp=3(>), value=5 → all elements > 5?
    let mut operand = Vec::new();
    operand.extend_from_slice(&0u32.to_le_bytes());
    operand.push(3); // >
    operand.extend_from_slice(&5u32.to_le_bytes());
    executor.op_all(&[], &operand);

    assert_eq!(executor.pipe_data.len(), 1);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<bool>().unwrap(), true);
}

#[test]
fn test_op_all_false() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        Box::new(1u32),
        Box::new(5u32),
        Box::new(10u32),
    ];

    // memberIdx=0, cmpOp=3(>), value=3 → all elements > 3?
    let mut operand = Vec::new();
    operand.extend_from_slice(&0u32.to_le_bytes());
    operand.push(3); // >
    operand.extend_from_slice(&3u32.to_le_bytes());
    executor.op_all(&[], &operand);

    assert_eq!(executor.pipe_data.len(), 1);
    assert_eq!(*executor.pipe_data[0].downcast_ref::<bool>().unwrap(), false);
}

// -----------------------------------------------------------------------
// 17. FIND operation
// -----------------------------------------------------------------------
#[test]
fn test_op_find_found() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        box_item("Sword", 100),
        box_item("Shield", 50),
        box_item("Potion", 50),
        box_item("Armor", 200),
    ];

    // memberIdx=1(price), cmpOp=0(==), value=50 → find first with price == 50
    let mut operand = Vec::new();
    operand.extend_from_slice(&1u32.to_le_bytes());
    operand.push(0); // ==
    operand.extend_from_slice(&50u32.to_le_bytes());
    executor.op_find(&[], &operand);

    assert_eq!(executor.pipe_data.len(), 1);
    let found = executor.pipe_data[0]
        .downcast_ref::<NavigableBox>()
        .unwrap();
    let fname = found.0.field_by_index(0).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(fname, "Shield");
}

#[test]
fn test_op_find_not_found() {
    let mut executor = new_executor();

    executor.pipe_data = vec![
        box_item("Sword", 100),
        box_item("Shield", 50),
    ];

    // memberIdx=1(price), cmpOp=0(==), value=999 → not found
    let mut operand = Vec::new();
    operand.extend_from_slice(&1u32.to_le_bytes());
    operand.push(0); // ==
    operand.extend_from_slice(&999u32.to_le_bytes());
    executor.op_find(&[], &operand);

    assert!(executor.pipe_data.is_empty());
}

// -----------------------------------------------------------------------
// 18. Full pipeline: PIPE → FILTER → SELECT → TAKE → EXEC
// -----------------------------------------------------------------------
#[test]
fn test_full_pipeline() {
    let mut executor = new_executor();

    let list: Vec<Box<dyn Any>> = vec![
        Box::new(100u32),
        Box::new(50u32),
        Box::new(10u32),
        Box::new(200u32),
        Box::new(5u32),
    ];
    let mut root: Box<dyn Any> = Box::new(list);

    // PIPE → FILTER(>50) → TAKE(2) → COUNT
    let insts = vec![
        pipe_inst(vec![]),
        filter_gt(vec![], 0, 50),
        take_inst(2),
        count_inst(),
    ];
    let stream = build_spoi_stream(&insts);

    let result = executor.execute(&mut *root, &stream);

    let result_type = result
        .get("resultType")
        .and_then(|v| v.downcast_ref::<u32>())
        .copied()
        .unwrap();
    assert_eq!(result_type, result_type::SINGLE);

    let value = result.get("value").unwrap();
    assert_eq!(*value.downcast_ref::<u32>().unwrap(), 2u32);
}

#[test]
fn test_full_pipeline_single_result() {
    let mut executor = new_executor();

    let list: Vec<Box<dyn Any>> = vec![
        Box::new(10u32),
        Box::new(20u32),
        Box::new(30u32),
    ];
    let mut root: Box<dyn Any> = Box::new(list);

    // PIPE → TAKE(1) → COUNT
    let insts = vec![
        pipe_inst(vec![]),
        take_inst(1),
        count_inst(),
    ];
    let stream = build_spoi_stream(&insts);

    let result = executor.execute(&mut *root, &stream);

    let result_type = result
        .get("resultType")
        .and_then(|v| v.downcast_ref::<u32>())
        .copied()
        .unwrap();
    assert_eq!(result_type, result_type::SINGLE);

    let value = result.get("value").unwrap();
    assert_eq!(*value.downcast_ref::<u32>().unwrap(), 1u32);
}

#[test]
fn test_full_pipeline_with_sort() {
    let mut executor = new_executor();

    let list: Vec<Box<dyn Any>> = vec![
        Box::new(30u32),
        Box::new(10u32),
        Box::new(20u32),
    ];
    let mut root: Box<dyn Any> = Box::new(list);

    // PIPE → SORT → TAKE(2) → EXEC
    let insts = vec![
        pipe_inst(vec![]),
        sort_inst(vec![]),
        take_inst(2),
        exec_inst(),
    ];
    let stream = build_spoi_stream(&insts);

    let result = executor.execute(&mut *root, &stream);

    let result_type = result
        .get("resultType")
        .and_then(|v| v.downcast_ref::<u32>())
        .copied()
        .unwrap();
    assert_eq!(result_type, result_type::VECTOR);

    let values = result
        .get("value")
        .and_then(|v| v.downcast_ref::<Vec<Box<dyn Any>>>())
        .unwrap();
    assert_eq!(values.len(), 2);
    assert_eq!(*values[0].downcast_ref::<u32>().unwrap(), 10u32);
    assert_eq!(*values[1].downcast_ref::<u32>().unwrap(), 20u32);
}