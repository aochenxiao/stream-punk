"""Stream-Punk Python Runtime Library
Zero-dependency binary serialization/deserialization for Python.
Compatible with the C++ StreamPunk binary format (little-endian).
"""

import struct
from io import BytesIO
from typing import Optional, Callable, TypeVar, Generic

T = TypeVar('T')


class SpRef(Generic[T]):
    def __init__(self, value: Optional[T], address: int):
        self.value = value
        self.address = address


class SpArray(Generic[T]):
    def __init__(self, size: int, initializer: T = None):
        self._data = [initializer] * size

    def size(self) -> int:
        return len(self._data)

    def at(self, index: int) -> T:
        if index < 0 or index >= len(self._data):
            raise IndexError("Index out of bounds")
        return self._data[index]

    def set(self, index: int, value: T):
        if index < 0 or index >= len(self._data):
            raise IndexError("Index out of bounds")
        self._data[index] = value


class SpVariant:
    def __init__(self, value=None):
        self._value = value
        self._type_index = -1
        if value is not None:
            self._update_type_index()

    def set(self, value):
        self._value = value
        self._update_type_index()

    @property
    def value(self):
        return self._value

    @property
    def type_index(self) -> int:
        return self._type_index

    def _update_type_index(self):
        self._type_index = -1

    def possible_types(self):
        return []


class I:
    def __init__(self, data: bytes, offset: int = 0):
        self._buf = data
        self.off = offset
        self._obj_map: dict[int, object] = {}

    def has_more_data(self) -> bool:
        return self.off < len(self._buf)

    def read_u8(self) -> int:
        v = self._buf[self.off]
        self.off += 1
        return v

    def read_u16(self) -> int:
        v = struct.unpack_from('<H', self._buf, self.off)[0]
        self.off += 2
        return v

    def read_u32(self) -> int:
        v = struct.unpack_from('<I', self._buf, self.off)[0]
        self.off += 4
        return v

    def read_u64(self) -> int:
        v = struct.unpack_from('<Q', self._buf, self.off)[0]
        self.off += 8
        return v

    def read_i8(self) -> int:
        v = struct.unpack_from('<b', self._buf, self.off)[0]
        self.off += 1
        return v

    def read_i16(self) -> int:
        v = struct.unpack_from('<h', self._buf, self.off)[0]
        self.off += 2
        return v

    def read_i32(self) -> int:
        v = struct.unpack_from('<i', self._buf, self.off)[0]
        self.off += 4
        return v

    def read_i64(self) -> int:
        v = struct.unpack_from('<q', self._buf, self.off)[0]
        self.off += 8
        return v

    def read_f32(self) -> float:
        v = struct.unpack_from('<f', self._buf, self.off)[0]
        self.off += 4
        return v

    def read_f64(self) -> float:
        v = struct.unpack_from('<d', self._buf, self.off)[0]
        self.off += 8
        return v

    def read_ch(self) -> str:
        v = self._buf[self.off]
        self.off += 1
        return chr(v)

    def read_ch8(self) -> str:
        v = self._buf[self.off]
        self.off += 1
        return chr(v)

    def read_ch16(self) -> str:
        v = struct.unpack_from('<H', self._buf, self.off)[0]
        self.off += 2
        return chr(v)

    def read_ch32(self) -> str:
        cp = struct.unpack_from('<I', self._buf, self.off)[0]
        self.off += 4
        if 0 <= cp <= 0x10FFFF:
            return chr(cp)
        return ''

    def read_bl(self) -> bool:
        v = self._buf[self.off] != 0
        self.off += 1
        return v

    def read_sz(self) -> int:
        return self.read_u32()

    def read_bytes(self, length: int):
        self.off += length

    def read_string(self) -> str:
        length = self.read_sz()
        if length == 0:
            return ''
        v = self._buf[self.off:self.off + length].decode('utf-8')
        self.off += length
        return v

    def read_u8string(self) -> str:
        length = self.read_sz()
        v = self._buf[self.off:self.off + length].decode('utf-8')
        self.off += length
        return v

    def read_u16string(self) -> str:
        length = self.read_sz()
        byte_len = length * 2
        v = self._buf[self.off:self.off + byte_len].decode('utf-16-le')
        self.off += byte_len
        return v

    def read_u32string(self) -> bytes:
        length = self.read_sz()
        byte_len = length * 4
        v = self._buf[self.off:self.off + byte_len]
        self.off += byte_len
        return v

    def read_ptr_with_typeID(self):
        addr = self.read_u64()
        if addr == 0:
            return SpRef(None, 0)
        if addr in self._obj_map:
            return self._obj_map[addr]
        ref = SpRef(None, addr)
        self._obj_map[addr] = ref
        ref.value = read_obj(self)
        return ref

    def read_ptr(self, reader: Callable[[], T]) -> SpRef[T]:
        addr = self.read_u64()
        if addr == 0:
            return SpRef(None, 0)
        if addr in self._obj_map:
            return self._obj_map[addr]
        value = reader()
        ref = SpRef(value, addr)
        self._obj_map[addr] = ref
        return ref

    def read_array(self, count: int, reader: Callable[[], T]) -> list:
        return [reader() for _ in range(count)]

    def read_Array(self, reader: Callable[[], T]) -> list:
        size = self.read_sz()
        return self.read_array(size, reader)

    def read_set(self, reader: Callable[[], T]) -> set:
        arr = self.read_Array(reader)
        try:
            return set(arr)
        except TypeError:
            return set(tuple(x) for x in arr)

    def read_unordered_set(self, reader: Callable[[], T]) -> set:
        return self.read_set(reader)

    def read_map(self, key_reader: Callable, value_reader: Callable) -> dict:
        size = self.read_sz()
        result = {}
        for _ in range(size):
            k = key_reader()
            v = value_reader()
            result[k] = v
        return result

    def read_unordered_map(self, key_reader: Callable, value_reader: Callable) -> dict:
        return self.read_map(key_reader, value_reader)

    def read_vector(self, reader: Callable[[], T]) -> list:
        return self.read_Array(reader)

    def read_deque(self, reader: Callable[[], T]) -> list:
        return self.read_Array(reader)

    def read_list(self, reader: Callable[[], T]) -> list:
        return self.read_Array(reader)

    def read_forward_list(self, reader: Callable[[], T]) -> list:
        return self.read_Array(reader)

    def read_SpArray(self, size: int, reader: Callable[[], T]) -> SpArray[T]:
        arr = SpArray(size)
        for i in range(size):
            arr.set(i, reader())
        return arr

    def read_std_string(self) -> str:
        return self.read_string()

    def read_bitset(self) -> list:
        size = self.read_u32()
        byte_len = (size + 7) // 8
        result = []
        for i in range(size):
            byte_idx = i // 8
            bit_idx = i % 8
            byte_val = self._buf[self.off + byte_idx]
            result.append(((byte_val >> bit_idx) & 1) == 1)
        self.off += byte_len
        return result

    def read_optional(self, reader: Callable[[], T]) -> Optional[T]:
        if self.read_bl():
            return reader()
        return None

    def read_variant(self, readers: list) -> object:
        index = self.read_u32()
        if index < 0 or index >= len(readers):
            raise IndexError(f"Variant index {index} out of range")
        return readers[index]()

    def read_stream_punk_time(self) -> int:
        sec = self.read_i64()
        atto_sec = self.read_i64()
        return sec * 1000 + atto_sec // 10**15


class O:
    def __init__(self):
        self._buf = BytesIO()
        self._obj_map: dict[int, object] = {}

    def write_u8(self, v: int):
        self._buf.write(struct.pack('<B', v))

    def write_u16(self, v: int):
        self._buf.write(struct.pack('<H', v))

    def write_u32(self, v: int):
        self._buf.write(struct.pack('<I', v))

    def write_u64(self, v: int):
        self._buf.write(struct.pack('<Q', v))

    def write_i8(self, v: int):
        self._buf.write(struct.pack('<b', v))

    def write_i16(self, v: int):
        self._buf.write(struct.pack('<h', v))

    def write_i32(self, v: int):
        self._buf.write(struct.pack('<i', v))

    def write_i64(self, v: int):
        self._buf.write(struct.pack('<q', v))

    def write_f32(self, v: float):
        self._buf.write(struct.pack('<f', v))

    def write_f64(self, v: float):
        self._buf.write(struct.pack('<d', v))

    def write_ch(self, v: str):
        self.write_ch16(v)

    def write_ch8(self, v: str):
        if len(v) > 0:
            self.write_u8(ord(v[0]))
        else:
            self.write_u8(0)

    def write_ch16(self, v: str):
        if len(v) > 0:
            self.write_u16(ord(v[0]))
        else:
            self.write_u16(0)

    def write_ch32(self, v: str):
        if len(v) > 0:
            self.write_u32(ord(v[0]))
        else:
            self.write_u32(0)

    def write_bl(self, v: bool):
        self.write_u8(1 if v else 0)

    def write_sz(self, v: int):
        self.write_u32(v)

    def write_string(self, v: str):
        encoded = v.encode('utf-8')
        self.write_sz(len(encoded))
        self._buf.write(encoded)

    def write_u8string(self, v: str):
        encoded = v.encode('utf-8')
        self.write_sz(len(encoded))
        self._buf.write(encoded)

    def write_u16string(self, v: str):
        encoded = v.encode('utf-16-le')
        self.write_sz(len(v))
        self._buf.write(encoded)

    def write_u32string(self, v: bytes):
        self.write_sz(len(v) // 4)
        self._buf.write(v)

    def write_ptr(self, value, address: int, writer: Callable):
        if value is None:
            self.write_u64(0)
            return
        if address == 0:
            address = len(self._obj_map) + 1
        if address in self._obj_map:
            self.write_u64(address)
            return
        self._obj_map[address] = value
        self.write_u64(address)
        writer(value)

    def write_ptr_with_typeID(self, value, address: int = 0):
        if value is None:
            self.write_u64(0)
            return
        if address == 0:
            address = len(self._obj_map) + 1
        if address in self._obj_map:
            self.write_u64(address)
            return
        self._obj_map[address] = value
        self.write_u64(address)
        write_obj(self, value)

    def write_array(self, arr: list, writer: Callable):
        for item in arr:
            writer(item)

    def write_Array(self, arr: list, writer: Callable):
        self.write_sz(len(arr))
        self.write_array(arr, writer)

    def write_set(self, s: set, writer: Callable):
        self.write_Array(list(s), writer)

    def write_unordered_set(self, s: set, writer: Callable):
        self.write_set(s, writer)

    def write_map(self, d: dict, key_writer: Callable, value_writer: Callable):
        self.write_sz(len(d))
        for k, v in d.items():
            key_writer(k)
            value_writer(v)

    def write_unordered_map(self, d: dict, key_writer: Callable, value_writer: Callable):
        self.write_map(d, key_writer, value_writer)

    def write_vector(self, vec: list, writer: Callable):
        self.write_Array(vec, writer)

    def write_deque(self, deq: list, writer: Callable):
        self.write_Array(deq, writer)

    def write_list(self, lst: list, writer: Callable):
        self.write_Array(lst, writer)

    def write_forward_list(self, lst: list, writer: Callable):
        self.write_Array(lst, writer)

    def write_SpArray(self, arr: SpArray, writer: Callable):
        for i in range(arr.size()):
            writer(arr.at(i))

    def write_bitset(self, bits: list):
        self.write_u32(len(bits))
        byte_len = (len(bits) + 7) // 8
        bytes_arr = bytearray(byte_len)
        for i, bit in enumerate(bits):
            if bit:
                byte_idx = i // 8
                bit_idx = i % 8
                bytes_arr[byte_idx] |= (1 << bit_idx)
        self._buf.write(bytes_arr)

    def write_optional(self, value, writer: Callable):
        self.write_bl(value is not None)
        if value is not None:
            writer(value)

    def write_variant(self, value, index: int, writers: list):
        self.write_u32(index)
        writers[index](value)

    def write_stream_punk_time(self, t_ms: int):
        sec = t_ms // 1000
        atto_sec = (t_ms % 1000) * 10**15
        self.write_i64(sec)
        self.write_i64(atto_sec)

    def to_bytes(self) -> bytes:
        return self._buf.getvalue()