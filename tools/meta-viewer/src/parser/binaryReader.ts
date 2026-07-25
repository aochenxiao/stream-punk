export class ParseError extends Error {
  offset?: number;

  constructor(message: string, offset?: number) {
    super(message);
    this.name = "ParseError";
    this.offset = offset;
  }
}

export class BinaryReader {
  private view: DataView;
  private offset: number;
  private decoder: TextDecoder;

  constructor(buffer: ArrayBuffer) {
    this.view = new DataView(buffer);
    this.offset = 0;
    this.decoder = new TextDecoder("utf-8");
  }

  readU32(): number {
    if (this.offset + 4 > this.view.byteLength) {
      throw new ParseError("Unexpected end of file reading u32", this.offset);
    }
    const v = this.view.getUint32(this.offset, true);
    this.offset += 4;
    return v;
  }

  readU16(): number {
    if (this.offset + 2 > this.view.byteLength) {
      throw new ParseError("Unexpected end of file reading u16", this.offset);
    }
    const v = this.view.getUint16(this.offset, true);
    this.offset += 2;
    return v;
  }

  readString(): string {
    const len = this.readU16();
    if (len === 0) return "";
    if (this.offset + len > this.view.byteLength) {
      throw new ParseError("Unexpected end of file reading string", this.offset);
    }
    const bytes = new Uint8Array(this.view.buffer, this.offset, len);
    this.offset += len;
    return this.decoder.decode(bytes);
  }

  readTokenArray(n: number): number[] {
    const tokens: number[] = [];
    for (let i = 0; i < n; i++) {
      tokens.push(this.readU32());
    }
    return tokens;
  }

  getOffset(): number {
    return this.offset;
  }

  isEOF(): boolean {
    return this.offset >= this.view.byteLength;
  }
}