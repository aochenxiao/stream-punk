import { TokenCategory } from "./types";
import type { TokenInfo } from "./types";

const TOKEN_NAMES: Record<number, string> = {
  0: "e_unknowType",
  1: "e_op_position",
  2: "e_op_select",
  3: "e_op_deptr",
  4: "e_op_ranges_insert_one",
  5: "bg",
  6: "ed",
  7: "vector",
  8: "array",
  9: "string",
  10: "bitset",
  11: "deque",
  12: "list",
  13: "flist",
  14: "set",
  15: "uset",
  16: "map",
  17: "umap",
  18: "sptr",
  19: "wptr",
  20: "uptr",
  21: "opt",
  22: "path",
  23: "atomic",
  24: "variant",
  25: "tuple",
  26: "u8",
  27: "u16",
  28: "u32",
  29: "u64",
  30: "i8",
  31: "i16",
  32: "i32",
  33: "i64",
  34: "f32",
  35: "f64",
  36: "ch",
  37: "ch8",
  38: "ch16",
  39: "ch32",
  40: "bl",
  41: "ptr",
  42: "voidPtr",
  43: "cst",
  44: "dur",
  45: "timepoint",
  46: "Base",
  47: "AllBasicTypes",
  48: "TemplateContainer",
  49: "PointerContainer",
  50: "ComplexTemplateNesting",
  51: "ComprehensiveContainer",
  52: "Child",
  53: "SelfReferential",
  54: "TemplateAndPointer",
  55: "InheritanceAndSelfReference",
  56: "MegaComplexClass",
  57: "SuperComplexContainer",
  58: "Test",
  59: "MQTT",
  60: "PointerDemo",
  61: "ContainerDemo",
  62: "NetworkSystem",
  63: "Device",
  64: "NetworkDevice",
  65: "Sensor",
  66: "TemperatureSensor",
  67: "SmartHomeSystem",
  68: "MultiLevelContainer",
  69: "SptrTest",
  70: "MousePosition",
  71: "e_customType",
};

export function getTokenName(token: number): string {
  return TOKEN_NAMES[token] ?? `unknown_${token}`;
}

export function getTokenCategory(token: number): TokenCategory {
  if (token >= 0 && token <= 4) return TokenCategory.DataOption;
  if (token >= 5 && token <= 6) return TokenCategory.Marker;
  if (token >= 7 && token <= 25) return TokenCategory.Container;
  if (token >= 26 && token <= 40) return TokenCategory.BasicType;
  if (token === 41) return TokenCategory.Pointer;
  if (token === 42) return TokenCategory.VoidPtr;
  if (token === 43) return TokenCategory.Const;
  if (token >= 44 && token <= 45) return TokenCategory.Chrono;
  if (token === 46) return TokenCategory.Base;
  if (token >= 47 && token <= 70) return TokenCategory.CustomType;
  if (token === 71) return TokenCategory.Sentinel;
  return TokenCategory.Unknown;
}

export function makeTokenInfo(token: number): TokenInfo {
  return {
    raw: token,
    name: getTokenName(token),
    category: getTokenCategory(token),
  };
}

export const CATEGORY_COLORS: Record<TokenCategory, string> = {
  [TokenCategory.BasicType]: "#4caf50",
  [TokenCategory.Container]: "#2196f3",
  [TokenCategory.Pointer]: "#ff9800",
  [TokenCategory.CustomType]: "#ce93d8",
  [TokenCategory.Chrono]: "#00bcd4",
  [TokenCategory.Marker]: "#9e9e9e",
  [TokenCategory.DataOption]: "#a1887f",
  [TokenCategory.Sentinel]: "#ef5350",
  [TokenCategory.Base]: "#78909c",
  [TokenCategory.VoidPtr]: "#90a4ae",
  [TokenCategory.Const]: "#90a4ae",
  [TokenCategory.Unknown]: "#607d8b",
};