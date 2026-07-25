// =================== StreamPunk SPOI Accessor 测试 ===================
// 测试 spoi_accessor 模块的类型常量、反序列化、访问器、注册表
// 以及 executor 集成

mod spoi_executor;

use spoi_executor::{
    // 类型常量
    TYPE_ID_U8, TYPE_ID_U16, TYPE_ID_U32, TYPE_ID_U64,
    TYPE_ID_I8, TYPE_ID_I16, TYPE_ID_I32, TYPE_ID_I64,
    TYPE_ID_F32, TYPE_ID_F64, TYPE_ID_STRING, TYPE_ID_BOOL,
    // 反序列化
    deserialize_value, NullValue,
    // 访问器 trait 和注册表
    SpoiAccessor, create_spoi_accessor_registry,
    // 访问器具体类型
    SpoiTestPlayerAccessor, SpoiTestStateAccessor,
    SpoiItemAccessor, SpoiInventoryAccessor,
    SpoiCharacterAccessor, SpoiWorldAccessor,
    // 类型定义
    SpoiTestPlayer, SpoiTestState, SpoiItem,
    SpoiInventory, SpoiCharacter, SpoiWorld,
    // Executor 相关
    SpoiExecutor, op, result_type, write_varint,
};
use std::any::Any;

// =============================== 辅助函数 ===============================

/// 构造 type_id(u32 LE) + value_bytes 的字节数组
fn make_typed_value(type_id: u32, value_bytes: &[u8]) -> Vec<u8> {
    let mut data = type_id.to_le_bytes().to_vec();
    data.extend_from_slice(value_bytes);
    data
}

/// 构造布尔值字节
fn bool_bytes(v: bool) -> Vec<u8> {
    vec![v as u8]
}

// =============================== 1. TYPE_ID 常量测试 ===============================

#[test]
fn test_type_id_constants() {
    assert_eq!(TYPE_ID_U8, 26);
    assert_eq!(TYPE_ID_U16, 27);
    assert_eq!(TYPE_ID_U32, 28);
    assert_eq!(TYPE_ID_U64, 29);
    assert_eq!(TYPE_ID_I8, 30);
    assert_eq!(TYPE_ID_I16, 31);
    assert_eq!(TYPE_ID_I32, 32);
    assert_eq!(TYPE_ID_I64, 33);
    assert_eq!(TYPE_ID_F32, 34);
    assert_eq!(TYPE_ID_F64, 35);
    assert_eq!(TYPE_ID_STRING, 9);
    assert_eq!(TYPE_ID_BOOL, 40);
}

// =============================== 2. deserialize_value 测试 ===============================

#[test]
fn test_deserialize_u8() {
    let data = make_typed_value(TYPE_ID_U8, &[42]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<u8>().unwrap(), 42u8);
}

#[test]
fn test_deserialize_u8_empty_value() {
    let data = make_typed_value(TYPE_ID_U8, &[]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<u8>().unwrap(), 0u8);
}

#[test]
fn test_deserialize_u16() {
    let data = make_typed_value(TYPE_ID_U16, &0x3412u16.to_le_bytes());
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<u16>().unwrap(), 0x3412u16);
}

#[test]
fn test_deserialize_u16_short() {
    let data = make_typed_value(TYPE_ID_U16, &[0x42]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<u16>().unwrap(), 0u16);
}

#[test]
fn test_deserialize_u32() {
    let data = make_typed_value(TYPE_ID_U32, &0x78563412u32.to_le_bytes());
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<u32>().unwrap(), 0x78563412u32);
}

#[test]
fn test_deserialize_u32_short() {
    let data = make_typed_value(TYPE_ID_U32, &[0x01, 0x02]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<u32>().unwrap(), 0u32);
}

#[test]
fn test_deserialize_u64() {
    let data = make_typed_value(TYPE_ID_U64, &0xEFCDAB8967452301u64.to_le_bytes());
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<u64>().unwrap(), 0xEFCDAB8967452301u64);
}

#[test]
fn test_deserialize_u64_short() {
    let data = make_typed_value(TYPE_ID_U64, &[0x01, 0x02, 0x03]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<u64>().unwrap(), 0u64);
}

#[test]
fn test_deserialize_i8() {
    let data = make_typed_value(TYPE_ID_I8, &[0xFEu8]); // -2 in two's complement
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<i8>().unwrap(), -2i8);
}

#[test]
fn test_deserialize_i8_positive() {
    let data = make_typed_value(TYPE_ID_I8, &[127]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<i8>().unwrap(), 127i8);
}

#[test]
fn test_deserialize_i8_empty_value() {
    let data = make_typed_value(TYPE_ID_I8, &[]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<i8>().unwrap(), 0i8);
}

#[test]
fn test_deserialize_i16() {
    let data = make_typed_value(TYPE_ID_I16, &(-300i16).to_le_bytes());
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<i16>().unwrap(), -300i16);
}

#[test]
fn test_deserialize_i16_short() {
    let data = make_typed_value(TYPE_ID_I16, &[0x42]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<i16>().unwrap(), 0i16);
}

#[test]
fn test_deserialize_i32() {
    let data = make_typed_value(TYPE_ID_I32, &(-123456i32).to_le_bytes());
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<i32>().unwrap(), -123456i32);
}

#[test]
fn test_deserialize_i32_short() {
    let data = make_typed_value(TYPE_ID_I32, &[0x01, 0x02]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<i32>().unwrap(), 0i32);
}

#[test]
fn test_deserialize_i64() {
    let data = make_typed_value(TYPE_ID_I64, &(-9876543210i64).to_le_bytes());
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<i64>().unwrap(), -9876543210i64);
}

#[test]
fn test_deserialize_i64_short() {
    let data = make_typed_value(TYPE_ID_I64, &[0x01, 0x02, 0x03]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<i64>().unwrap(), 0i64);
}

#[test]
fn test_deserialize_f32() {
    let data = make_typed_value(TYPE_ID_F32, &3.14f32.to_le_bytes());
    let val = deserialize_value(&data);
    let f = *val.downcast_ref::<f32>().unwrap();
    assert!((f - 3.14f32).abs() < 0.001);
}

#[test]
fn test_deserialize_f32_short() {
    let data = make_typed_value(TYPE_ID_F32, &[0x01, 0x02]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<f32>().unwrap(), 0f32);
}

#[test]
fn test_deserialize_f64() {
    let data = make_typed_value(TYPE_ID_F64, &std::f64::consts::PI.to_le_bytes());
    let val = deserialize_value(&data);
    let f = *val.downcast_ref::<f64>().unwrap();
    assert!((f - std::f64::consts::PI).abs() < 0.001);
}

#[test]
fn test_deserialize_f64_short() {
    let data = make_typed_value(TYPE_ID_F64, &[0x01, 0x02, 0x03]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<f64>().unwrap(), 0f64);
}

#[test]
fn test_deserialize_string() {
    let data = make_typed_value(TYPE_ID_STRING, b"hello world");
    let val = deserialize_value(&data);
    assert_eq!(val.downcast_ref::<String>().unwrap(), "hello world");
}

#[test]
fn test_deserialize_string_empty() {
    let data = make_typed_value(TYPE_ID_STRING, b"");
    let val = deserialize_value(&data);
    assert_eq!(val.downcast_ref::<String>().unwrap(), "");
}

#[test]
fn test_deserialize_string_utf8_invalid() {
    // 无效 UTF-8 序列：0xFF
    let data = make_typed_value(TYPE_ID_STRING, &[0xFF, 0xFE, 0xFD]);
    let val = deserialize_value(&data);
    assert!(val.downcast_ref::<Vec<u8>>().is_some());
}

#[test]
fn test_deserialize_bool_true() {
    let data = make_typed_value(TYPE_ID_BOOL, &bool_bytes(true));
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<bool>().unwrap(), true);
}

#[test]
fn test_deserialize_bool_false() {
    let data = make_typed_value(TYPE_ID_BOOL, &bool_bytes(false));
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<bool>().unwrap(), false);
}

#[test]
fn test_deserialize_bool_empty_value() {
    let data = make_typed_value(TYPE_ID_BOOL, &[]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<bool>().unwrap(), false);
}

#[test]
fn test_deserialize_bool_nonzero() {
    // 任何非零值都应视为 true
    let data = make_typed_value(TYPE_ID_BOOL, &[42]);
    let val = deserialize_value(&data);
    assert_eq!(*val.downcast_ref::<bool>().unwrap(), true);
}

#[test]
fn test_deserialize_unknown_type() {
    // type_id=999（未知类型），回退到原始字节
    let data = make_typed_value(999, &[0xAA, 0xBB, 0xCC]);
    let val = deserialize_value(&data);
    let bytes = val.downcast_ref::<Vec<u8>>().unwrap();
    assert_eq!(bytes, &[0xAA, 0xBB, 0xCC]);
}

#[test]
fn test_deserialize_empty_data() {
    let val = deserialize_value(&[]);
    assert!(val.downcast_ref::<NullValue>().is_some());
}

#[test]
fn test_deserialize_too_short() {
    // 只有 2 字节，不足 4 字节的 type_id
    let val = deserialize_value(&[0x01, 0x02]);
    assert!(val.downcast_ref::<NullValue>().is_some());
}

// =============================== 3. SpoiTestPlayerAccessor 测试 ===============================

#[test]
fn test_player_accessor_field_count() {
    let accessor = SpoiTestPlayerAccessor;
    assert_eq!(accessor.field_count(), 4);
}

#[test]
fn test_player_accessor_get_field() {
    let accessor = SpoiTestPlayerAccessor;
    let player = SpoiTestPlayer {
        name: "Hero".to_string(),
        hp: 100,
        level: 5,
        posX: 12.5,
    };

    let name = accessor.get_field(&player, 0).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(name, "Hero");

    let hp = accessor.get_field(&player, 1).unwrap().downcast_ref::<i32>().unwrap();
    assert_eq!(*hp, 100);

    let level = accessor.get_field(&player, 2).unwrap().downcast_ref::<i32>().unwrap();
    assert_eq!(*level, 5);

    let pos = accessor.get_field(&player, 3).unwrap().downcast_ref::<f64>().unwrap();
    assert!((*pos - 12.5).abs() < 0.001);

    // 越界索引
    assert!(accessor.get_field(&player, 4).is_none());
    assert!(accessor.get_field(&player, 99).is_none());
}

#[test]
fn test_player_accessor_set_field() {
    let accessor = SpoiTestPlayerAccessor;
    let mut player = SpoiTestPlayer {
        name: String::new(),
        hp: 0,
        level: 0,
        posX: 0.0,
    };

    accessor.set_field(&mut player, 0, Box::new("Warrior".to_string()));
    accessor.set_field(&mut player, 1, Box::new(200i32));
    accessor.set_field(&mut player, 2, Box::new(10i32));
    accessor.set_field(&mut player, 3, Box::new(99.9f64));

    assert_eq!(player.name, "Warrior");
    assert_eq!(player.hp, 200);
    assert_eq!(player.level, 10);
    assert!((player.posX - 99.9).abs() < 0.001);
}

#[test]
fn test_player_accessor_set_field_wrong_type() {
    let accessor = SpoiTestPlayerAccessor;
    let mut player = SpoiTestPlayer {
        name: "Original".to_string(),
        hp: 50,
        level: 1,
        posX: 0.0,
    };

    // 尝试用错误类型设置字段（不应修改）
    accessor.set_field(&mut player, 0, Box::new(42i32));
    assert_eq!(player.name, "Original");

    accessor.set_field(&mut player, 1, Box::new("wrong".to_string()));
    assert_eq!(player.hp, 50);
}

#[test]
fn test_player_accessor_get_field_wrong_type() {
    let accessor = SpoiTestPlayerAccessor;
    let not_a_player: i32 = 42;

    // 传入非 SpoiTestPlayer 类型应返回 None
    assert!(accessor.get_field(&not_a_player, 0).is_none());
}

// =============================== 4. SpoiTestStateAccessor 测试 ===============================

#[test]
fn test_state_accessor_field_count() {
    let accessor = SpoiTestStateAccessor;
    assert_eq!(accessor.field_count(), 3);
}

#[test]
fn test_state_accessor_get_field() {
    let accessor = SpoiTestStateAccessor;
    let players: Box<dyn Any> = Box::new(vec![
        Box::new("Player1".to_string()) as Box<dyn Any>,
        Box::new("Player2".to_string()) as Box<dyn Any>,
    ]);
    let state = SpoiTestState {
        tick: 42,
        currentMap: "Dungeon".to_string(),
        players,
    };

    let tick = accessor.get_field(&state, 0).unwrap().downcast_ref::<i32>().unwrap();
    assert_eq!(*tick, 42);

    let map = accessor.get_field(&state, 1).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(map, "Dungeon");

    assert!(accessor.get_field(&state, 2).is_some());
    assert!(accessor.get_field(&state, 3).is_none());
}

#[test]
fn test_state_accessor_set_field() {
    let accessor = SpoiTestStateAccessor;
    let mut state = SpoiTestState {
        tick: 0,
        currentMap: String::new(),
        players: Box::new(NullValue),
    };

    accessor.set_field(&mut state, 0, Box::new(100i32));
    accessor.set_field(&mut state, 1, Box::new("Town".to_string()));

    assert_eq!(state.tick, 100);
    assert_eq!(state.currentMap, "Town");
}

// =============================== 5. SpoiItemAccessor 测试 ===============================

#[test]
fn test_item_accessor_field_count() {
    let accessor = SpoiItemAccessor;
    assert_eq!(accessor.field_count(), 2);
}

#[test]
fn test_item_accessor_get_field() {
    let accessor = SpoiItemAccessor;
    let item = SpoiItem {
        name: "Sword".to_string(),
        value: 500,
    };

    let name = accessor.get_field(&item, 0).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(name, "Sword");

    let value = accessor.get_field(&item, 1).unwrap().downcast_ref::<i32>().unwrap();
    assert_eq!(*value, 500);

    assert!(accessor.get_field(&item, 2).is_none());
}

#[test]
fn test_item_accessor_set_field() {
    let accessor = SpoiItemAccessor;
    let mut item = SpoiItem {
        name: String::new(),
        value: 0,
    };

    accessor.set_field(&mut item, 0, Box::new("Potion".to_string()));
    accessor.set_field(&mut item, 1, Box::new(50i32));

    assert_eq!(item.name, "Potion");
    assert_eq!(item.value, 50);
}

// =============================== 6. SpoiInventoryAccessor 测试 ===============================

#[test]
fn test_inventory_accessor_field_count() {
    let accessor = SpoiInventoryAccessor;
    assert_eq!(accessor.field_count(), 3);
}

#[test]
fn test_inventory_accessor_get_field() {
    let accessor = SpoiInventoryAccessor;
    let inv = SpoiInventory {
        items: Box::new(NullValue),
        equipped: Box::new(NullValue),
        gold: 1000,
    };

    let gold = accessor.get_field(&inv, 2).unwrap().downcast_ref::<i32>().unwrap();
    assert_eq!(*gold, 1000);

    assert!(accessor.get_field(&inv, 0).is_some());
    assert!(accessor.get_field(&inv, 1).is_some());
    assert!(accessor.get_field(&inv, 3).is_none());
}

#[test]
fn test_inventory_accessor_set_field() {
    let accessor = SpoiInventoryAccessor;
    let mut inv = SpoiInventory {
        items: Box::new(NullValue),
        equipped: Box::new(NullValue),
        gold: 0,
    };

    accessor.set_field(&mut inv, 2, Box::new(9999i32));
    assert_eq!(inv.gold, 9999);

    // 设置 Box<dyn Any> 字段：使用 NullValue 作为占位类型
    assert!(inv.items.downcast_ref::<NullValue>().is_some());
}

// =============================== 7. SpoiCharacterAccessor 测试 ===============================

#[test]
fn test_character_accessor_field_count() {
    let accessor = SpoiCharacterAccessor;
    assert_eq!(accessor.field_count(), 5);
}

#[test]
fn test_character_accessor_get_field() {
    let accessor = SpoiCharacterAccessor;
    let ch = SpoiCharacter {
        name: "Mage".to_string(),
        hp: 80,
        inventory: Box::new(NullValue),
        weapon: Box::new(NullValue),
        petLevel: 3,
    };

    let name = accessor.get_field(&ch, 0).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(name, "Mage");

    let hp = accessor.get_field(&ch, 1).unwrap().downcast_ref::<i32>().unwrap();
    assert_eq!(*hp, 80);

    let pet = accessor.get_field(&ch, 4).unwrap().downcast_ref::<i32>().unwrap();
    assert_eq!(*pet, 3);

    assert!(accessor.get_field(&ch, 5).is_none());
}

#[test]
fn test_character_accessor_set_field() {
    let accessor = SpoiCharacterAccessor;
    let mut ch = SpoiCharacter {
        name: String::new(),
        hp: 0,
        inventory: Box::new(NullValue),
        weapon: Box::new(NullValue),
        petLevel: 0,
    };

    accessor.set_field(&mut ch, 0, Box::new("Archer".to_string()));
    accessor.set_field(&mut ch, 1, Box::new(120i32));
    accessor.set_field(&mut ch, 4, Box::new(5i32));

    assert_eq!(ch.name, "Archer");
    assert_eq!(ch.hp, 120);
    assert_eq!(ch.petLevel, 5);
}

// =============================== 8. SpoiWorldAccessor 测试 ===============================

#[test]
fn test_world_accessor_field_count() {
    let accessor = SpoiWorldAccessor;
    assert_eq!(accessor.field_count(), 3);
}

#[test]
fn test_world_accessor_get_field() {
    let accessor = SpoiWorldAccessor;
    let world = SpoiWorld {
        worldName: "Azeroth".to_string(),
        tick: 1000,
        characters: Box::new(NullValue),
    };

    let name = accessor.get_field(&world, 0).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(name, "Azeroth");

    let tick = accessor.get_field(&world, 1).unwrap().downcast_ref::<i32>().unwrap();
    assert_eq!(*tick, 1000);

    assert!(accessor.get_field(&world, 2).is_some());
    assert!(accessor.get_field(&world, 3).is_none());
}

#[test]
fn test_world_accessor_set_field() {
    let accessor = SpoiWorldAccessor;
    let mut world = SpoiWorld {
        worldName: String::new(),
        tick: 0,
        characters: Box::new(NullValue),
    };

    accessor.set_field(&mut world, 0, Box::new("MiddleEarth".to_string()));
    accessor.set_field(&mut world, 1, Box::new(5000i32));

    assert_eq!(world.worldName, "MiddleEarth");
    assert_eq!(world.tick, 5000);
}

// =============================== 9. create_spoi_accessor_registry 测试 ===============================

#[test]
fn test_registry_contains_all_types() {
    let registry = create_spoi_accessor_registry();

    assert_eq!(registry.len(), 6);
    assert!(registry.contains_key("SpoiTestPlayer"));
    assert!(registry.contains_key("SpoiTestState"));
    assert!(registry.contains_key("SpoiItem"));
    assert!(registry.contains_key("SpoiInventory"));
    assert!(registry.contains_key("SpoiCharacter"));
    assert!(registry.contains_key("SpoiWorld"));
}

#[test]
fn test_registry_accessors_are_valid() {
    let registry = create_spoi_accessor_registry();

    // 验证每个注册的 accessor 的 field_count 正确
    let player_acc = registry.get("SpoiTestPlayer").unwrap();
    assert_eq!(player_acc.field_count(), 4);

    let state_acc = registry.get("SpoiTestState").unwrap();
    assert_eq!(state_acc.field_count(), 3);

    let item_acc = registry.get("SpoiItem").unwrap();
    assert_eq!(item_acc.field_count(), 2);

    let inv_acc = registry.get("SpoiInventory").unwrap();
    assert_eq!(inv_acc.field_count(), 3);

    let ch_acc = registry.get("SpoiCharacter").unwrap();
    assert_eq!(ch_acc.field_count(), 5);

    let world_acc = registry.get("SpoiWorld").unwrap();
    assert_eq!(world_acc.field_count(), 3);
}

#[test]
fn test_registry_get_field_via_registry() {
    let registry = create_spoi_accessor_registry();

    let player = SpoiTestPlayer {
        name: "TestHero".to_string(),
        hp: 75,
        level: 8,
        posX: 20.0,
    };

    let acc = registry.get("SpoiTestPlayer").unwrap();
    let name = acc.get_field(&player, 0).unwrap().downcast_ref::<String>().unwrap();
    assert_eq!(name, "TestHero");

    let hp = acc.get_field(&player, 1).unwrap().downcast_ref::<i32>().unwrap();
    assert_eq!(*hp, 75);
}

#[test]
fn test_registry_set_field_via_registry() {
    let registry = create_spoi_accessor_registry();

    let mut item = SpoiItem {
        name: String::new(),
        value: 0,
    };

    let acc = registry.get("SpoiItem").unwrap();
    acc.set_field(&mut item, 0, Box::new("Elixir".to_string()));
    acc.set_field(&mut item, 1, Box::new(300i32));

    assert_eq!(item.name, "Elixir");
    assert_eq!(item.value, 300);
}

// =============================== 10. Executor 集成测试 ===============================

#[test]
fn test_executor_with_accessor_registry() {
    let registry = create_spoi_accessor_registry();
    let executor = SpoiExecutor::new(registry);

    // 验证 accessor 注册表已正确传入
    assert!(executor.get_accessor("SpoiTestPlayer").is_some());
    assert!(executor.get_accessor("SpoiItem").is_some());
    assert!(executor.get_accessor("SpoiCharacter").is_some());
    assert!(executor.get_accessor("NonExistent").is_none());
}

#[test]
fn test_executor_basic_pipe_count() {
    let registry = create_spoi_accessor_registry();
    let mut executor = SpoiExecutor::new(registry);

    // 构造一个简单的 list 作为 root
    let list: Vec<Box<dyn Any>> = vec![
        Box::new(1u32),
        Box::new(2u32),
        Box::new(3u32),
        Box::new(4u32),
        Box::new(5u32),
    ];
    let mut root: Box<dyn Any> = Box::new(list);

    // 构造 SPOI 指令流: PIPE → COUNT
    let mut insn_bytes = Vec::new();
    write_varint(&mut insn_bytes, 2); // 2 条指令

    // 指令 1: PIPE, path=[], operand=[]
    insn_bytes.push(op::PIPE);
    write_varint(&mut insn_bytes, 0);
    write_varint(&mut insn_bytes, 0);

    // 指令 2: COUNT, path=[], operand=[]
    insn_bytes.push(op::COUNT);
    write_varint(&mut insn_bytes, 0);
    write_varint(&mut insn_bytes, 0);

    let result = executor.execute(&mut *root, &insn_bytes);
    let result_type = result
        .get("resultType")
        .and_then(|v| v.downcast_ref::<u32>())
        .copied()
        .unwrap();
    assert_eq!(result_type, result_type::SINGLE);

    let value = result.get("value").unwrap();
    assert_eq!(*value.downcast_ref::<u32>().unwrap(), 5u32);
}

#[test]
fn test_executor_pipe_count_empty() {
    let registry = create_spoi_accessor_registry();
    let mut executor = SpoiExecutor::new(registry);

    let list: Vec<Box<dyn Any>> = Vec::new();
    let mut root: Box<dyn Any> = Box::new(list);

    // PIPE → COUNT
    let mut insn_bytes = Vec::new();
    write_varint(&mut insn_bytes, 2);

    insn_bytes.push(op::PIPE);
    write_varint(&mut insn_bytes, 0);
    write_varint(&mut insn_bytes, 0);

    insn_bytes.push(op::COUNT);
    write_varint(&mut insn_bytes, 0);
    write_varint(&mut insn_bytes, 0);

    let result = executor.execute(&mut *root, &insn_bytes);
    let result_type = result
        .get("resultType")
        .and_then(|v| v.downcast_ref::<u32>())
        .copied()
        .unwrap();
    assert_eq!(result_type, result_type::SINGLE);

    let value = result.get("value").unwrap();
    assert_eq!(*value.downcast_ref::<u32>().unwrap(), 0u32);
}

#[test]
fn test_executor_take_result() {
    let registry = create_spoi_accessor_registry();
    let executor = SpoiExecutor::new(registry);

    let (result_type, pipe_data) = executor.take_result();
    assert_eq!(result_type, result_type::UNDEF);
    assert!(pipe_data.is_empty());
}