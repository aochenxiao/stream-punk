"""
Stream-Punk Python 序列化/反序列化测试套件

测试覆盖：
- 基本类型读写（u8/u16/u32/u64/i8/i16/i32/i64/f32/f64/bl/ch）
- 字符串（u8string/u16string/u32string）
- 容器（Array/Vector/Set/Map）
- Optional
- Bitset
- 往返测试（write → read）
- 边界条件（零值、最大值、负值、空容器、空字符串）
"""
import sys
import os
import struct
import importlib
import importlib.util
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
spec = importlib.util.spec_from_file_location("stream_punk", os.path.join(os.path.dirname(os.path.abspath(__file__)), "stream-punk.py"))
stream_punk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stream_punk)
I = stream_punk.I
O = stream_punk.O
SpRef = stream_punk.SpRef
SpArray = stream_punk.SpArray
SpVariant = stream_punk.SpVariant


class TestBasicTypes(unittest.TestCase):
    """基本类型读写测试"""

    # === 无符号整数 ===
    def test_u8_roundtrip(self):
        for v in [0, 1, 127, 128, 255]:
            o = O(); o.write_u8(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_u8(), v)

    def test_u16_roundtrip(self):
        for v in [0, 1, 256, 1000, 0xFFFF]:
            o = O(); o.write_u16(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_u16(), v)

    def test_u32_roundtrip(self):
        for v in [0, 1, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF]:
            o = O(); o.write_u32(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_u32(), v)

    def test_u64_roundtrip(self):
        for v in [0, 1, 0x100000000, 0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF]:
            o = O(); o.write_u64(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_u64(), v)

    # === 有符号整数 ===
    def test_i8_roundtrip(self):
        for v in [-128, -1, 0, 1, 127]:
            o = O(); o.write_i8(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_i8(), v)

    def test_i16_roundtrip(self):
        for v in [-32768, -1, 0, 1, 32767]:
            o = O(); o.write_i16(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_i16(), v)

    def test_i32_roundtrip(self):
        for v in [-2147483648, -1, 0, 1, 2147483647]:
            o = O(); o.write_i32(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_i32(), v)

    def test_i64_roundtrip(self):
        for v in [-9223372036854775808, -1, 0, 1, 9223372036854775807]:
            o = O(); o.write_i64(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_i64(), v)

    # === 浮点数 ===
    def test_f32_roundtrip(self):
        for v in [0.0, -1.0, 3.14, float('inf'), float('-inf')]:
            o = O(); o.write_f32(v)
            i = I(o.to_bytes())
            result = i.read_f32()
            if v != v:  # NaN
                self.assertTrue(result != result)
            else:
                self.assertAlmostEqual(result, v, places=5)

    def test_f32_nan(self):
        o = O(); o.write_f32(float('nan'))
        i = I(o.to_bytes())
        result = i.read_f32()
        self.assertTrue(result != result)

    def test_f64_roundtrip(self):
        for v in [0.0, -1.0, 3.141592653589793, float('inf'), float('-inf')]:
            o = O(); o.write_f64(v)
            i = I(o.to_bytes())
            result = i.read_f64()
            if v != v:
                self.assertTrue(result != result)
            else:
                self.assertAlmostEqual(result, v, places=10)

    def test_f64_nan(self):
        o = O(); o.write_f64(float('nan'))
        i = I(o.to_bytes())
        result = i.read_f64()
        self.assertTrue(result != result)

    # === 布尔值 ===
    def test_bl_true(self):
        o = O(); o.write_bl(True)
        i = I(o.to_bytes())
        self.assertTrue(i.read_bl())

    def test_bl_false(self):
        o = O(); o.write_bl(False)
        i = I(o.to_bytes())
        self.assertFalse(i.read_bl())

    # === 字符 ===
    def test_ch_roundtrip(self):
        for v in ['A', 'z', '0', '\n']:
            o = O(); o.write_ch8(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_ch8(), v)

    def test_ch16_roundtrip(self):
        for v in ['A', '中']:
            o = O(); o.write_ch16(v)
            i = I(o.to_bytes())
            self.assertEqual(i.read_ch16(), v)


class TestStringTypes(unittest.TestCase):
    """字符串类型读写测试"""

    def test_write_read_empty(self):
        o = O(); o.write_string("")
        i = I(o.to_bytes())
        self.assertEqual(i.read_string(), "")

    def test_write_read_ascii(self):
        o = O(); o.write_string("Hello World")
        i = I(o.to_bytes())
        self.assertEqual(i.read_string(), "Hello World")

    def test_write_read_unicode(self):
        s = "你好世界 🌍 — 测试"
        o = O(); o.write_string(s)
        i = I(o.to_bytes())
        self.assertEqual(i.read_string(), s)

    def test_write_read_long(self):
        s = "A" * 10000
        o = O(); o.write_string(s)
        i = I(o.to_bytes())
        self.assertEqual(i.read_string(), s)

    def test_u8string(self):
        o = O(); o.write_u8string("Hello UTF-8")
        i = I(o.to_bytes())
        self.assertEqual(i.read_u8string(), "Hello UTF-8")

    def test_u16string(self):
        s = "你好世界"
        o = O(); o.write_u16string(s)
        i = I(o.to_bytes())
        self.assertEqual(i.read_u16string(), s)

    def test_u16string_empty(self):
        o = O(); o.write_u16string("")
        i = I(o.to_bytes())
        self.assertEqual(i.read_u16string(), "")

    def test_u32string_roundtrip(self):
        data = b'\x01\x00\x00\x00\x02\x00\x00\x00'
        o = O(); o.write_u32string(data)
        i = I(o.to_bytes())
        result = i.read_u32string()
        self.assertEqual(result, data)

    def test_std_string(self):
        o = O(); o.write_string("std::string")
        i = I(o.to_bytes())
        self.assertEqual(i.read_std_string(), "std::string")


class TestContainers(unittest.TestCase):
    """容器类型读写测试"""

    def test_array_empty(self):
        o = O(); o.write_Array([], lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_Array(lambda: i.read_u32())
        self.assertEqual(result, [])

    def test_array_u32(self):
        data = [1, 2, 3, 4, 5]
        o = O(); o.write_Array(data, lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_Array(lambda: i.read_u32())
        self.assertEqual(result, data)

    def test_vector(self):
        data = [10, 20, 30]
        o = O(); o.write_vector(data, lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_vector(lambda: i.read_u32())
        self.assertEqual(result, data)

    def test_array_strings(self):
        data = ["Alice", "Bob", "Carol"]
        o = O(); o.write_Array(data, lambda v: o.write_string(v))
        i = I(o.to_bytes())
        result = i.read_Array(lambda: i.read_string())
        self.assertEqual(result, data)

    def test_set(self):
        data = {1, 2, 3}
        o = O(); o.write_set(data, lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_set(lambda: i.read_u32())
        self.assertEqual(result, data)

    def test_set_empty(self):
        o = O(); o.write_set(set(), lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_set(lambda: i.read_u32())
        self.assertEqual(result, set())

    def test_map(self):
        data = {"key1": 100, "key2": 200}
        o = O(); o.write_map(data,
            lambda k: o.write_string(k),
            lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_map(lambda: i.read_string(), lambda: i.read_u32())
        self.assertEqual(result, data)

    def test_map_empty(self):
        o = O(); o.write_map({},
            lambda k: o.write_string(k),
            lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_map(lambda: i.read_string(), lambda: i.read_u32())
        self.assertEqual(result, {})

    def test_nested_array(self):
        """嵌套数组"""
        data = [[1, 2], [3, 4, 5], []]
        o = O()
        o.write_Array(data, lambda inner: o.write_Array(inner, lambda v: o.write_u32(v)))
        i = I(o.to_bytes())
        result = i.read_Array(lambda: i.read_Array(lambda: i.read_u32()))
        self.assertEqual(result, data)


class TestOptional(unittest.TestCase):
    """Optional 读写测试"""

    def test_optional_present(self):
        o = O(); o.write_optional(42, lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_optional(lambda: i.read_u32())
        self.assertEqual(result, 42)

    def test_optional_none(self):
        o = O(); o.write_optional(None, lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_optional(lambda: i.read_u32())
        self.assertIsNone(result)

    def test_optional_zero(self):
        """0 是有效值，不是 None"""
        o = O(); o.write_optional(0, lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_optional(lambda: i.read_u32())
        self.assertEqual(result, 0)


class TestBitset(unittest.TestCase):
    """Bitset 读写测试"""

    def test_bitset_empty(self):
        o = O(); o.write_bitset([])
        i = I(o.to_bytes())
        result = i.read_bitset()
        self.assertEqual(result, [])

    def test_bitset_all_false(self):
        bits = [False, False, False]
        o = O(); o.write_bitset(bits)
        i = I(o.to_bytes())
        result = i.read_bitset()
        self.assertEqual(result, bits)

    def test_bitset_all_true(self):
        bits = [True, True, True]
        o = O(); o.write_bitset(bits)
        i = I(o.to_bytes())
        result = i.read_bitset()
        self.assertEqual(result, bits)

    def test_bitset_mixed(self):
        bits = [True, False, True, False, False, True, False, True, True]
        o = O(); o.write_bitset(bits)
        i = I(o.to_bytes())
        result = i.read_bitset()
        self.assertEqual(result, bits)

    def test_bitset_large(self):
        bits = [i % 3 == 0 for i in range(100)]
        o = O(); o.write_bitset(bits)
        i = I(o.to_bytes())
        result = i.read_bitset()
        self.assertEqual(result, bits)


class TestMultiFieldRoundtrip(unittest.TestCase):
    """多字段连续读写往返测试"""

    def test_mixed_types(self):
        """连续写入多种类型后读取"""
        o = O()
        o.write_u8(42)
        o.write_i32(-100)
        o.write_f64(3.14)
        o.write_bl(True)
        o.write_string("hello")
        o.write_Array([1, 2, 3], lambda v: o.write_u32(v))

        i = I(o.to_bytes())
        self.assertEqual(i.read_u8(), 42)
        self.assertEqual(i.read_i32(), -100)
        self.assertAlmostEqual(i.read_f64(), 3.14, places=10)
        self.assertTrue(i.read_bl())
        self.assertEqual(i.read_string(), "hello")
        self.assertEqual(i.read_Array(lambda: i.read_u32()), [1, 2, 3])

    def test_offset_reader(self):
        """带偏移量读取"""
        o = O()
        o.write_u32(0)  # padding
        o.write_u32(42)
        o.write_u32(0)  # padding

        data = o.to_bytes()
        i = I(data, offset=4)
        self.assertEqual(i.read_u32(), 42)
        self.assertEqual(i.off, 8)

    def test_has_more_data(self):
        o = O(); o.write_u32(42)
        data = o.to_bytes()
        i = I(data)
        self.assertTrue(i.has_more_data())
        i.read_u32()
        self.assertFalse(i.has_more_data())


class TestSpArray(unittest.TestCase):
    """SpArray 辅助类型测试"""

    def test_create_and_access(self):
        arr = SpArray[str](3, "init")
        self.assertEqual(arr.size(), 3)
        self.assertEqual(arr.at(0), "init")
        self.assertEqual(arr.at(1), "init")
        self.assertEqual(arr.at(2), "init")

    def test_set_and_get(self):
        arr = SpArray[int](3, 0)
        arr.set(0, 10)
        arr.set(1, 20)
        self.assertEqual(arr.at(0), 10)
        self.assertEqual(arr.at(1), 20)

    def test_out_of_bounds(self):
        arr = SpArray[int](2, 0)
        with self.assertRaises(IndexError):
            arr.at(2)
        with self.assertRaises(IndexError):
            arr.set(2, 42)


class TestSpRef(unittest.TestCase):
    """SpRef 辅助类型测试"""

    def test_create(self):
        ref = SpRef("hello", 0x1000)
        self.assertEqual(ref.value, "hello")
        self.assertEqual(ref.address, 0x1000)

    def test_null_ref(self):
        ref = SpRef(None, 0)
        self.assertIsNone(ref.value)
        self.assertEqual(ref.address, 0)


class TestSpVariant(unittest.TestCase):
    """SpVariant 辅助类型测试"""

    def test_create(self):
        v = SpVariant(42)
        self.assertEqual(v.value, 42)

    def test_set(self):
        v = SpVariant()
        v.set("hello")
        self.assertEqual(v.value, "hello")

    def test_none_init(self):
        v = SpVariant()
        self.assertIsNone(v.value)


class TestEdgeCases(unittest.TestCase):
    """边界条件测试"""

    def test_zero_length_array(self):
        o = O(); o.write_Array([], lambda v: o.write_u8(v))
        i = I(o.to_bytes())
        result = i.read_Array(lambda: i.read_u8())
        self.assertEqual(result, [])

    def test_empty_string_vs_none(self):
        """空字符串和 None 是不同的"""
        o = O(); o.write_optional("", lambda v: o.write_string(v))
        i = I(o.to_bytes())
        result = i.read_optional(lambda: i.read_string())
        self.assertEqual(result, "")

        o2 = O(); o2.write_optional(None, lambda v: o.write_string(v))
        i2 = I(o2.to_bytes())
        result2 = i2.read_optional(lambda: i2.read_string())
        self.assertIsNone(result2)

    def test_max_u32_array(self):
        """最大 u32 值数组"""
        data = [0xFFFFFFFF, 0, 0xFFFFFFFF]
        o = O(); o.write_Array(data, lambda v: o.write_u32(v))
        i = I(o.to_bytes())
        result = i.read_Array(lambda: i.read_u32())
        self.assertEqual(result, data)

    def test_negative_in_array(self):
        """数组中的负值"""
        data = [-1, 0, 1, -100, 100]
        o = O(); o.write_Array(data, lambda v: o.write_i32(v))
        i = I(o.to_bytes())
        result = i.read_Array(lambda: i.read_i32())
        self.assertEqual(result, data)


if __name__ == '__main__':
    unittest.main()