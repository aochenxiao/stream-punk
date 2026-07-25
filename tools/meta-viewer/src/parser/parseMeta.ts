import { BinaryReader, ParseError } from "./binaryReader";
import { makeTokenInfo } from "./spToken";
import type { MetaFile, ParsedTypeDesc, TypeMeta, MemberMeta } from "./types";

const META_MAGIC = 0x53504d44;
const META_VERSION = 1;

// E_type enum values used in walkTypeDesc
const E_type = {
  bg: 5,
  ed: 6,
  vector: 7,
  array: 8,
  string: 9,
  bitset: 10,
  deque: 11,
  list: 12,
  flist: 13,
  set: 14,
  uset: 15,
  map: 16,
  umap: 17,
  sptr: 18,
  wptr: 19,
  uptr: 20,
  opt: 21,
  path: 22,
  atomic: 23,
  variant: 24,
  tuple: 25,
  u8: 26,
  bl: 40,
  ptr: 41,
  voidPtr: 42,
  cst: 43,
  dur: 44,
  timepoint: 45,
  Base: 46,
} as const;

// Returns { span, result } where span is number of tokens consumed
function walkTypeDesc(tokens: number[]): { span: number; result: ParsedTypeDesc } {
  if (tokens.length === 0) {
    return { span: 0, result: { tokens: [], children: [] } };
  }

  const t = tokens[0];

  // Single-token terminals: basic types
  if (t >= E_type.u8 && t <= E_type.bl) {
    return { span: 1, result: { tokens: [makeTokenInfo(t)], children: [] } };
  }

  // Single-token terminals: voidPtr, path, Base
  if (t === E_type.voidPtr || t === E_type.path || t === E_type.Base) {
    return { span: 1, result: { tokens: [makeTokenInfo(t)], children: [] } };
  }

  // Custom types (47-70): single token, looked up from registry
  if (t >= 47 && t <= 70) {
    return { span: 1, result: { tokens: [makeTokenInfo(t)], children: [] } };
  }

  // dur: [dur, Rep_desc..., num, den]
  if (t === E_type.dur) {
    const { span: repSpan, result: repDesc } = walkTypeDesc(tokens.slice(1));
    const numToken = tokens[1 + repSpan];
    const denToken = tokens[1 + repSpan + 1];
    return {
      span: 1 + repSpan + 2,
      result: {
        tokens: [makeTokenInfo(t), makeTokenInfo(numToken), makeTokenInfo(denToken)],
        children: [repDesc],
      },
    };
  }

  // timepoint: [timepoint, Duration_desc...]
  if (t === E_type.timepoint) {
    const { span: durSpan, result: durDesc } = walkTypeDesc(tokens.slice(1));
    return {
      span: 1 + durSpan,
      result: { tokens: [makeTokenInfo(t)], children: [durDesc] },
    };
  }

  // 1-parameter types: string, vector, deque, list, flist, set, uset, sptr, wptr, uptr, opt, atomic, ptr, cst
  if (
    t === E_type.string ||
    t === E_type.vector ||
    t === E_type.deque ||
    t === E_type.list ||
    t === E_type.flist ||
    t === E_type.set ||
    t === E_type.uset ||
    t === E_type.sptr ||
    t === E_type.wptr ||
    t === E_type.uptr ||
    t === E_type.opt ||
    t === E_type.atomic ||
    t === E_type.ptr ||
    t === E_type.cst
  ) {
    const { span: subSpan, result: subDesc } = walkTypeDesc(tokens.slice(1));
    return {
      span: 1 + subSpan,
      result: { tokens: [makeTokenInfo(t)], children: [subDesc] },
    };
  }

  // 2-parameter types: map, umap
  if (t === E_type.map || t === E_type.umap) {
    const { span: n1, result: key } = walkTypeDesc(tokens.slice(1));
    const { span: n2, result: val } = walkTypeDesc(tokens.slice(1 + n1));
    return {
      span: 1 + n1 + n2,
      result: { tokens: [makeTokenInfo(t)], children: [key, val] },
    };
  }

  // array<N, T>: [array, N, T_desc...]
  if (t === E_type.array) {
    const N = tokens[1];
    const { span: subSpan, result: subDesc } = walkTypeDesc(tokens.slice(2));
    return {
      span: 2 + subSpan,
      result: {
        tokens: [makeTokenInfo(t), makeTokenInfo(N)],
        children: [subDesc],
        extra: String(N),
      },
    };
  }

  // bitset<N>: [bitset, N]
  if (t === E_type.bitset) {
    const N = tokens[1];
    return {
      span: 2,
      result: {
        tokens: [makeTokenInfo(t), makeTokenInfo(N)],
        children: [],
        extra: String(N),
      },
    };
  }

  // variant / tuple: N type params, terminated by ed
  if (t === E_type.variant || t === E_type.tuple) {
    const resultTokens = [makeTokenInfo(t)];
    const children: ParsedTypeDesc[] = [];
    let pos = 1;
    while (pos < tokens.length && tokens[pos] !== E_type.ed) {
      const { span: subSpan, result: subDesc } = walkTypeDesc(tokens.slice(pos));
      children.push(subDesc);
      pos += subSpan;
    }
    if (pos < tokens.length && tokens[pos] === E_type.ed) {
      resultTokens.push(makeTokenInfo(tokens[pos]));
      pos++;
    }
    return {
      span: pos,
      result: { tokens: resultTokens, children },
    };
  }

  // Unknown: treat as single token
  return { span: 1, result: { tokens: [makeTokenInfo(t)], children: [] } };
}

export function parseMetaFile(buffer: ArrayBuffer): MetaFile {
  const reader = new BinaryReader(buffer);

  if (buffer.byteLength < 12) {
    throw new ParseError("File too small to contain valid metadata header");
  }

  const magic = reader.readU32();
  if (magic !== META_MAGIC) {
    throw new ParseError(
      `Invalid magic number: 0x${magic.toString(16).toUpperCase()}, expected 0x${META_MAGIC.toString(16)}`,
      0
    );
  }

  const version = reader.readU32();
  if (version !== META_VERSION) {
    throw new ParseError(
      `Unsupported metadata version: ${version}, expected ${META_VERSION}`,
      4
    );
  }

  const typeCount = reader.readU32();

  const types: TypeMeta[] = [];
  for (let ti = 0; ti < typeCount; ti++) {
    const typeMeta: TypeMeta = {
      typeID: reader.readU32(),
      className: reader.readString(),
      baseName: reader.readString(),
      memberCount: 0,
      members: [],
    };

    const memberCount = reader.readU16();
    typeMeta.memberCount = memberCount;

    for (let mi = 0; mi < memberCount; mi++) {
      const memberName = reader.readString();
      const descLen = reader.readU16();
      const rawTokens = reader.readTokenArray(descLen);
      const { result: parsedDesc } = walkTypeDesc(rawTokens);

      const member: MemberMeta = {
        name: memberName,
        descLen,
        rawTokens,
        parsedDesc,
      };
      typeMeta.members.push(member);
    }

    types.push(typeMeta);
  }

  return {
    header: { magic, version, typeCount },
    types,
  };
}