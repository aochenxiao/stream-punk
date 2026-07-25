use std::collections::{HashMap, HashSet};
use std::fmt::Debug;
use std::sync::Arc;

/* =================== 辅助类型 =================== */

#[derive(Debug, Clone)]
pub struct SpRef<T: Debug + Clone> {
    pub value: Option<T>,
    pub address: u64,
}

impl<T: Debug + Clone> SpRef<T> {
    pub fn none() -> Self {
        SpRef { value: None, address: 0 }
    }
    pub fn new(value: T, address: u64) -> Self {
        SpRef { value: Some(value), address }
    }
}

impl<T: Debug + Clone> PartialEq for SpRef<T> {
    fn eq(&self, other: &Self) -> bool {
        self.address == other.address
    }
}

impl<T: Debug + Clone> Eq for SpRef<T> {}

impl<T: Debug + Clone> std::hash::Hash for SpRef<T> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.address.hash(state);
    }
}

/* =================== 序列化写入器 =================== */

#[derive(Debug, Clone)]
pub struct O {
    pub buf: Vec<u8>,
}

impl O {
    pub fn new() -> Self {
        O { buf: Vec::new() }
    }

    pub fn to_bytes(&self) -> Vec<u8> {
        self.buf.clone()
    }

    pub fn write_u8(&mut self, v: u8) {
        self.buf.push(v);
    }

    pub fn write_u16(&mut self, v: u16) {
        self.buf.extend_from_slice(&v.to_le_bytes());
    }

    pub fn write_u32(&mut self, v: u32) {
        self.buf.extend_from_slice(&v.to_le_bytes());
    }

    pub fn write_u64(&mut self, v: u64) {
        self.buf.extend_from_slice(&v.to_le_bytes());
    }

    pub fn write_i8(&mut self, v: i8) {
        self.buf.push(v as u8);
    }

    pub fn write_i16(&mut self, v: i16) {
        self.buf.extend_from_slice(&v.to_le_bytes());
    }

    pub fn write_i32(&mut self, v: i32) {
        self.buf.extend_from_slice(&v.to_le_bytes());
    }

    pub fn write_i64(&mut self, v: i64) {
        self.buf.extend_from_slice(&v.to_le_bytes());
    }

    pub fn write_f32(&mut self, v: f32) {
        self.buf.extend_from_slice(&v.to_le_bytes());
    }

    pub fn write_f64(&mut self, v: f64) {
        self.buf.extend_from_slice(&v.to_le_bytes());
    }

    pub fn write_bl(&mut self, v: bool) {
        self.buf.push(if v { 1 } else { 0 });
    }

    pub fn write_ch(&mut self, v: char) {
        self.buf.push(v as u8);
    }

    pub fn write_ch8(&mut self, v: char) {
        self.buf.push(v as u8);
    }

    pub fn write_ch16(&mut self, v: char) {
        let code = v as u16;
        self.buf.extend_from_slice(&code.to_le_bytes());
    }

    pub fn write_ch32(&mut self, v: u32) {
        self.buf.extend_from_slice(&v.to_le_bytes());
    }

    pub fn write_sz(&mut self, v: u32) {
        self.write_u32(v);
    }

    pub fn write_string(&mut self, s: &str) {
        let bytes = s.as_bytes();
        self.write_sz(bytes.len() as u32);
        self.buf.extend_from_slice(bytes);
    }

    pub fn write_u8string(&mut self, s: &str) {
        self.write_string(s);
    }

    pub fn write_u16string(&mut self, s: &str) {
        let utf16: Vec<u16> = s.encode_utf16().collect();
        self.write_sz(utf16.len() as u32);
        for c in utf16 {
            self.buf.extend_from_slice(&c.to_le_bytes());
        }
    }

    pub fn write_u32string(&mut self, s: &[u32]) {
        self.write_sz(s.len() as u32);
        for cp in s {
            self.buf.extend_from_slice(&cp.to_le_bytes());
        }
    }

    pub fn write_ptr_with_typeID(&mut self, value: Option<&dyn SpBase>) {
        match value {
            Some(v) => {
                let addr = v as *const dyn SpBase as *const () as usize as u64;
                self.write_u64(addr);
            }
            None => {
                self.write_u64(0);
            }
        }
    }

    pub fn write_ptr<T: Debug + Clone, F: Fn(&mut O, &T)>(&mut self, value: Option<&T>, address: u64, writer: F) {
        match value {
            Some(v) => {
                self.write_u64(address);
                writer(self, v);
            }
            None => {
                self.write_u64(0);
            }
        }
    }

    pub fn write_Array<T: Debug + Clone, F: Fn(&mut O, &T)>(&mut self, arr: &Vec<T>, writer: F) {
        self.write_sz(arr.len() as u32);
        for v in arr {
            writer(self, v);
        }
    }

    pub fn write_set<T: Debug + Clone, F: Fn(&mut O, &T)>(&mut self, set: &HashSet<T>, writer: F) where T: Eq + std::hash::Hash {
        self.write_sz(set.len() as u32);
        for v in set {
            writer(self, v);
        }
    }

    pub fn write_map<K: Debug + Clone, V: Debug + Clone, KF: Fn(&mut O, &K), VF: Fn(&mut O, &V)>(&mut self, map: &HashMap<K, V>, key_writer: KF, value_writer: VF) where K: Eq + std::hash::Hash {
        self.write_sz(map.len() as u32);
        for (k, v) in map {
            key_writer(self, k);
            value_writer(self, v);
        }
    }
}

/* =================== 反序列化读取器 =================== */

#[derive(Debug, Clone)]
pub struct I {
    buf: Vec<u8>,
    pub off: usize,
    obj_map: HashMap<u64, u64>,
}

impl I {
    pub fn new(data: Vec<u8>) -> Self {
        I { buf: data, off: 0, obj_map: HashMap::new() }
    }

    pub fn has_more_data(&self) -> bool {
        self.off < self.buf.len()
    }

    fn read_bytes<const N: usize>(&mut self) -> [u8; N] {
        let mut arr = [0u8; N];
        arr.copy_from_slice(&self.buf[self.off..self.off + N]);
        self.off += N;
        arr
    }

    pub fn read_u8(&mut self) -> u8 {
        let v = self.buf[self.off];
        self.off += 1;
        v
    }

    pub fn read_u16(&mut self) -> u16 {
        u16::from_le_bytes(self.read_bytes::<2>())
    }

    pub fn read_u32(&mut self) -> u32 {
        u32::from_le_bytes(self.read_bytes::<4>())
    }

    pub fn read_u64(&mut self) -> u64 {
        u64::from_le_bytes(self.read_bytes::<8>())
    }

    pub fn read_i8(&mut self) -> i8 {
        self.read_u8() as i8
    }

    pub fn read_i16(&mut self) -> i16 {
        i16::from_le_bytes(self.read_bytes::<2>())
    }

    pub fn read_i32(&mut self) -> i32 {
        i32::from_le_bytes(self.read_bytes::<4>())
    }

    pub fn read_i64(&mut self) -> i64 {
        i64::from_le_bytes(self.read_bytes::<8>())
    }

    pub fn read_f32(&mut self) -> f32 {
        f32::from_le_bytes(self.read_bytes::<4>())
    }

    pub fn read_f64(&mut self) -> f64 {
        f64::from_le_bytes(self.read_bytes::<8>())
    }

    pub fn read_bl(&mut self) -> bool {
        self.read_u8() != 0
    }

    pub fn read_ch(&mut self) -> char {
        self.read_u8() as char
    }

    pub fn read_ch8(&mut self) -> char {
        self.read_u8() as char
    }

    pub fn read_ch16(&mut self) -> char {
        let v = self.read_u16();
        char::from_u32(v as u32).unwrap_or('\0')
    }

    pub fn read_ch32(&mut self) -> u32 {
        self.read_u32()
    }

    pub fn read_sz(&mut self) -> usize {
        self.read_u32() as usize
    }

    pub fn skip_bytes(&mut self, length: usize) {
        self.off += length;
    }

    pub fn read_string(&mut self) -> String {
        let length = self.read_sz();
        if length == 0 {
            return String::new();
        }
        let v = String::from_utf8(self.buf[self.off..self.off + length].to_vec())
            .unwrap_or_default();
        self.off += length;
        v
    }

    pub fn read_u8string(&mut self) -> String {
        self.read_string()
    }

    pub fn read_u16string(&mut self) -> String {
        let length = self.read_sz();
        if length == 0 {
            return String::new();
        }
        let mut chars: Vec<u16> = Vec::with_capacity(length);
        for _ in 0..length {
            chars.push(self.read_u16());
        }
        String::from_utf16(&chars).unwrap_or_default()
    }

    pub fn read_u32string(&mut self) -> Vec<u32> {
        let length = self.read_sz();
        let mut chars: Vec<u32> = Vec::with_capacity(length);
        for _ in 0..length {
            chars.push(self.read_u32());
        }
        chars
    }

    pub fn read_ptr_with_typeID(&mut self) -> SpRef<Arc<dyn SpBase>> {
        let addr = self.read_u64();
        if addr == 0 {
            return SpRef::none();
        }
        let value = match read_obj(self) {
            Some(b) => Arc::from(b),
            None => return SpRef::none(),
        };
        SpRef::new(value, addr)
    }

    pub fn read_ptr<T: Debug + Clone, F: Fn(&mut I) -> T>(&mut self, reader: F) -> SpRef<T> {
        let addr = self.read_u64();
        if addr == 0 {
            return SpRef { value: None, address: 0 };
        }
        let value = reader(self);
        SpRef { value: Some(value), address: addr }
    }

    pub fn read_Array<T: Debug + Clone, F: Fn(&mut I) -> T>(&mut self, reader: F) -> Vec<T> {
        let size = self.read_sz();
        let mut result = Vec::with_capacity(size);
        for _ in 0..size {
            result.push(reader(self));
        }
        result
    }

    pub fn read_set<T: Debug + Clone + Eq + std::hash::Hash, F: Fn(&mut I) -> T>(&mut self, reader: F) -> HashSet<T> {
        let size = self.read_sz();
        let mut result = HashSet::with_capacity(size);
        for _ in 0..size {
            result.insert(reader(self));
        }
        result
    }

    pub fn read_map<K: Debug + Clone + Eq + std::hash::Hash, V: Debug + Clone, KF: Fn(&mut I) -> K, VF: Fn(&mut I) -> V>(
        &mut self,
        key_reader: KF,
        value_reader: VF,
    ) -> HashMap<K, V> {
        let size = self.read_sz();
        let mut result = HashMap::with_capacity(size);
        for _ in 0..size {
            let k = key_reader(self);
            let v = value_reader(self);
            result.insert(k, v);
        }
        result
    }

    pub fn read_vector<T: Debug + Clone, F: Fn(&mut I) -> T>(&mut self, reader: F) -> Vec<T> {
        self.read_Array(reader)
    }

    pub fn read_deque<T: Debug + Clone, F: Fn(&mut I) -> T>(&mut self, reader: F) -> Vec<T> {
        self.read_Array(reader)
    }

    pub fn read_list<T: Debug + Clone, F: Fn(&mut I) -> T>(&mut self, reader: F) -> Vec<T> {
        self.read_Array(reader)
    }

    pub fn read_forward_list<T: Debug + Clone, F: Fn(&mut I) -> T>(&mut self, reader: F) -> Vec<T> {
        self.read_Array(reader)
    }

    pub fn read_std_string(&mut self) -> String {
        self.read_string()
    }
}

/* =================== 不定类型容器 =================== */

#[derive(Debug, Clone)]
pub struct SpVariant {
    pub type_index: u32,
    pub value: Option<Arc<dyn SpBase>>,
}

impl SpVariant {
    pub fn new() -> Self {
        SpVariant { type_index: 0, value: None }
    }

    pub fn write_variant(&self, o: &mut O) {
        o.write_u32(self.type_index);
        if let Some(ref v) = self.value {
            write_obj(o, v.as_ref());
        }
    }

    pub fn read_variant(i: &mut I) -> Self {
        let type_index = i.read_u32();
        let value = read_obj(i).map(|b| Arc::from(b));
        SpVariant { type_index, value }
    }
}

#[derive(Debug, Clone)]
pub struct SpArray<T: Debug + Clone> {
    data: Vec<T>,
}

impl<T: Debug + Clone> SpArray<T> {
    pub fn new(size: usize, initializer: T) -> Self {
        SpArray { data: vec![initializer; size] }
    }

    pub fn size(&self) -> usize {
        self.data.len()
    }

    pub fn at(&self, index: usize) -> &T {
        &self.data[index]
    }

    pub fn set(&mut self, index: usize, value: T) {
        if index < self.data.len() {
            self.data[index] = value;
        }
    }

    pub fn write_SpArray<F: Fn(&mut O, &T)>(&self, o: &mut O, writer: F) {
        for v in &self.data {
            writer(o, v);
        }
    }

    pub fn read_SpArray<F: Fn(&mut I) -> T>(i: &mut I, size: usize, reader: F) -> Self {
        let mut data = Vec::with_capacity(size);
        for _ in 0..size {
            data.push(reader(i));
        }
        SpArray { data }
    }
}

/* =================== 特性 =================== */

pub trait SpBase: Debug {
    fn type_id(&self) -> u32;
    fn from_(&mut self, i: &mut I);
    fn to(&self, o: &mut O);
}

/* =================== 类型分发（由代码生成器输出具体实现） =================== */

/// 读取对象（桩实现，由 sp-gen 生成具体分发逻辑）
pub fn read_obj(i: &mut I) -> Option<Box<dyn SpBase>> {
    let _ = i;
    None
}

/// 写入对象（桩实现，由 sp-gen 生成具体分发逻辑）
pub fn write_obj(o: &mut O, obj: &dyn SpBase) {
    let _ = o;
    let _ = obj;
}