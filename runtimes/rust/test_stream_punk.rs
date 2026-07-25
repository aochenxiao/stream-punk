// Stream-Punk Rust 序列化/反序列化测试
// 运行: rustc --test test_stream_punk.rs -o test_stream_punk.exe && ./test_stream_punk.exe

#[path = "stream-punk.rs"]
mod stream_punk;

use stream_punk::{I, O, SpRef, SpArray, SpVariant};

// ======================== 基本类型测试 ========================

#[test]
fn test_u8_roundtrip() {
    for v in [0u8, 1, 127, 128, 255] {
        let mut o = O::new();
        o.write_u8(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_u8(), v, "u8: {}", v);
    }
}

#[test]
fn test_u16_roundtrip() {
    for v in [0u16, 1, 256, 1000, 0xFFFF] {
        let mut o = O::new();
        o.write_u16(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_u16(), v, "u16: {}", v);
    }
}

#[test]
fn test_u32_roundtrip() {
    for v in [0u32, 1, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF] {
        let mut o = O::new();
        o.write_u32(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_u32(), v, "u32: {}", v);
    }
}

#[test]
fn test_u64_roundtrip() {
    for v in [0u64, 1, 0x100000000, 0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF] {
        let mut o = O::new();
        o.write_u64(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_u64(), v, "u64: {}", v);
    }
}

#[test]
fn test_i8_roundtrip() {
    for v in [-128i8, -1, 0, 1, 127] {
        let mut o = O::new();
        o.write_i8(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_i8(), v, "i8: {}", v);
    }
}

#[test]
fn test_i16_roundtrip() {
    for v in [-32768i16, -1, 0, 1, 32767] {
        let mut o = O::new();
        o.write_i16(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_i16(), v, "i16: {}", v);
    }
}

#[test]
fn test_i32_roundtrip() {
    for v in [-2147483648i32, -1, 0, 1, 2147483647] {
        let mut o = O::new();
        o.write_i32(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_i32(), v, "i32: {}", v);
    }
}

#[test]
fn test_i64_roundtrip() {
    for v in [-9223372036854775808i64, -1, 0, 1, 9223372036854775807] {
        let mut o = O::new();
        o.write_i64(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_i64(), v, "i64: {}", v);
    }
}

#[test]
fn test_f32_roundtrip() {
    for v in [0.0f32, -1.0, 3.14] {
        let mut o = O::new();
        o.write_f32(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_f32(), v, "f32: {}", v);
    }
}

#[test]
fn test_f64_roundtrip() {
    for v in [0.0f64, -1.0, 3.141592653589793] {
        let mut o = O::new();
        o.write_f64(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_f64(), v, "f64: {}", v);
    }
}

#[test]
fn test_bl_roundtrip() {
    let mut o = O::new();
    o.write_bl(true);
    o.write_bl(false);
    let mut i = I::new(o.to_bytes());
    assert!(i.read_bl(), "bl: expected true");
    assert!(!i.read_bl(), "bl: expected false");
}

#[test]
fn test_ch_roundtrip() {
    for v in ['A', 'z', '0', '\n'] {
        let mut o = O::new();
        o.write_ch8(v);
        let mut i = I::new(o.to_bytes());
        assert_eq!(i.read_ch8(), v, "ch8: {}", v);
    }
}

// ======================== 字符串测试 ========================

#[test]
fn test_string_empty() {
    let mut o = O::new();
    o.write_string("");
    let mut i = I::new(o.to_bytes());
    assert_eq!(i.read_string(), "", "empty string");
}

#[test]
fn test_string_ascii() {
    let s = "Hello World";
    let mut o = O::new();
    o.write_string(s);
    let mut i = I::new(o.to_bytes());
    assert_eq!(i.read_string(), s, "ascii string");
}

#[test]
fn test_string_unicode() {
    let s = "你好世界 — 测试";
    let mut o = O::new();
    o.write_string(s);
    let mut i = I::new(o.to_bytes());
    assert_eq!(i.read_string(), s, "unicode string");
}

#[test]
fn test_string_long() {
    let s = "A".repeat(10000);
    let mut o = O::new();
    o.write_string(&s);
    let mut i = I::new(o.to_bytes());
    assert_eq!(i.read_string(), s, "long string");
}

#[test]
fn test_u16string() {
    let s = "你好世界";
    let mut o = O::new();
    o.write_u16string(s);
    let mut i = I::new(o.to_bytes());
    assert_eq!(i.read_u16string(), s, "u16string");
}

// ======================== 容器测试 ========================

#[test]
fn test_array_empty() {
    let mut o = O::new();
    let data: Vec<u32> = vec![];
    o.write_Array(&data, |o, v| o.write_u32(*v));
    let mut i = I::new(o.to_bytes());
    let result: Vec<u32> = i.read_Array(|i| i.read_u32());
    assert_eq!(result.len(), 0, "empty array");
}

#[test]
fn test_array_u32() {
    let mut o = O::new();
    let data = vec![1u32, 2, 3, 4, 5];
    o.write_Array(&data, |o, v| o.write_u32(*v));
    let mut i = I::new(o.to_bytes());
    let result: Vec<u32> = i.read_Array(|i| i.read_u32());
    assert_eq!(result, data, "array u32");
}

#[test]
fn test_array_strings() {
    let mut o = O::new();
    let data = vec!["Alice", "Bob", "Carol"];
    o.write_Array(&data, |o, v| o.write_string(v));
    let mut i = I::new(o.to_bytes());
    let result: Vec<String> = i.read_Array(|i| i.read_string());
    assert_eq!(result, vec!["Alice", "Bob", "Carol"], "array strings");
}

#[test]
fn test_set() {
    use std::collections::HashSet;
    let mut o = O::new();
    let mut data = HashSet::new();
    data.insert(1u32);
    data.insert(2u32);
    data.insert(3u32);
    o.write_set(&data, |o, v| o.write_u32(*v));
    let mut i = I::new(o.to_bytes());
    let result: HashSet<u32> = i.read_set(|i| i.read_u32());
    assert_eq!(result.len(), 3, "set size");
    assert!(result.contains(&1) && result.contains(&2) && result.contains(&3));
}

#[test]
fn test_map() {
    use std::collections::HashMap;
    let mut o = O::new();
    let mut data = HashMap::new();
    data.insert("key1".to_string(), 100u32);
    data.insert("key2".to_string(), 200u32);
    o.write_map(&data, |o, k| o.write_string(k), |o, v| o.write_u32(*v));
    let mut i = I::new(o.to_bytes());
    let result: HashMap<String, u32> = i.read_map(|i| i.read_string(), |i| i.read_u32());
    assert_eq!(result.len(), 2, "map size");
    assert_eq!(result.get("key1"), Some(&100));
    assert_eq!(result.get("key2"), Some(&200));
}

// ======================== 多字段往返测试 ========================

#[test]
fn test_mixed_types() {
    let mut o = O::new();
    o.write_u8(42);
    o.write_i32(-100);
    o.write_f64(3.14);
    o.write_bl(true);
    o.write_string("hello");

    let mut i = I::new(o.to_bytes());
    assert_eq!(i.read_u8(), 42);
    assert_eq!(i.read_i32(), -100);
    assert_eq!(i.read_f64(), 3.14);
    assert!(i.read_bl());
    assert_eq!(i.read_string(), "hello");
}

#[test]
fn test_offset_reader() {
    let mut o = O::new();
    o.write_u32(0); // padding
    o.write_u32(42);
    o.write_u32(0); // padding

    let data = o.to_bytes();
    let mut i = I::new(data);
    i.off = 4;
    assert_eq!(i.read_u32(), 42);
    assert_eq!(i.off, 8);
}

#[test]
fn test_has_more_data() {
    let mut o = O::new();
    o.write_u32(42);
    let mut i = I::new(o.to_bytes());
    assert!(i.has_more_data());
    i.read_u32();
    assert!(!i.has_more_data());
}

// ======================== 辅助类型测试 ========================

#[test]
fn test_sp_array() {
    let mut arr = SpArray::new(3, "init");
    assert_eq!(arr.size(), 3);
    assert_eq!(*arr.at(0), "init");
    arr.set(1, "hello");
    assert_eq!(*arr.at(1), "hello");
}

#[test]
fn test_sp_ref() {
    let ref_val = SpRef::new("hello", 0x1000);
    assert_eq!(ref_val.value, Some("hello"));
    assert_eq!(ref_val.address, 0x1000);

    let null_ref: SpRef<&str> = SpRef::none();
    assert!(null_ref.value.is_none());
    assert_eq!(null_ref.address, 0);
}

// ======================== 边界条件测试 ========================

#[test]
fn test_zero_length_array() {
    let mut o = O::new();
    let data: Vec<u8> = vec![];
    o.write_Array(&data, |o, v| o.write_u8(*v));
    let mut i = I::new(o.to_bytes());
    let result: Vec<u8> = i.read_Array(|i| i.read_u8());
    assert_eq!(result.len(), 0);
}

#[test]
fn test_negative_in_array() {
    let mut o = O::new();
    let data = vec![-1i32, 0, 1, -100, 100];
    o.write_Array(&data, |o, v| o.write_i32(*v));
    let mut i = I::new(o.to_bytes());
    let result: Vec<i32> = i.read_Array(|i| i.read_i32());
    assert_eq!(result, data);
}