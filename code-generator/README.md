# Stream-Punk Code Generators

This directory contains code generators for Stream-Punk serialization library.

## TypeScript Generator
- **File**: `stream-punk-ts-gen.cpp`
- **Output**: `stream-punk-data.ts`
- **Usage**: Generates TypeScript class definitions and enums from C++ header files
- **Template**: Uses `stream-punk.ts` as base template

## JavaScript Generator
- **File**: `stream-punk-js-gen.cpp` 
- **Output**: `stream-punk-data.js`
- **Usage**: Generates JavaScript class definitions and enums from C++ header files
- **Template**: Uses `stream-punk.js` as base template

## Usage

### TypeScript Generator
```bash
stream-punk-ts-gen --path ./output/stream-punk-data.ts
```

### JavaScript Generator
```bash
stream-punk-js-gen --path ./output/stream-punk-data.js
```

## Key Differences

### TypeScript vs JavaScript Output
- **TypeScript**: Includes type annotations (`: number`, `: string`, etc.)
- **JavaScript**: No type annotations, uses native JavaScript types
- **TypeScript**: Uses `export enum` for enums
- **JavaScript**: Uses `export const` object for enums
- **TypeScript**: Type-safe class definitions with explicit types
- **JavaScript**: Dynamic class definitions without explicit typing

### Type Mappings
Both generators map C++ types to appropriate JavaScript/TypeScript equivalents:
- `u8`, `u16`, `u32`, `i8`, `i16`, `i32`, `f32`, `f64` → `number`
- `u64`, `i64` → `bigint` 
- `bl` → `boolean`
- `ch`, `ch8`, `ch16`, `ch32` → `string`
- STL containers → Native JS equivalents (`Array`, `Set`, `Map`)

## Dependencies
- C++20 compiler
- cxxopts library for command-line parsing
- Stream-Punk headers