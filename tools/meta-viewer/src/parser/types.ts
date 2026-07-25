export interface MetaHeader {
  magic: number;
  version: number;
  typeCount: number;
}

export const TokenCategory = {
  DataOption: "dataOption",
  Marker: "marker",
  Container: "container",
  BasicType: "basicType",
  Pointer: "pointer",
  VoidPtr: "voidPtr",
  Const: "const",
  Chrono: "chrono",
  Base: "base",
  CustomType: "customType",
  Sentinel: "sentinel",
  Unknown: "unknown",
} as const;

export type TokenCategory = (typeof TokenCategory)[keyof typeof TokenCategory];

export interface TokenInfo {
  raw: number;
  name: string;
  category: TokenCategory;
}

export interface ParsedTypeDesc {
  tokens: TokenInfo[];
  children: ParsedTypeDesc[];
  extra?: string;
}

export interface MemberMeta {
  name: string;
  descLen: number;
  rawTokens: number[];
  parsedDesc: ParsedTypeDesc;
}

export interface TypeMeta {
  typeID: number;
  className: string;
  baseName: string;
  memberCount: number;
  members: MemberMeta[];
}

export interface MetaFile {
  header: MetaHeader;
  types: TypeMeta[];
}