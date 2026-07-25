// =================== StreamPunk SPOI 刁钻测试 — Rust 运行时 ===================
// 编译: rustc --edition 2021 --test test_spoi_rigorous.rs
// 运行: ./test_spoi_rigorous.exe
//
// 测试覆盖 7 个类别：
//   1. 数值边界 — 各类型最大值/最小值/零值/NaN/Inf
//   2. 字符串边界 — 空串、Unicode/emoji、null字节、长串、特殊字符
//   3. 反序列化异常 — 截断数据、无效type_id、空数据
//   4. Accessor 越界 — 负索引、超大索引、不同类型对象
//   5. Executor 组合操作 — 多层FILTER、空管道、边界组合
//   6. 跨类型 Executor
//   7. Registry 边界

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
    SpoiExecutor, SpoiInstruction,
    op, result_type, write_varint,
    compare_values,
};
use std::any::Any;

// =============================== 测试宏 ===============================

macro_rules! test {
    ($name:expr, $fn:expr) => {
        match std::panic::catch_unwind(std::panic::AssertUnwindSafe($fn)) {
            Ok(_) => println!("  PASS: {}", $name),
            Err(e) => {
                let msg = if let Some(s) = e.downcast_ref::<String>() {
                    s.clone()
                } else if let Some(s) = e.downcast_ref::<&str>() {
                    s.to_string()
                } else {
                    "unknown panic".to_string()
                };
                println!("  FAIL: {} - {}", $name, msg);
            }
        }
    };
}

macro_rules! assert_or_panic {
    ($cond:expr, $msg:expr) => {
        if !$cond {
            panic!("{}", $msg);
        }
    };
}

// =============================== 辅助函数 ===============================

fn make_typed_value(type_id: u32, value_bytes: &[u8]) -> Vec<u8> {
    let mut result = Vec::with_capacity(4 + value_bytes.len());
    result.extend_from_slice(&type_id.to_le_bytes());
    result.extend_from_slice(value_bytes);
    result
}

fn make_set_operand_i32(value: i32) -> Vec<u8> {
    make_typed_value(TYPE_ID_I32, &value.to_le_bytes())
}

fn make_set_operand_string(value: &str) -> Vec<u8> {
    make_typed_value(TYPE_ID_STRING, value.as_bytes())
}

fn make_set_operand_f64(value: f64) -> Vec<u8> {
    make_typed_value(TYPE_ID_F64, &value.to_le_bytes())
}

fn make_set_operand_u32(value: u32) -> Vec<u8> {
    make_typed_value(TYPE_ID_U32, &value.to_le_bytes())
}

fn make_filter_operand(member_idx: u32, cmp_op: u8, type_id: u32, value_bytes: &[u8]) -> Vec<u8> {
    let typed_value = make_typed_value(type_id, value_bytes);
    let mut operand = Vec::with_capacity(5 + 1 + typed_value.len());
    operand.extend_from_slice(&member_idx.to_le_bytes());
    operand.push(cmp_op);
    write_varint(&mut operand, typed_value.len());
    operand.extend_from_slice(&typed_value);
    operand
}

fn build_spoi_stream(instructions: &[SpoiInstruction]) -> Vec<u8> {
    let mut buf = Vec::new();
    write_varint(&mut buf, instructions.len());
    for inst in instructions {
        buf.push(inst.op);
        write_varint(&mut buf, inst.path.len());
        for &seg in &inst.path {
            write_varint(&mut buf, seg);
        }
        write_varint(&mut buf, inst.operand.len());
        buf.extend_from_slice(&inst.operand);
    }
    buf
}

fn make_inst(op: u8, path: Vec<usize>, operand: Vec<u8>) -> SpoiInstruction {
    SpoiInstruction { op, path, operand }
}

fn pipe_inst(path: Vec<usize>) -> SpoiInstruction {
    make_inst(op::PIPE, path, vec![])
}

fn count_inst() -> SpoiInstruction {
    make_inst(op::COUNT, vec![], vec![])
}

fn take_inst(n: u32) -> SpoiInstruction {
    make_inst(op::TAKE, vec![], n.to_le_bytes().to_vec())
}

fn drop_inst(n: u32) -> SpoiInstruction {
    make_inst(op::DROP, vec![], n.to_le_bytes().to_vec())
}

fn reverse_inst() -> SpoiInstruction {
    make_inst(op::REVERSE, vec![], vec![])
}

fn distinct_inst() -> SpoiInstruction {
    make_inst(op::DISTINCT, vec![], vec![])
}

fn select_inst(path: Vec<usize>) -> SpoiInstruction {
    make_inst(op::SELECT, path, vec![])
}

fn sort_inst(path: Vec<usize>) -> SpoiInstruction {
    make_inst(op::SORT, path, vec![])
}

fn exec_inst() -> SpoiInstruction {
    make_inst(op::EXEC, vec![], vec![])
}

fn filter_inst(member_idx: u32, cmp_op: u8, type_id: u32, value_bytes: &[u8]) -> SpoiInstruction {
    make_inst(op::FILTER, vec![], make_filter_operand(member_idx, cmp_op, type_id, value_bytes))
}

fn set_inst(path: Vec<usize>, operand: Vec<u8>) -> SpoiInstruction {
    make_inst(op::SET, path, operand)
}

fn add_inst(path: Vec<usize>, operand: Vec<u8>) -> SpoiInstruction {
    make_inst(op::ADD, path, operand)
}

fn any_inst(member_idx: u32, cmp_op: u8, type_id: u32, value_bytes: &[u8]) -> SpoiInstruction {
    make_inst(op::ANY, vec![], make_filter_operand(member_idx, cmp_op, type_id, value_bytes))
}

fn all_inst(member_idx: u32, cmp_op: u8, type_id: u32, value_bytes: &[u8]) -> SpoiInstruction {
    make_inst(op::ALL, vec![], make_filter_operand(member_idx, cmp_op, type_id, value_bytes))
}

fn find_inst(member_idx: u32, cmp_op: u8, type_id: u32, value_bytes: &[u8]) -> SpoiInstruction {
    make_inst(op::FIND, vec![], make_filter_operand(member_idx, cmp_op, type_id, value_bytes))
}

fn new_accessor_executor() -> SpoiExecutor {
    SpoiExecutor::new(create_spoi_accessor_registry())
}

// =============================== 1. 数值边界测试 ===============================

#[test]
fn test_category_1_numeric_boundaries() {
    println!("=== 1. 数值边界测试 ===");

    // --- u8 ---
    test!("u8 max 255", || {
        let data = make_typed_value(TYPE_ID_U8, &[255]);
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<u8>().unwrap() == 255u8, "u8 max failed");
    });

    test!("u8 min 0", || {
        let data = make_typed_value(TYPE_ID_U8, &[0]);
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<u8>().unwrap() == 0u8, "u8 min failed");
    });

    // --- u16 ---
    test!("u16 max 65535", || {
        let data = make_typed_value(TYPE_ID_U16, &65535u16.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<u16>().unwrap() == 65535u16, "u16 max failed");
    });

    test!("u16 min 0", || {
        let data = make_typed_value(TYPE_ID_U16, &0u16.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<u16>().unwrap() == 0u16, "u16 min failed");
    });

    // --- u32 ---
    test!("u32 max 4294967295", || {
        let data = make_typed_value(TYPE_ID_U32, &u32::MAX.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<u32>().unwrap() == u32::MAX, "u32 max failed");
    });

    test!("u32 min 0", || {
        let data = make_typed_value(TYPE_ID_U32, &0u32.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<u32>().unwrap() == 0u32, "u32 min failed");
    });

    // --- u64 ---
    test!("u64 max", || {
        let data = make_typed_value(TYPE_ID_U64, &u64::MAX.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<u64>().unwrap() == u64::MAX, "u64 max failed");
    });

    test!("u64 min 0", || {
        let data = make_typed_value(TYPE_ID_U64, &0u64.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<u64>().unwrap() == 0u64, "u64 min failed");
    });

    // --- i8 ---
    test!("i8 max 127", || {
        let data = make_typed_value(TYPE_ID_I8, &[127]);
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<i8>().unwrap() == 127i8, "i8 max failed");
    });

    test!("i8 min -128", || {
        let data = make_typed_value(TYPE_ID_I8, &[0x80u8]);
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<i8>().unwrap() == -128i8, "i8 min failed");
    });

    // --- i16 ---
    test!("i16 max 32767", || {
        let data = make_typed_value(TYPE_ID_I16, &i16::MAX.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<i16>().unwrap() == i16::MAX, "i16 max failed");
    });

    test!("i16 min -32768", || {
        let data = make_typed_value(TYPE_ID_I16, &i16::MIN.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<i16>().unwrap() == i16::MIN, "i16 min failed");
    });

    // --- i32 ---
    test!("i32 max 2147483647", || {
        let data = make_typed_value(TYPE_ID_I32, &i32::MAX.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<i32>().unwrap() == i32::MAX, "i32 max failed");
    });

    test!("i32 min -2147483648", || {
        let data = make_typed_value(TYPE_ID_I32, &i32::MIN.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<i32>().unwrap() == i32::MIN, "i32 min failed");
    });

    test!("i32 zero", || {
        let data = make_typed_value(TYPE_ID_I32, &0i32.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<i32>().unwrap() == 0i32, "i32 zero failed");
    });

    // --- i64 ---
    test!("i64 max", || {
        let data = make_typed_value(TYPE_ID_I64, &i64::MAX.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<i64>().unwrap() == i64::MAX, "i64 max failed");
    });

    test!("i64 min", || {
        let data = make_typed_value(TYPE_ID_I64, &i64::MIN.to_le_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<i64>().unwrap() == i64::MIN, "i64 min failed");
    });

    // --- f32 ---
    test!("f32 NaN", || {
        let data = make_typed_value(TYPE_ID_F32, &f32::NAN.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f32>().unwrap();
        assert_or_panic!(f.is_nan(), "f32 NaN failed");
    });

    test!("f32 Inf", || {
        let data = make_typed_value(TYPE_ID_F32, &f32::INFINITY.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f32>().unwrap();
        assert_or_panic!(f.is_infinite() && f.is_sign_positive(), "f32 Inf failed");
    });

    test!("f32 NegInf", || {
        let data = make_typed_value(TYPE_ID_F32, &f32::NEG_INFINITY.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f32>().unwrap();
        assert_or_panic!(f.is_infinite() && f.is_sign_negative(), "f32 NegInf failed");
    });

    test!("f32 max", || {
        let data = make_typed_value(TYPE_ID_F32, &f32::MAX.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f32>().unwrap();
        assert_or_panic!(f == f32::MAX, "f32 max failed");
    });

    test!("f32 min_positive", || {
        let data = make_typed_value(TYPE_ID_F32, &f32::MIN_POSITIVE.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f32>().unwrap();
        assert_or_panic!(f > 0.0 && f < 1e-30, "f32 min_positive failed");
    });

    test!("f32 zero", || {
        let data = make_typed_value(TYPE_ID_F32, &0.0f32.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f32>().unwrap();
        assert_or_panic!(f == 0.0, "f32 zero failed");
    });

    test!("f32 negative zero", || {
        let data = make_typed_value(TYPE_ID_F32, &(-0.0f32).to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f32>().unwrap();
        assert_or_panic!(f.to_bits() == (-0.0f32).to_bits(), "f32 negative zero failed");
    });

    // --- f64 ---
    test!("f64 NaN", || {
        let data = make_typed_value(TYPE_ID_F64, &f64::NAN.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f64>().unwrap();
        assert_or_panic!(f.is_nan(), "f64 NaN failed");
    });

    test!("f64 Inf", || {
        let data = make_typed_value(TYPE_ID_F64, &f64::INFINITY.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f64>().unwrap();
        assert_or_panic!(f.is_infinite() && f.is_sign_positive(), "f64 Inf failed");
    });

    test!("f64 NegInf", || {
        let data = make_typed_value(TYPE_ID_F64, &f64::NEG_INFINITY.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f64>().unwrap();
        assert_or_panic!(f.is_infinite() && f.is_sign_negative(), "f64 NegInf failed");
    });

    test!("f64 max", || {
        let data = make_typed_value(TYPE_ID_F64, &f64::MAX.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f64>().unwrap();
        assert_or_panic!(f == f64::MAX, "f64 max failed");
    });

    test!("f64 min_positive", || {
        let data = make_typed_value(TYPE_ID_F64, &f64::MIN_POSITIVE.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f64>().unwrap();
        assert_or_panic!(f > 0.0 && f < 1e-300, "f64 min_positive failed");
    });

    test!("f64 zero", || {
        let data = make_typed_value(TYPE_ID_F64, &0.0f64.to_le_bytes());
        let val = deserialize_value(&data);
        let f = *val.downcast_ref::<f64>().unwrap();
        assert_or_panic!(f == 0.0, "f64 zero failed");
    });

    // --- bool ---
    test!("bool true", || {
        let data = make_typed_value(TYPE_ID_BOOL, &[1]);
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<bool>().unwrap() == true, "bool true failed");
    });

    test!("bool false", || {
        let data = make_typed_value(TYPE_ID_BOOL, &[0]);
        let val = deserialize_value(&data);
        assert_or_panic!(*val.downcast_ref::<bool>().unwrap() == false, "bool false failed");
    });

    println!();
}

// =============================== 2. 字符串边界测试 ===============================

#[test]
fn test_category_2_string_boundaries() {
    println!("=== 2. 字符串边界测试 ===");

    test!("empty string", || {
        let data = make_typed_value(TYPE_ID_STRING, b"");
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap().is_empty(), "empty string failed");
    });

    test!("single char", || {
        let data = make_typed_value(TYPE_ID_STRING, b"X");
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == "X", "single char failed");
    });

    test!("Unicode CJK", || {
        let text = "你好世界";
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "Unicode CJK failed");
    });

    test!("Unicode emoji", || {
        let text = "😀🎉🔥";
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "emoji failed");
    });

    test!("Unicode mixed", || {
        let text = "Hello世界🌍!";
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "unicode mixed failed");
    });

    test!("null byte in middle", || {
        let bytes = b"hello\x00world";
        let data = make_typed_value(TYPE_ID_STRING, bytes);
        let val = deserialize_value(&data);
        let s = val.downcast_ref::<String>().unwrap();
        assert_or_panic!(s.as_bytes().contains(&0), "null byte not preserved");
        assert_or_panic!(s.len() == 11, "null byte length wrong");
    });

    test!("null byte only", || {
        let data = make_typed_value(TYPE_ID_STRING, &[0u8]);
        let val = deserialize_value(&data);
        let s = val.downcast_ref::<String>().unwrap();
        assert_or_panic!(s.len() == 1 && s.as_bytes()[0] == 0, "null byte only failed");
    });

    test!("special chars: tabs newlines", || {
        let text = "line1\nline2\tindented\r\nwindows";
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "special chars failed");
    });

    test!("special chars: quotes backslash", || {
        let text = r#"He said "hello" with \backslash\"#;
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "quotes backslash failed");
    });

    test!("special chars: all printable ASCII", || {
        let text: String = (32u8..127u8).map(|c| c as char).collect();
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == &text, "printable ASCII failed");
    });

    test!("long string 1000 chars", || {
        let text = "A".repeat(1000);
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        let s = val.downcast_ref::<String>().unwrap();
        assert_or_panic!(s.len() == 1000, "long string length failed");
        assert_or_panic!(s.chars().all(|c| c == 'A'), "long string content failed");
    });

    test!("long string 10000 chars", || {
        let text = "B".repeat(10000);
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        let s = val.downcast_ref::<String>().unwrap();
        assert_or_panic!(s.len() == 10000, "10000 string length failed");
    });

    test!("CJK specific chars", || {
        let text = "日本語テスト";
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "Japanese failed");
    });

    test!("Korean chars", || {
        let text = "한국어테스트";
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "Korean failed");
    });

    test!("right-to-left chars", || {
        let text = "العربية";
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "Arabic failed");
    });

    test!("zero-width chars", || {
        let text = "a\u{200B}b\u{200C}c";
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "zero-width failed");
    });

    test!("string with only spaces", || {
        let text = "     ";
        let data = make_typed_value(TYPE_ID_STRING, text.as_bytes());
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<String>().unwrap() == text, "spaces only failed");
    });

    println!();
}

// =============================== 3. 反序列化异常测试 ===============================

#[test]
fn test_category_3_deserialize_anomalies() {
    println!("=== 3. 反序列化异常测试 ===");

    test!("empty data returns NullValue", || {
        let val = deserialize_value(&[]);
        assert_or_panic!(val.downcast_ref::<NullValue>().is_some(), "empty data should be NullValue");
    });

    test!("only 1 byte", || {
        let val = deserialize_value(&[0x01]);
        assert_or_panic!(val.downcast_ref::<NullValue>().is_some(), "1 byte should be NullValue");
    });

    test!("only 2 bytes", || {
        let val = deserialize_value(&[0x01, 0x02]);
        assert_or_panic!(val.downcast_ref::<NullValue>().is_some(), "2 bytes should be NullValue");
    });

    test!("only 3 bytes", || {
        let val = deserialize_value(&[0x01, 0x02, 0x03]);
        assert_or_panic!(val.downcast_ref::<NullValue>().is_some(), "3 bytes should be NullValue");
    });

    test!("exactly 4 bytes (type_id only, no value) should panic", || {
        let data = TYPE_ID_U32.to_le_bytes().to_vec();
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            deserialize_value(&data);
        }));
        assert_or_panic!(result.is_err(), "should panic on empty value bytes");
    });

    test!("type_id 0 unknown", || {
        let data = make_typed_value(0, &[0xAA, 0xBB]);
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<Vec<u8>>().is_some(), "unknown type_id should return Vec<u8>");
    });

    test!("type_id 999 unknown", || {
        let bytes = b"custom_data";
        let data = make_typed_value(999, bytes);
        let val = deserialize_value(&data);
        let v = val.downcast_ref::<Vec<u8>>().unwrap();
        assert_or_panic!(v == bytes, "unknown type_id should preserve bytes");
    });

    test!("type_id very large (u32 max)", || {
        let data = make_typed_value(u32::MAX, &[0x42]);
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<Vec<u8>>().is_some(), "u32::MAX type_id should return Vec<u8>");
    });

    test!("u32 truncated value should panic", || {
        let data = make_typed_value(TYPE_ID_U32, &[0x01, 0x02]);
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            deserialize_value(&data);
        }));
        assert_or_panic!(result.is_err(), "should panic on truncated u32 value");
    });

    test!("u64 truncated value should panic", || {
        let data = make_typed_value(TYPE_ID_U64, &[0x01, 0x02, 0x03]);
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            deserialize_value(&data);
        }));
        assert_or_panic!(result.is_err(), "should panic on truncated u64 value");
    });

    test!("i32 truncated value should panic", || {
        let data = make_typed_value(TYPE_ID_I32, &[0x01]);
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            deserialize_value(&data);
        }));
        assert_or_panic!(result.is_err(), "should panic on truncated i32 value");
    });

    test!("f32 truncated value should panic", || {
        let data = make_typed_value(TYPE_ID_F32, &[0x01, 0x02]);
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            deserialize_value(&data);
        }));
        assert_or_panic!(result.is_err(), "should panic on truncated f32 value");
    });

    test!("f64 truncated value should panic", || {
        let data = make_typed_value(TYPE_ID_F64, &[0x01, 0x02, 0x03]);
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            deserialize_value(&data);
        }));
        assert_or_panic!(result.is_err(), "should panic on truncated f64 value");
    });

    test!("u8 truncated value (empty) should panic", || {
        let data = make_typed_value(TYPE_ID_U8, &[]);
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            deserialize_value(&data);
        }));
        assert_or_panic!(result.is_err(), "should panic on empty u8 value");
    });

    test!("bool truncated value (empty) should panic", || {
        let data = make_typed_value(TYPE_ID_BOOL, &[]);
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            deserialize_value(&data);
        }));
        assert_or_panic!(result.is_err(), "should panic on empty bool value");
    });

    test!("type_id zero with empty value", || {
        let data = 0u32.to_le_bytes().to_vec();
        let val = deserialize_value(&data);
        let v = val.downcast_ref::<Vec<u8>>().unwrap();
        assert_or_panic!(v.is_empty(), "empty value for unknown type");
    });

    // 无效 UTF-8
    test!("invalid UTF-8 sequence", || {
        let data = make_typed_value(TYPE_ID_STRING, &[0xFF, 0xFE, 0xFD]);
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<Vec<u8>>().is_some(), "invalid UTF-8 should return Vec<u8>");
    });

    test!("invalid UTF-8 lone surrogate", || {
        let data = make_typed_value(TYPE_ID_STRING, &[0xED, 0xA0, 0x80]);
        let val = deserialize_value(&data);
        assert_or_panic!(val.downcast_ref::<Vec<u8>>().is_some(), "lone surrogate should return Vec<u8>");
    });

    println!();
}

// =============================== 4. Accessor 越界测试 ===============================

#[test]
fn test_category_4_accessor_bounds() {
    println!("=== 4. Accessor 越界测试 ===");

    let player_accessor = SpoiTestPlayerAccessor;
    let item_accessor = SpoiItemAccessor;
    let state_accessor = SpoiTestStateAccessor;
    let inv_accessor = SpoiInventoryAccessor;
    let ch_accessor = SpoiCharacterAccessor;
    let world_accessor = SpoiWorldAccessor;

    // --- 负索引（通过超大 usize 模拟） ---
    test!("player get_field index=usize::MAX", || {
        let player = SpoiTestPlayer { name: "T".to_string(), hp: 1, level: 1, posX: 0.0 };
        assert_or_panic!(player_accessor.get_field(&player, usize::MAX).is_none(), "usize::MAX should be None");
    });

    test!("player get_field index=field_count", || {
        let player = SpoiTestPlayer { name: "T".to_string(), hp: 1, level: 1, posX: 0.0 };
        assert_or_panic!(player_accessor.get_field(&player, 4).is_none(), "index=4 should be None for 4-field player");
    });

    test!("player get_field index=100", || {
        let player = SpoiTestPlayer { name: "T".to_string(), hp: 1, level: 1, posX: 0.0 };
        assert_or_panic!(player_accessor.get_field(&player, 100).is_none(), "index=100 should be None");
    });

    test!("player get_field index=1000", || {
        let player = SpoiTestPlayer { name: "T".to_string(), hp: 1, level: 1, posX: 0.0 };
        assert_or_panic!(player_accessor.get_field(&player, 1000).is_none(), "index=1000 should be None");
    });

    // --- 不同类型对象 ---
    test!("item_accessor on player object", || {
        let player = SpoiTestPlayer { name: "T".to_string(), hp: 1, level: 1, posX: 0.0 };
        assert_or_panic!(item_accessor.get_field(&player, 0).is_none(), "item accessor on player should be None");
    });

    test!("player_accessor on item object", || {
        let item = SpoiItem { name: "Sword".to_string(), value: 100 };
        assert_or_panic!(player_accessor.get_field(&item, 0).is_none(), "player accessor on item should be None");
    });

    test!("state_accessor on world object", || {
        let world = SpoiWorld { worldName: "X".to_string(), tick: 0, characters: Box::new(NullValue) };
        assert_or_panic!(state_accessor.get_field(&world, 0).is_none(), "state accessor on world should be None");
    });

    test!("character_accessor on i32", || {
        let num: i32 = 42;
        assert_or_panic!(ch_accessor.get_field(&num, 0).is_none(), "character accessor on i32 should be None");
    });

    test!("world_accessor on string", || {
        let s = "hello".to_string();
        assert_or_panic!(world_accessor.get_field(&s, 0).is_none(), "world accessor on string should be None");
    });

    // --- 所有 accessor 越界 ---
    test!("item accessor index=2 out of bounds", || {
        let item = SpoiItem { name: "S".to_string(), value: 0 };
        assert_or_panic!(item_accessor.get_field(&item, 2).is_none(), "item index=2 should be None");
    });

    test!("state accessor index=3 out of bounds", || {
        let state = SpoiTestState { tick: 0, currentMap: "M".to_string(), players: Box::new(NullValue) };
        assert_or_panic!(state_accessor.get_field(&state, 3).is_none(), "state index=3 should be None");
    });

    test!("inventory accessor index=3 out of bounds", || {
        let inv = SpoiInventory { items: Box::new(NullValue), equipped: Box::new(NullValue), gold: 0 };
        assert_or_panic!(inv_accessor.get_field(&inv, 3).is_none(), "inventory index=3 should be None");
    });

    test!("character accessor index=5 out of bounds", || {
        let ch = SpoiCharacter { name: "C".to_string(), hp: 0, inventory: Box::new(NullValue), weapon: Box::new(NullValue), petLevel: 0 };
        assert_or_panic!(ch_accessor.get_field(&ch, 5).is_none(), "character index=5 should be None");
    });

    test!("world accessor index=3 out of bounds", || {
        let world = SpoiWorld { worldName: "W".to_string(), tick: 0, characters: Box::new(NullValue) };
        assert_or_panic!(world_accessor.get_field(&world, 3).is_none(), "world index=3 should be None");
    });

    // --- set_field 越界不 panic ---
    test!("player set_field outside bounds no panic", || {
        let mut player = SpoiTestPlayer { name: "T".to_string(), hp: 1, level: 1, posX: 0.0 };
        player_accessor.set_field(&mut player, 99, Box::new(42i32));
        // 不应 panic，字段值不应改变
        assert_or_panic!(player.hp == 1, "hp should be unchanged");
    });

    test!("item set_field outside bounds no panic", || {
        let mut item = SpoiItem { name: "S".to_string(), value: 100 };
        item_accessor.set_field(&mut item, 99, Box::new("wrong".to_string()));
        assert_or_panic!(item.value == 100, "value should be unchanged");
    });

    // --- set_field 错误类型 ---
    test!("player set_field wrong type", || {
        let mut player = SpoiTestPlayer { name: "Original".to_string(), hp: 50, level: 1, posX: 0.0 };
        player_accessor.set_field(&mut player, 0, Box::new(42i32)); // name 期望 String
        assert_or_panic!(player.name == "Original", "name should be unchanged");
    });

    println!();
}

// =============================== 5. Executor 组合操作测试 ===============================

#[test]
fn test_category_5_executor_combinations() {
    println!("=== 5. Executor 组合操作测试 ===");

    // --- 5.1 SET + PIPE + EXEC ---
    test!("SET + PIPE + EXEC on simple list", || {
        let list: Vec<Box<dyn Any>> = vec![Box::new(10u32), Box::new(20u32), Box::new(30u32)];
        let mut root: Box<dyn Any> = Box::new(list);
        let mut executor = new_accessor_executor();

        let insts = vec![
            pipe_inst(vec![]),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        let result = executor.execute(&mut *root, &stream);
        let result_type = result.get("resultType").and_then(|v| v.downcast_ref::<u32>()).copied().unwrap();
        assert_or_panic!(result_type == result_type::VECTOR, "should be VECTOR");
        let values = result.get("value").and_then(|v| v.downcast_ref::<Vec<Box<dyn Any>>>()).unwrap();
        assert_or_panic!(values.len() == 3, "should have 3 elements");
    });

    // --- 5.2 多层 FILTER ---
    test!("two FILTER in sequence", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = vec![
            Box::new(1u32), Box::new(2u32), Box::new(3u32),
            Box::new(4u32), Box::new(5u32), Box::new(6u32),
        ];
        // FILTER: 自身 > 2 (memberIdx=0 对基本类型就是自身)
        let operand1 = make_filter_operand(0, 3, TYPE_ID_U32, &2u32.to_le_bytes());
        executor.op_filter(&[], &operand1);
        assert_or_panic!(executor.pipe_data.len() == 4, "after first filter should have 4");

        // FILTER: 自身 > 4
        let operand2 = make_filter_operand(0, 3, TYPE_ID_U32, &4u32.to_le_bytes());
        executor.op_filter(&[], &operand2);
        assert_or_panic!(executor.pipe_data.len() == 2, "after second filter should have 2");
        assert_or_panic!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap() == 5u32, "first should be 5");
        assert_or_panic!(*executor.pipe_data[1].downcast_ref::<u32>().unwrap() == 6u32, "second should be 6");
    });

    test!("three FILTER in sequence", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = (1u32..=10u32).map(|v| Box::new(v) as Box<dyn Any>).collect();
        // > 3
        let f1 = make_filter_operand(0, 3, TYPE_ID_U32, &3u32.to_le_bytes());
        executor.op_filter(&[], &f1);
        // < 8
        let f2 = make_filter_operand(0, 2, TYPE_ID_U32, &8u32.to_le_bytes());
        executor.op_filter(&[], &f2);
        // != 6
        let f3 = make_filter_operand(0, 1, TYPE_ID_U32, &6u32.to_le_bytes());
        executor.op_filter(&[], &f3);
        assert_or_panic!(executor.pipe_data.len() == 3, "should have 3 elements: 4,5,7");
        let vals: Vec<u32> = executor.pipe_data.iter().map(|b| *b.downcast_ref::<u32>().unwrap()).collect();
        assert_or_panic!(vals == vec![4, 5, 7], "should be [4,5,7]");
    });

    // --- 5.3 空管道操作 ---
    test!("empty pipe_data COUNT", || {
        let mut executor = new_accessor_executor();
        executor.op_count();
        assert_or_panic!(executor.pipe_data.len() == 1, "COUNT should produce 1 result");
        assert_or_panic!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap() == 0u32, "empty COUNT should be 0");
    });

    test!("empty pipe_data FILTER", || {
        let mut executor = new_accessor_executor();
        let operand = make_filter_operand(0, 0, TYPE_ID_U32, &1u32.to_le_bytes());
        executor.op_filter(&[], &operand);
        assert_or_panic!(executor.pipe_data.is_empty(), "empty filter should stay empty");
    });

    test!("empty pipe_data REVERSE", || {
        let mut executor = new_accessor_executor();
        executor.op_reverse();
        assert_or_panic!(executor.pipe_data.is_empty(), "empty reverse should stay empty");
    });

    test!("empty pipe_data DISTINCT", || {
        let mut executor = new_accessor_executor();
        executor.op_distinct();
        assert_or_panic!(executor.pipe_data.is_empty(), "empty distinct should stay empty");
    });

    test!("empty pipe_data SORT", || {
        let mut executor = new_accessor_executor();
        executor.op_sort(&[]);
        assert_or_panic!(executor.pipe_data.is_empty(), "empty sort should stay empty");
    });

    test!("empty pipe_data TAKE(0)", || {
        let mut executor = new_accessor_executor();
        executor.op_take(&0u32.to_le_bytes());
        assert_or_panic!(executor.pipe_data.is_empty(), "empty take should stay empty");
    });

    test!("empty pipe_data TAKE(5)", || {
        let mut executor = new_accessor_executor();
        executor.op_take(&5u32.to_le_bytes());
        assert_or_panic!(executor.pipe_data.is_empty(), "empty take should stay empty");
    });

    test!("empty pipe_data DROP(0)", || {
        let mut executor = new_accessor_executor();
        executor.op_drop(&0u32.to_le_bytes());
        assert_or_panic!(executor.pipe_data.is_empty(), "empty drop should stay empty");
    });

    test!("empty pipe_data DROP(5)", || {
        let mut executor = new_accessor_executor();
        executor.op_drop(&5u32.to_le_bytes());
        assert_or_panic!(executor.pipe_data.is_empty(), "empty drop should stay empty");
    });

    test!("empty pipe_data ANY", || {
        let mut executor = new_accessor_executor();
        let operand = make_filter_operand(0, 3, TYPE_ID_U32, &5u32.to_le_bytes());
        executor.op_any(&[], &operand);
        assert_or_panic!(executor.pipe_data.len() == 1, "ANY should produce 1 result");
        assert_or_panic!(*executor.pipe_data[0].downcast_ref::<bool>().unwrap() == false, "empty ANY should be false");
    });

    test!("empty pipe_data ALL", || {
        let mut executor = new_accessor_executor();
        let operand = make_filter_operand(0, 3, TYPE_ID_U32, &5u32.to_le_bytes());
        executor.op_all(&[], &operand);
        assert_or_panic!(executor.pipe_data.len() == 1, "ALL should produce 1 result");
        assert_or_panic!(*executor.pipe_data[0].downcast_ref::<bool>().unwrap() == true, "empty ALL should be true");
    });

    test!("empty pipe_data FIND", || {
        let mut executor = new_accessor_executor();
        let operand = make_filter_operand(0, 0, TYPE_ID_U32, &1u32.to_le_bytes());
        executor.op_find(&[], &operand);
        assert_or_panic!(executor.pipe_data.is_empty(), "empty FIND should stay empty");
    });

    // --- 5.4 COUNT + TAKE + DROP 边界组合 ---
    test!("COUNT then SORT (single element)", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32), Box::new(3u32)];
        executor.op_count();
        executor.op_sort(&[]);
        assert_or_panic!(executor.pipe_data.len() == 1, "COUNT then SORT should have 1");
        assert_or_panic!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap() == 3u32, "should be 3");
    });

    test!("TAKE more than available", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32)];
        executor.op_take(&10u32.to_le_bytes());
        assert_or_panic!(executor.pipe_data.len() == 2, "TAKE(10) on 2 elements should keep 2");
    });

    test!("DROP more than available", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32)];
        executor.op_drop(&10u32.to_le_bytes());
        assert_or_panic!(executor.pipe_data.is_empty(), "DROP(10) on 2 elements should clear");
    });

    test!("TAKE(0) on non-empty", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32)];
        executor.op_take(&0u32.to_le_bytes());
        assert_or_panic!(executor.pipe_data.is_empty(), "TAKE(0) should clear");
    });

    test!("DROP(0) on non-empty", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32)];
        executor.op_drop(&0u32.to_le_bytes());
        assert_or_panic!(executor.pipe_data.len() == 2, "DROP(0) should keep all");
    });

    // --- 5.5 SELECT + SORT + REVERSE + DISTINCT 组合 ---
    test!("SELECT + SORT + REVERSE + DISTINCT", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = vec![
            Box::new(3u32), Box::new(1u32), Box::new(2u32),
            Box::new(3u32), Box::new(1u32), Box::new(4u32),
        ];
        executor.op_sort(&[]);
        executor.op_reverse();
        executor.op_distinct();
        assert_or_panic!(executor.pipe_data.len() == 4, "distinct should have 4");
        let vals: Vec<u32> = executor.pipe_data.iter().map(|b| *b.downcast_ref::<u32>().unwrap()).collect();
        assert_or_panic!(vals == vec![4, 3, 2, 1], "should be descending [4,3,2,1]");
    });

    // --- 5.6 REVERSE 两次恢复原序 ---
    test!("double REVERSE restores order", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = vec![Box::new(1u32), Box::new(2u32), Box::new(3u32)];
        executor.op_reverse();
        executor.op_reverse();
        let vals: Vec<u32> = executor.pipe_data.iter().map(|b| *b.downcast_ref::<u32>().unwrap()).collect();
        assert_or_panic!(vals == vec![1, 2, 3], "double reverse should restore");
    });

    // --- 5.7 单元素 REVERSE ---
    test!("single element REVERSE", || {
        let mut executor = new_accessor_executor();
        executor.pipe_data = vec![Box::new(42u32)];
        executor.op_reverse();
        assert_or_panic!(executor.pipe_data.len() == 1, "single element reverse should stay 1");
        assert_or_panic!(*executor.pipe_data[0].downcast_ref::<u32>().unwrap() == 42u32, "value should be 42");
    });

    // --- 5.8 REPLACE / REMOVE / INSERT 操作 ---
    test!("REMOVE valid index", || {
        let mut executor = new_accessor_executor();
        let list: Vec<Box<dyn Any>> = vec![Box::new(10u32), Box::new(20u32), Box::new(30u32)];
        let mut root: Box<dyn Any> = Box::new(list);
        let operand = 1u32.to_le_bytes().to_vec();
        executor.op_remove(&mut *root, &[], &operand);
        let remaining = root.downcast_ref::<Vec<Box<dyn Any>>>().unwrap();
        assert_or_panic!(remaining.len() == 2, "should have 2 after remove");
        assert_or_panic!(*remaining[0].downcast_ref::<u32>().unwrap() == 10u32, "first should be 10");
        assert_or_panic!(*remaining[1].downcast_ref::<u32>().unwrap() == 30u32, "second should be 30");
    });

    test!("INSERT at beginning", || {
        let mut executor = new_accessor_executor();
        let list: Vec<Box<dyn Any>> = vec![Box::new(20u32), Box::new(30u32)];
        let mut root: Box<dyn Any> = Box::new(list);
        let mut operand = 0u32.to_le_bytes().to_vec();
        operand.extend_from_slice(&10u32.to_le_bytes());
        executor.op_insert(&mut *root, &[], &operand);
        let remaining = root.downcast_ref::<Vec<Box<dyn Any>>>().unwrap();
        assert_or_panic!(remaining.len() == 3, "should have 3 after insert");
        assert_or_panic!(*remaining[0].downcast_ref::<u32>().unwrap() == 10u32, "first should be 10");
    });

    test!("INSERT at end", || {
        let mut executor = new_accessor_executor();
        let list: Vec<Box<dyn Any>> = vec![Box::new(10u32), Box::new(20u32)];
        let mut root: Box<dyn Any> = Box::new(list);
        let mut operand = 2u32.to_le_bytes().to_vec();
        operand.extend_from_slice(&30u32.to_le_bytes());
        executor.op_insert(&mut *root, &[], &operand);
        let remaining = root.downcast_ref::<Vec<Box<dyn Any>>>().unwrap();
        assert_or_panic!(remaining.len() == 3, "should have 3 after insert");
        assert_or_panic!(*remaining[2].downcast_ref::<u32>().unwrap() == 30u32, "last should be 30");
    });

    test!("REPLACE valid index", || {
        let mut executor = new_accessor_executor();
        let list: Vec<Box<dyn Any>> = vec![Box::new(10u32), Box::new(20u32), Box::new(30u32)];
        let mut root: Box<dyn Any> = Box::new(list);
        let mut operand = 1u32.to_le_bytes().to_vec();
        operand.extend_from_slice(&99u32.to_le_bytes());
        executor.op_replace(&mut *root, &[], &operand);
        let remaining = root.downcast_ref::<Vec<Box<dyn Any>>>().unwrap();
        assert_or_panic!(*remaining[1].downcast_ref::<u32>().unwrap() == 99u32, "second should be 99");
    });

    // --- 5.9 APPEND ---
    test!("APPEND to list", || {
        let mut executor = new_accessor_executor();
        let list: Vec<Box<dyn Any>> = vec![Box::new(10u32), Box::new(20u32)];
        let mut root: Box<dyn Any> = Box::new(list);
        let operand = make_typed_value(TYPE_ID_U32, &30u32.to_le_bytes());
        executor.op_append(&mut *root, &[], &operand);
        let remaining = root.downcast_ref::<Vec<Box<dyn Any>>>().unwrap();
        assert_or_panic!(remaining.len() == 3, "should have 3 after append");
        assert_or_panic!(*remaining[2].downcast_ref::<u32>().unwrap() == 30u32, "last should be 30");
    });

    // --- 5.10 RESET / SETNULL ---
    test!("RESET sets NullValue", || {
        let mut executor = new_accessor_executor();
        let list: Vec<Box<dyn Any>> = vec![Box::new(10u32), Box::new(20u32)];
        let mut root: Box<dyn Any> = Box::new(list);
        executor.op_reset(&mut *root, &[0]);
        let remaining = root.downcast_ref::<Vec<Box<dyn Any>>>().unwrap();
        assert_or_panic!(remaining[0].downcast_ref::<NullValue>().is_some(), "first should be NullValue after reset");
    });

    test!("SETNULL sets NullValue", || {
        let mut executor = new_accessor_executor();
        let list: Vec<Box<dyn Any>> = vec![Box::new(10u32), Box::new(20u32)];
        let mut root: Box<dyn Any> = Box::new(list);
        executor.op_setnull(&mut *root, &[0]);
        let remaining = root.downcast_ref::<Vec<Box<dyn Any>>>().unwrap();
        assert_or_panic!(remaining[0].downcast_ref::<NullValue>().is_some(), "first should be NullValue after setnull");
    });

    // --- 5.11 完整管道: PIPE→FILTER→SORT→REVERSE→TAKE→DROP→DISTINCT→COUNT ---
    test!("full complex pipeline", || {
        let mut executor = new_accessor_executor();
        let list: Vec<Box<dyn Any>> = vec![
            Box::new(5u32), Box::new(1u32), Box::new(3u32),
            Box::new(2u32), Box::new(5u32), Box::new(4u32),
            Box::new(1u32), Box::new(3u32),
        ];
        let mut root: Box<dyn Any> = Box::new(list);

        let insts = vec![
            pipe_inst(vec![]),
            filter_inst(0, 3, TYPE_ID_U32, &2u32.to_le_bytes()), // > 2
            sort_inst(vec![]),
            reverse_inst(),
            take_inst(3),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        let result = executor.execute(&mut *root, &stream);
        let values = result.get("value").and_then(|v| v.downcast_ref::<Vec<Box<dyn Any>>>()).unwrap();
        // >2: 5,3,5,4,3 → sorted: 3,3,4,5,5 → reversed: 5,5,4,3,3 → take(3): 5,5,4
        assert_or_panic!(values.len() == 3, "should have 3 elements");
        let vals: Vec<u32> = values.iter().map(|b| *b.downcast_ref::<u32>().unwrap()).collect();
        assert_or_panic!(vals == vec![5, 5, 4], "should be [5,5,4]");
    });

    println!();
}

// =============================== 6. 跨类型 Executor 测试 ===============================

#[test]
fn test_category_6_cross_type_executor() {
    println!("=== 6. 跨类型 Executor 测试 ===");

    // --- 6.1 SpoiItem ---
    test!("SpoiItem SET name/value", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let item = SpoiItem { name: "Sword".to_string(), value: 100 };
        let mut root: Box<dyn Any> = Box::new(item);

        let insts = vec![
            set_inst(vec![0], make_set_operand_string("Excalibur")),
            set_inst(vec![1], make_set_operand_i32(999)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);

        let item_ref = root.downcast_ref::<SpoiItem>().unwrap();
        assert_or_panic!(item_ref.name == "Excalibur", "name should be Excalibur");
        assert_or_panic!(item_ref.value == 999, "value should be 999");
    });

    test!("SpoiItem ADD value", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let item = SpoiItem { name: "Potion".to_string(), value: 50 };
        let mut root: Box<dyn Any> = Box::new(item);

        let insts = vec![
            add_inst(vec![1], make_set_operand_i32(25)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        let item_ref = root.downcast_ref::<SpoiItem>().unwrap();
        assert_or_panic!(item_ref.value == 75, "50 + 25 should be 75");
    });

    // --- 6.2 SpoiInventory ---
    test!("SpoiInventory SET gold", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let inv = SpoiInventory { items: Box::new(NullValue), equipped: Box::new(NullValue), gold: 100 };
        let mut root: Box<dyn Any> = Box::new(inv);

        let insts = vec![
            set_inst(vec![2], make_set_operand_i32(5000)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        let inv_ref = root.downcast_ref::<SpoiInventory>().unwrap();
        assert_or_panic!(inv_ref.gold == 5000, "gold should be 5000");
    });

    test!("SpoiInventory ADD gold", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let inv = SpoiInventory { items: Box::new(NullValue), equipped: Box::new(NullValue), gold: 200 };
        let mut root: Box<dyn Any> = Box::new(inv);

        let insts = vec![
            add_inst(vec![2], make_set_operand_i32(300)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        let inv_ref = root.downcast_ref::<SpoiInventory>().unwrap();
        assert_or_panic!(inv_ref.gold == 500, "200 + 300 should be 500");
    });

    // --- 6.3 SpoiCharacter ---
    test!("SpoiCharacter SET name/hp/petLevel", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let ch = SpoiCharacter {
            name: "Mage".to_string(), hp: 50,
            inventory: Box::new(NullValue), weapon: Box::new(NullValue), petLevel: 1,
        };
        let mut root: Box<dyn Any> = Box::new(ch);

        let insts = vec![
            set_inst(vec![0], make_set_operand_string("Wizard")),
            set_inst(vec![1], make_set_operand_i32(100)),
            set_inst(vec![4], make_set_operand_i32(10)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        let ch_ref = root.downcast_ref::<SpoiCharacter>().unwrap();
        assert_or_panic!(ch_ref.name == "Wizard", "name should be Wizard");
        assert_or_panic!(ch_ref.hp == 100, "hp should be 100");
        assert_or_panic!(ch_ref.petLevel == 10, "petLevel should be 10");
    });

    test!("SpoiCharacter ADD hp", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let ch = SpoiCharacter {
            name: "Warrior".to_string(), hp: 80,
            inventory: Box::new(NullValue), weapon: Box::new(NullValue), petLevel: 1,
        };
        let mut root: Box<dyn Any> = Box::new(ch);

        let insts = vec![
            add_inst(vec![1], make_set_operand_i32(-30)),
            add_inst(vec![4], make_set_operand_i32(2)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        let ch_ref = root.downcast_ref::<SpoiCharacter>().unwrap();
        assert_or_panic!(ch_ref.hp == 50, "80 + (-30) should be 50");
        assert_or_panic!(ch_ref.petLevel == 3, "1 + 2 should be 3");
    });

    // --- 6.4 SpoiWorld ---
    test!("SpoiWorld SET worldName/tick", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let world = SpoiWorld { worldName: "Old".to_string(), tick: 0, characters: Box::new(NullValue) };
        let mut root: Box<dyn Any> = Box::new(world);

        let insts = vec![
            set_inst(vec![0], make_set_operand_string("NewWorld")),
            set_inst(vec![1], make_set_operand_i32(9999)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        let world_ref = root.downcast_ref::<SpoiWorld>().unwrap();
        assert_or_panic!(world_ref.worldName == "NewWorld", "worldName should be NewWorld");
        assert_or_panic!(world_ref.tick == 9999, "tick should be 9999");
    });

    test!("SpoiWorld ADD tick", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let world = SpoiWorld { worldName: "W".to_string(), tick: 100, characters: Box::new(NullValue) };
        let mut root: Box<dyn Any> = Box::new(world);

        let insts = vec![
            add_inst(vec![1], make_set_operand_i32(1)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        let world_ref = root.downcast_ref::<SpoiWorld>().unwrap();
        assert_or_panic!(world_ref.tick == 101, "100 + 1 should be 101");
    });

    // --- 6.5 SpoiTestPlayer ---
    test!("SpoiTestPlayer SET all fields", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let player = SpoiTestPlayer { name: "Hero".to_string(), hp: 100, level: 1, posX: 0.0 };
        let mut root: Box<dyn Any> = Box::new(player);

        let insts = vec![
            set_inst(vec![0], make_set_operand_string("SuperHero")),
            set_inst(vec![1], make_set_operand_i32(200)),
            set_inst(vec![2], make_set_operand_i32(50)),
            set_inst(vec![3], make_set_operand_f64(123.456)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        let p = root.downcast_ref::<SpoiTestPlayer>().unwrap();
        assert_or_panic!(p.name == "SuperHero", "name mismatch");
        assert_or_panic!(p.hp == 200, "hp mismatch");
        assert_or_panic!(p.level == 50, "level mismatch");
        assert_or_panic!((p.posX - 123.456).abs() < 0.001, "posX mismatch");
    });

    // --- 6.6 SpoiTestState ---
    test!("SpoiTestState SET tick/map", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());
        let state = SpoiTestState { tick: 0, currentMap: "Start".to_string(), players: Box::new(NullValue) };
        let mut root: Box<dyn Any> = Box::new(state);

        let insts = vec![
            set_inst(vec![0], make_set_operand_i32(100)),
            set_inst(vec![1], make_set_operand_string("Dungeon")),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        let s = root.downcast_ref::<SpoiTestState>().unwrap();
        assert_or_panic!(s.tick == 100, "tick mismatch");
        assert_or_panic!(s.currentMap == "Dungeon", "map mismatch");
    });

    // --- 6.7 跨类型：用不同 executor 操作不同对象 ---
    test!("cross-type: player then item then world", || {
        let mut executor = SpoiExecutor::new(create_spoi_accessor_registry());

        // 操作 player
        let player = SpoiTestPlayer { name: "P1".to_string(), hp: 50, level: 1, posX: 0.0 };
        let mut root: Box<dyn Any> = Box::new(player);
        let insts = vec![
            set_inst(vec![1], make_set_operand_i32(999)),
            exec_inst(),
        ];
        let stream = build_spoi_stream(&insts);
        executor.execute(&mut *root, &stream);
        assert_or_panic!(root.downcast_ref::<SpoiTestPlayer>().unwrap().hp == 999, "player hp mismatch");

        // 操作 item
        let mut executor2 = SpoiExecutor::new(create_spoi_accessor_registry());
        let item = SpoiItem { name: "I1".to_string(), value: 10 };
        let mut root2: Box<dyn Any> = Box::new(item);
        let insts2 = vec![
            set_inst(vec![1], make_set_operand_i32(888)),
            exec_inst(),
        ];
        let stream2 = build_spoi_stream(&insts2);
        executor2.execute(&mut *root2, &stream2);
        assert_or_panic!(root2.downcast_ref::<SpoiItem>().unwrap().value == 888, "item value mismatch");

        // 操作 world
        let mut executor3 = SpoiExecutor::new(create_spoi_accessor_registry());
        let world = SpoiWorld { worldName: "W1".to_string(), tick: 0, characters: Box::new(NullValue) };
        let mut root3: Box<dyn Any> = Box::new(world);
        let insts3 = vec![
            set_inst(vec![1], make_set_operand_i32(777)),
            exec_inst(),
        ];
        let stream3 = build_spoi_stream(&insts3);
        executor3.execute(&mut *root3, &stream3);
        assert_or_panic!(root3.downcast_ref::<SpoiWorld>().unwrap().tick == 777, "world tick mismatch");
    });

    println!();
}

// =============================== 7. Registry 边界测试 ===============================

#[test]
fn test_category_7_registry_boundaries() {
    println!("=== 7. Registry 边界测试 ===");

    // --- 7.1 size=6 ---
    test!("registry size is 6", || {
        let registry = create_spoi_accessor_registry();
        assert_or_panic!(registry.len() == 6, "registry should have 6 entries");
    });

    test!("registry contains all expected keys", || {
        let registry = create_spoi_accessor_registry();
        assert_or_panic!(registry.contains_key("SpoiTestPlayer"), "missing SpoiTestPlayer");
        assert_or_panic!(registry.contains_key("SpoiTestState"), "missing SpoiTestState");
        assert_or_panic!(registry.contains_key("SpoiItem"), "missing SpoiItem");
        assert_or_panic!(registry.contains_key("SpoiInventory"), "missing SpoiInventory");
        assert_or_panic!(registry.contains_key("SpoiCharacter"), "missing SpoiCharacter");
        assert_or_panic!(registry.contains_key("SpoiWorld"), "missing SpoiWorld");
    });

    // --- 7.2 缺失 key ---
    test!("registry missing key returns None", || {
        let registry = create_spoi_accessor_registry();
        assert_or_panic!(registry.get("NonExistent").is_none(), "NonExistent should be None");
        assert_or_panic!(registry.get("").is_none(), "empty string should be None");
        assert_or_panic!(registry.get("spoiTestPlayer").is_none(), "wrong case should be None");
        assert_or_panic!(registry.get("SPOITESTPLAYER").is_none(), "uppercase should be None");
    });

    test!("executor get_accessor missing key", || {
        let executor = new_accessor_executor();
        assert_or_panic!(executor.get_accessor("NonExistent").is_none(), "executor should return None for missing key");
        assert_or_panic!(executor.get_accessor("").is_none(), "executor should return None for empty");
    });

    // --- 7.3 all fieldCount > 0 ---
    test!("all accessors have field_count > 0", || {
        let registry = create_spoi_accessor_registry();
        for (name, acc) in registry.iter() {
            assert_or_panic!(acc.field_count() > 0, &format!("{} has field_count == 0", name));
        }
    });

    test!("specific field counts", || {
        let registry = create_spoi_accessor_registry();
        assert_or_panic!(registry.get("SpoiTestPlayer").unwrap().field_count() == 4, "SpoiTestPlayer should have 4 fields");
        assert_or_panic!(registry.get("SpoiTestState").unwrap().field_count() == 3, "SpoiTestState should have 3 fields");
        assert_or_panic!(registry.get("SpoiItem").unwrap().field_count() == 2, "SpoiItem should have 2 fields");
        assert_or_panic!(registry.get("SpoiInventory").unwrap().field_count() == 3, "SpoiInventory should have 3 fields");
        assert_or_panic!(registry.get("SpoiCharacter").unwrap().field_count() == 5, "SpoiCharacter should have 5 fields");
        assert_or_panic!(registry.get("SpoiWorld").unwrap().field_count() == 3, "SpoiWorld should have 3 fields");
    });

    // --- 7.4 注册表访问器功能验证 ---
    test!("registry accessor get_field works", || {
        let registry = create_spoi_accessor_registry();
        let player = SpoiTestPlayer { name: "Check".to_string(), hp: 42, level: 7, posX: 1.5 };
        let acc = registry.get("SpoiTestPlayer").unwrap();
        let name = acc.get_field(&player, 0).unwrap().downcast_ref::<String>().unwrap();
        assert_or_panic!(name == "Check", "name mismatch via registry");
        let hp = acc.get_field(&player, 1).unwrap().downcast_ref::<i32>().unwrap();
        assert_or_panic!(*hp == 42, "hp mismatch via registry");
    });

    test!("registry accessor set_field works", || {
        let registry = create_spoi_accessor_registry();
        let mut item = SpoiItem { name: "Old".to_string(), value: 0 };
        let acc = registry.get("SpoiItem").unwrap();
        acc.set_field(&mut item, 0, Box::new("New".to_string()));
        acc.set_field(&mut item, 1, Box::new(100i32));
        assert_or_panic!(item.name == "New", "name not set via registry");
        assert_or_panic!(item.value == 100, "value not set via registry");
    });

    // --- 7.5 注册表不可变性（多次调用返回相同结果） ---
    test!("registry multiple calls consistent", || {
        let r1 = create_spoi_accessor_registry();
        let r2 = create_spoi_accessor_registry();
        assert_or_panic!(r1.len() == r2.len(), "two registries should have same size");
        for key in r1.keys() {
            assert_or_panic!(r2.contains_key(key), "r2 should contain key from r1");
        }
    });

    // --- 7.6 executor 使用 registry 的集成功能 ---
    test!("executor with registry accessor lookup", || {
        let executor = new_accessor_executor();
        assert_or_panic!(executor.get_accessor("SpoiTestPlayer").is_some(), "should find SpoiTestPlayer");
        assert_or_panic!(executor.get_accessor("SpoiItem").is_some(), "should find SpoiItem");
        assert_or_panic!(executor.get_accessor("SpoiInventory").is_some(), "should find SpoiInventory");
        assert_or_panic!(executor.get_accessor("SpoiCharacter").is_some(), "should find SpoiCharacter");
        assert_or_panic!(executor.get_accessor("SpoiWorld").is_some(), "should find SpoiWorld");
        assert_or_panic!(executor.get_accessor("SpoiTestState").is_some(), "should find SpoiTestState");
    });

    println!();
}

// =============================== 附加：对比值函数测试 ===============================

#[test]
fn test_compare_values_edge_cases() {
    println!("=== 附加: compare_values 边界测试 ===");

    test!("compare i32 eq", || {
        let a = Box::new(5i32);
        let b = Box::new(5i32);
        assert_or_panic!(compare_values(&*a, &*b, 0) == true, "5 == 5");
        assert_or_panic!(compare_values(&*a, &*b, 1) == false, "5 != 5");
    });

    test!("compare i32 lt", || {
        let a = Box::new(3i32);
        let b = Box::new(5i32);
        assert_or_panic!(compare_values(&*a, &*b, 2) == true, "3 < 5");
        assert_or_panic!(compare_values(&*a, &*b, 3) == false, "3 > 5");
    });

    test!("compare i32 le", || {
        let a = Box::new(5i32);
        let b = Box::new(5i32);
        assert_or_panic!(compare_values(&*a, &*b, 4) == true, "5 <= 5");
        assert_or_panic!(compare_values(&*a, &*b, 5) == true, "5 >= 5");
    });

    test!("compare f64 eq", || {
        let a = Box::new(3.14f64);
        let b = Box::new(3.14f64);
        assert_or_panic!(compare_values(&*a, &*b, 0) == true, "3.14 == 3.14");
    });

    test!("compare f64 lt", || {
        let a = Box::new(1.0f64);
        let b = Box::new(2.0f64);
        assert_or_panic!(compare_values(&*a, &*b, 2) == true, "1.0 < 2.0");
    });

    test!("compare string eq", || {
        let a = Box::new("hello".to_string());
        let b = Box::new("hello".to_string());
        assert_or_panic!(compare_values(&*a, &*b, 0) == true, "\"hello\" == \"hello\"");
    });

    test!("compare string lt", || {
        let a = Box::new("abc".to_string());
        let b = Box::new("xyz".to_string());
        assert_or_panic!(compare_values(&*a, &*b, 2) == true, "\"abc\" < \"xyz\"");
    });

    test!("compare bool eq", || {
        let a = Box::new(true);
        let b = Box::new(true);
        assert_or_panic!(compare_values(&*a, &*b, 0) == true, "true == true");
        assert_or_panic!(compare_values(&*a, &*b, 1) == false, "true != true");
    });

    test!("compare bool ne", || {
        let a = Box::new(true);
        let b = Box::new(false);
        assert_or_panic!(compare_values(&*a, &*b, 1) == true, "true != false");
    });

    test!("compare unknown cmp_op returns true", || {
        let a = Box::new(1i32);
        let b = Box::new(2i32);
        assert_or_panic!(compare_values(&*a, &*b, 99) == true, "unknown cmp_op should return true");
    });

    println!();
}