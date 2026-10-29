/*
 * DHB - Deepin-HarmonyOS-Bridge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wire-compatible implementation of docs/03-protocol.md for ArkTS.
 * Keep this file in sync with include/dhb/protocol.h and src/protocol.cpp.
 */

export const PROTOCOL_MAGIC: number = 0x31424844;
export const PROTOCOL_VERSION: number = 1;
export const FRAME_HEADER_SIZE: number = 20;
export const MAX_FRAME_PAYLOAD: number = 16 * 1024 * 1024;
export const MAX_CLIPBOARD_BYTES: number = 60000;

export enum FrameType {
  HELLO = 0x01,
  HELLO_ACK = 0x02,
  AUTH_CHALLENGE = 0x03,
  AUTH_RESPONSE = 0x04,
  AUTH_OK = 0x05,
  AUTH_FAIL = 0x06,
  PING = 0x10,
  PONG = 0x11,
  FILE_OFFER = 0x20,
  FILE_ACCEPT = 0x21,
  FILE_REJECT = 0x22,
  FILE_CHUNK = 0x23,
  FILE_DONE = 0x24,
  FILE_RESULT = 0x25,
  FILE_CANCEL = 0x26,
  FILE_PROGRESS = 0x27,
  CLIPBOARD_SET = 0x30,
  CLIPBOARD_DATA = 0x31,
  INPUT_EVENT = 0x40,
  SCREEN_CONFIG = 0x50,
  SCREEN_FRAME = 0x51,
  SCREEN_KEYFRAME_REQUEST = 0x52,
  ERROR = 0x60,
  BYE = 0x7f
}

export enum Capability {
  FILE = 1,
  CLIPBOARD = 2,
  SCREEN_SINK = 4,
  SCREEN_SOURCE = 8,
  INPUT = 16,
  NOTIFY = 32,
  DISCOVERY = 64
}

export class Frame {
  type: FrameType = FrameType.ERROR;
  flags: number = 0;
  stream: number = 0;
  payload: Uint8Array = new Uint8Array(0);
}

export class HelloMessage {
  proto: number = PROTOCOL_VERSION;
  flags: number = 0;
  caps: number = 0;
  nonce: Uint8Array = new Uint8Array(16);
  deviceId: string = '';
  deviceName: string = '';
  os: string = '';
  appVersion: string = '';
}

export class FileOfferMessage {
  id: number = 0;
  size: number = 0;
  mtime: number = 0;
  mode: number = 0;
  sha256: Uint8Array = new Uint8Array(32);
  name: string = '';
}

export class FileAcceptMessage {
  id: number = 0;
  offset: number = 0;
}

export class FileRejectMessage {
  id: number = 0;
  code: number = 0;
  reason: string = '';
}

export class FileDoneMessage {
  id: number = 0;
  sha256: Uint8Array = new Uint8Array(32);
}

export class FileResultMessage {
  id: number = 0;
  status: number = 0;
  sha256: Uint8Array = new Uint8Array(32);
  message: string = '';
}

export class ClipboardSetMessage {
  text: string = '';
}

export class PingMessage {
  timestampMs: number = 0;
}

// ---------------------------------------------------------------------------
// UTF-8 (manual, so the file has no runtime-specific TextEncoder dependency)
// ---------------------------------------------------------------------------
export function utf8Encode(text: string): Uint8Array {
  const bytes: number[] = [];
  for (let i = 0; i < text.length; i++) {
    let cp: number = text.charCodeAt(i);
    if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < text.length) {
      const low: number = text.charCodeAt(i + 1);
      if (low >= 0xdc00 && low <= 0xdfff) {
        cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
        i++;
      }
    }
    if (cp < 0x80) {
      bytes.push(cp);
    } else if (cp < 0x800) {
      bytes.push(0xc0 | (cp >> 6));
      bytes.push(0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
      bytes.push(0xe0 | (cp >> 12));
      bytes.push(0x80 | ((cp >> 6) & 0x3f));
      bytes.push(0x80 | (cp & 0x3f));
    } else {
      bytes.push(0xf0 | (cp >> 18));
      bytes.push(0x80 | ((cp >> 12) & 0x3f));
      bytes.push(0x80 | ((cp >> 6) & 0x3f));
      bytes.push(0x80 | (cp & 0x3f));
    }
  }
  return new Uint8Array(bytes);
}

export function utf8Decode(data: Uint8Array): string {
  let out: string = '';
  let i: number = 0;
  while (i < data.length) {
    const first: number = data[i++];
    let cp: number = 0xfffd;
    if (first < 0x80) {
      cp = first;
    } else if ((first & 0xe0) === 0xc0 && i < data.length) {
      cp = ((first & 0x1f) << 6) | (data[i++] & 0x3f);
    } else if ((first & 0xf0) === 0xe0 && i + 1 < data.length) {
      cp = ((first & 0x0f) << 12) | ((data[i++] & 0x3f) << 6) | (data[i++] & 0x3f);
    } else if ((first & 0xf8) === 0xf0 && i + 2 < data.length) {
      cp = ((first & 0x07) << 18) | ((data[i++] & 0x3f) << 12) |
           ((data[i++] & 0x3f) << 6) | (data[i++] & 0x3f);
    }
    if (cp <= 0xffff) {
      out += String.fromCharCode(cp);
    } else {
      cp -= 0x10000;
      out += String.fromCharCode(0xd800 + (cp >> 10));
      out += String.fromCharCode(0xdc00 + (cp & 0x3ff));
    }
  }
  return out;
}

export function bytesToHex(data: Uint8Array): string {
  const digits: string = '0123456789abcdef';
  let out: string = '';
  for (let i = 0; i < data.length; i++) {
    out += digits.charAt((data[i] >> 4) & 0x0f);
    out += digits.charAt(data[i] & 0x0f);
  }
  return out;
}

export function hexToBytes(text: string): Uint8Array | null {
  if (text.length % 2 !== 0) return null;
  const out = new Uint8Array(text.length / 2);
  for (let i = 0; i < out.length; i++) {
    const hi = hexNibble(text.charAt(i * 2));
    const lo = hexNibble(text.charAt(i * 2 + 1));
    if (hi < 0 || lo < 0) return null;
    out[i] = (hi << 4) | lo;
  }
  return out;
}

function hexNibble(c: string): number {
  if (c >= '0' && c <= '9') return c.charCodeAt(0) - 48;
  if (c >= 'a' && c <= 'f') return c.charCodeAt(0) - 87;
  if (c >= 'A' && c <= 'F') return c.charCodeAt(0) - 55;
  return -1;
}

// ---------------------------------------------------------------------------
// CRC-32C
// ---------------------------------------------------------------------------
let CRC_TABLE: number[] = [];

function crcTable(): number[] {
  if (CRC_TABLE.length !== 0) return CRC_TABLE;
  const table: number[] = [];
  for (let i = 0; i < 256; i++) {
    let c: number = i;
    for (let k = 0; k < 8; k++) {
      c = (c & 1) !== 0 ? (0x82f63b78 ^ (c >>> 1)) : (c >>> 1);
    }
    table.push(c >>> 0);
  }
  CRC_TABLE = table;
  return CRC_TABLE;
}

export function crc32c(data: Uint8Array): number {
  const table = crcTable();
  let crc: number = 0xffffffff;
  for (let i = 0; i < data.length; i++) {
    crc = (table[(crc ^ data[i]) & 0xff] ^ (crc >>> 8)) >>> 0;
  }
  return (~crc) >>> 0;
}

// ---------------------------------------------------------------------------
// SHA-256 / HMAC / HKDF
// ---------------------------------------------------------------------------
const SHA256_K: number[] = [
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
  0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
  0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
  0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
  0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
  0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
];

function rotr32(x: number, n: number): number {
  return ((x >>> n) | (x << (32 - n))) >>> 0;
}

export class Sha256 {
  private state: number[] = [
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
  ];
  private buffer: Uint8Array = new Uint8Array(0);
  private totalBytes: number = 0;

  update(data: Uint8Array): void {
    this.totalBytes += data.length;
    let offset: number = 0;
    if (this.buffer.length !== 0) {
      const merged = new Uint8Array(this.buffer.length + data.length);
      merged.set(this.buffer, 0);
      merged.set(data, this.buffer.length);
      data = merged;
      this.buffer = new Uint8Array(0);
    }
    while (offset + 64 <= data.length) {
      this.transform(data.subarray(offset, offset + 64));
      offset += 64;
    }
    if (offset < data.length) {
      this.buffer = data.slice(offset);
    }
  }

  digest(): Uint8Array {
    const bitLen: number = this.totalBytes * 8;
    const padded: number[] = [];
    for (let i = 0; i < this.buffer.length; i++) padded.push(this.buffer[i]);
    padded.push(0x80);
    while (padded.length % 64 !== 56) padded.push(0);
    const high: number = Math.floor(bitLen / 0x100000000);
    const low: number = bitLen >>> 0;
    for (let i = 3; i >= 0; i--) padded.push((high >>> (i * 8)) & 0xff);
    for (let i = 3; i >= 0; i--) padded.push((low >>> (i * 8)) & 0xff);
    const block = new Uint8Array(64);
    for (let offset = 0; offset < padded.length; offset += 64) {
      for (let i = 0; i < 64; i++) block[i] = padded[offset + i];
      this.transform(block);
    }
    const out = new Uint8Array(32);
    for (let i = 0; i < 8; i++) {
      out[i * 4] = (this.state[i] >>> 24) & 0xff;
      out[i * 4 + 1] = (this.state[i] >>> 16) & 0xff;
      out[i * 4 + 2] = (this.state[i] >>> 8) & 0xff;
      out[i * 4 + 3] = this.state[i] & 0xff;
    }
    return out;
  }

  private transform(block: Uint8Array): void {
    const w: number[] = [];
    for (let i = 0; i < 16; i++) {
      w.push(((block[i * 4] << 24) | (block[i * 4 + 1] << 16) |
              (block[i * 4 + 2] << 8) | block[i * 4 + 3]) >>> 0);
    }
    for (let i = 16; i < 64; i++) {
      const s0 = (rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >>> 3)) >>> 0;
      const s1 = (rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >>> 10)) >>> 0;
      w.push((w[i - 16] + s0 + w[i - 7] + s1) >>> 0);
    }
    let a = this.state[0];
    let b = this.state[1];
    let c = this.state[2];
    let d = this.state[3];
    let e = this.state[4];
    let f = this.state[5];
    let g = this.state[6];
    let h = this.state[7];
    for (let i = 0; i < 64; i++) {
      const s1 = (rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25)) >>> 0;
      const ch = ((e & f) ^ ((~e) & g)) >>> 0;
      const t1 = (h + s1 + ch + SHA256_K[i] + w[i]) >>> 0;
      const s0 = (rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22)) >>> 0;
      const maj = ((a & b) ^ (a & c) ^ (b & c)) >>> 0;
      const t2 = (s0 + maj) >>> 0;
      h = g;
      g = f;
      f = e;
      e = (d + t1) >>> 0;
      d = c;
      c = b;
      b = a;
      a = (t1 + t2) >>> 0;
    }
    this.state[0] = (this.state[0] + a) >>> 0;
    this.state[1] = (this.state[1] + b) >>> 0;
    this.state[2] = (this.state[2] + c) >>> 0;
    this.state[3] = (this.state[3] + d) >>> 0;
    this.state[4] = (this.state[4] + e) >>> 0;
    this.state[5] = (this.state[5] + f) >>> 0;
    this.state[6] = (this.state[6] + g) >>> 0;
    this.state[7] = (this.state[7] + h) >>> 0;
  }
}

export function sha256(data: Uint8Array): Uint8Array {
  const hasher = new Sha256();
  hasher.update(data);
  return hasher.digest();
}

export function hmacSha256(key: Uint8Array, data: Uint8Array): Uint8Array {
  let normalized: Uint8Array = key;
  if (normalized.length > 64) normalized = sha256(normalized);
  const ipad = new Uint8Array(64);
  const opad = new Uint8Array(64);
  for (let i = 0; i < 64; i++) {
    const k = i < normalized.length ? normalized[i] : 0;
    ipad[i] = k ^ 0x36;
    opad[i] = k ^ 0x5c;
  }
  const inner = new Uint8Array(64 + data.length);
  inner.set(ipad, 0);
  inner.set(data, 64);
  const innerHash = sha256(inner);
  const outer = new Uint8Array(64 + 32);
  outer.set(opad, 0);
  outer.set(innerHash, 64);
  return sha256(outer);
}

export function hkdfSha256(ikm: Uint8Array, salt: Uint8Array, info: Uint8Array): Uint8Array {
  const prk = hmacSha256(salt, ikm);
  const t = new Uint8Array(info.length + 1);
  t.set(info, 0);
  t[info.length] = 0x01;
  return hmacSha256(prk, t);
}

export function normalizePsk(psk: string): Uint8Array {
  const decoded = hexToBytes(psk);
  if (psk.length === 64 && decoded !== null && decoded.length === 32) return decoded;
  return sha256(utf8Encode(psk));
}

export function authProof(psk: Uint8Array, clientNonce: Uint8Array, serverNonce: Uint8Array,
                          challengeNonce: Uint8Array, clientId: string, serverId: string): Uint8Array {
  const writer = new ByteWriter();
  writer.bytes(utf8Encode('DHB1-AUTH'));
  writer.bytes(clientNonce);
  writer.bytes(serverNonce);
  writer.bytes(challengeNonce);
  writer.str(clientId);
  writer.str(serverId);
  return hmacSha256(psk, writer.toUint8Array());
}

export function deriveSessionKey(psk: Uint8Array, clientNonce: Uint8Array,
                                 serverNonce: Uint8Array): Uint8Array {
  const salt = new Uint8Array(clientNonce.length + serverNonce.length);
  salt.set(clientNonce, 0);
  salt.set(serverNonce, clientNonce.length);
  return hkdfSha256(psk, salt, utf8Encode('DHB1-SESSION'));
}

// ---------------------------------------------------------------------------
// Byte writer / reader
// ---------------------------------------------------------------------------
export class ByteWriter {
  private buf: number[] = [];

  u8(value: number): void {
    this.buf.push(value & 0xff);
  }

  u16(value: number): void {
    this.u8(value & 0xff);
    this.u8((value >>> 8) & 0xff);
  }

  u32(value: number): void {
    this.u8(value & 0xff);
    this.u8((value >>> 8) & 0xff);
    this.u8((value >>> 16) & 0xff);
    this.u8((value >>> 24) & 0xff);
  }

  u64(value: number): void {
    const low = value % 0x100000000;
    const high = Math.floor(value / 0x100000000);
    this.u32(low);
    this.u32(high);
  }

  bytes(data: Uint8Array): void {
    for (let i = 0; i < data.length; i++) this.buf.push(data[i]);
  }

  str(value: string): void {
    const encoded = utf8Encode(value);
    const length = Math.min(encoded.length, 65535);
    this.u16(length);
    for (let i = 0; i < length; i++) this.buf.push(encoded[i]);
  }

  toUint8Array(): Uint8Array {
    return new Uint8Array(this.buf);
  }
}

export class ByteReader {
  private view: DataView;
  private pos: number = 0;

  constructor(data: Uint8Array) {
    this.view = new DataView(data.buffer, data.byteOffset, data.byteLength);
  }

  u8(): number | null {
    if (this.pos + 1 > this.view.byteLength) return null;
    return this.view.getUint8(this.pos++);
  }

  u16(): number | null {
    if (this.pos + 2 > this.view.byteLength) return null;
    const value = this.view.getUint16(this.pos, true);
    this.pos += 2;
    return value;
  }

  u32(): number | null {
    if (this.pos + 4 > this.view.byteLength) return null;
    const value = this.view.getUint32(this.pos, true);
    this.pos += 4;
    return value;
  }

  u64(): number | null {
    if (this.pos + 8 > this.view.byteLength) return null;
    const low = this.view.getUint32(this.pos, true);
    const high = this.view.getUint32(this.pos + 4, true);
    this.pos += 8;
    return high * 0x100000000 + low;
  }

  bytes(size: number): Uint8Array | null {
    if (size < 0 || this.pos + size > this.view.byteLength) return null;
    const out = new Uint8Array(size);
    for (let i = 0; i < size; i++) out[i] = this.view.getUint8(this.pos + i);
    this.pos += size;
    return out;
  }

  str(maxLength: number = 4096): string | null {
    const length = this.u16();
    if (length === null || length > maxLength) return null;
    const data = this.bytes(length);
    if (data === null) return null;
    return utf8Decode(data);
  }

  remaining(): number {
    return this.view.byteLength - this.pos;
  }

  done(): boolean {
    return this.pos === this.view.byteLength;
  }
}

// ---------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------
export function encodeFrame(frame: Frame): Uint8Array {
  const out = new Uint8Array(FRAME_HEADER_SIZE + frame.payload.length);
  const view = new DataView(out.buffer);
  view.setUint32(0, PROTOCOL_MAGIC, true);
  view.setUint8(4, PROTOCOL_VERSION);
  view.setUint8(5, frame.type);
  view.setUint16(6, frame.flags, true);
  view.setUint32(8, frame.stream, true);
  view.setUint32(12, frame.payload.length, true);
  view.setUint32(16, crc32c(frame.payload), true);
  out.set(frame.payload, FRAME_HEADER_SIZE);
  return out;
}

function parseFrame(buffer: Uint8Array): Frame | null {
  if (buffer.length < FRAME_HEADER_SIZE) return null;
  const view = new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);
  if (view.getUint32(0, true) !== PROTOCOL_MAGIC) throw new Error('bad frame magic');
  if (view.getUint8(4) !== PROTOCOL_VERSION) throw new Error('bad protocol version');
  const type = view.getUint8(5) as FrameType;
  const flags = view.getUint16(6, true);
  const stream = view.getUint32(8, true);
  const length = view.getUint32(12, true);
  const crc = view.getUint32(16, true);
  if (length > MAX_FRAME_PAYLOAD) throw new Error('frame too large');
  if (buffer.length < FRAME_HEADER_SIZE + length) return null;
  const payload = buffer.slice(FRAME_HEADER_SIZE, FRAME_HEADER_SIZE + length);
  if (crc32c(payload) !== crc) throw new Error('frame CRC mismatch');
  const frame = new Frame();
  frame.type = type;
  frame.flags = flags;
  frame.stream = stream;
  frame.payload = payload;
  return frame;
}

export class FrameParser {
  private buffer: Uint8Array = new Uint8Array(0);

  append(chunk: Uint8Array): void {
    if (this.buffer.length === 0) {
      this.buffer = chunk.slice();
      return;
    }
    const merged = new Uint8Array(this.buffer.length + chunk.length);
    merged.set(this.buffer, 0);
    merged.set(chunk, this.buffer.length);
    this.buffer = merged;
  }

  next(): Frame | null {
    const frame = parseFrame(this.buffer);
    if (frame === null) return null;
    const consumed = FRAME_HEADER_SIZE + frame.payload.length;
    this.buffer = this.buffer.slice(consumed);
    return frame;
  }
}

// ---------------------------------------------------------------------------
// Control message codecs
// ---------------------------------------------------------------------------
export function encodeHello(message: HelloMessage): Uint8Array {
  const w = new ByteWriter();
  w.u16(message.proto);
  w.u16(message.flags);
  w.u32(message.caps);
  w.bytes(message.nonce);
  w.str(message.deviceId);
  w.str(message.deviceName);
  w.str(message.os);
  w.str(message.appVersion);
  return w.toUint8Array();
}

export function decodeHello(payload: Uint8Array): HelloMessage | null {
  const r = new ByteReader(payload);
  const message = new HelloMessage();
  const proto = r.u16();
  const flags = r.u16();
  const caps = r.u32();
  const nonce = r.bytes(16);
  const deviceId = r.str(256);
  const deviceName = r.str(256);
  const os = r.str(128);
  const appVersion = r.str(128);
  if (proto === null || flags === null || caps === null || nonce === null ||
      deviceId === null || deviceName === null || os === null || appVersion === null) {
    return null;
  }
  if (!r.done()) return null;
  message.proto = proto;
  message.flags = flags;
  message.caps = caps;
  message.nonce = nonce;
  message.deviceId = deviceId;
  message.deviceName = deviceName;
  message.os = os;
  message.appVersion = appVersion;
  return message;
}

export function encodeFileOffer(message: FileOfferMessage): Uint8Array {
  const w = new ByteWriter();
  w.u32(message.id);
  w.u64(message.size);
  w.u64(message.mtime);
  w.u32(message.mode);
  w.bytes(message.sha256);
  w.str(message.name);
  return w.toUint8Array();
}

export function decodeFileOffer(payload: Uint8Array): FileOfferMessage | null {
  const r = new ByteReader(payload);
  const message = new FileOfferMessage();
  const id = r.u32();
  const size = r.u64();
  const mtime = r.u64();
  const mode = r.u32();
  const hash = r.bytes(32);
  const name = r.str(1024);
  if (id === null || size === null || mtime === null || mode === null || hash === null ||
      name === null || !r.done()) {
    return null;
  }
  message.id = id;
  message.size = size;
  message.mtime = mtime;
  message.mode = mode;
  message.sha256 = hash;
  message.name = name;
  return message;
}

export function encodeFileAccept(message: FileAcceptMessage): Uint8Array {
  const w = new ByteWriter();
  w.u32(message.id);
  w.u64(message.offset);
  return w.toUint8Array();
}

export function decodeFileAccept(payload: Uint8Array): FileAcceptMessage | null {
  const r = new ByteReader(payload);
  const id = r.u32();
  const offset = r.u64();
  if (id === null || offset === null || !r.done()) return null;
  const message = new FileAcceptMessage();
  message.id = id;
  message.offset = offset;
  return message;
}

export function encodeFileReject(message: FileRejectMessage): Uint8Array {
  const w = new ByteWriter();
  w.u32(message.id);
  w.u8(message.code);
  w.str(message.reason);
  return w.toUint8Array();
}

export function decodeFileReject(payload: Uint8Array): FileRejectMessage | null {
  const r = new ByteReader(payload);
  const id = r.u32();
  const code = r.u8();
  const reason = r.str(512);
  if (id === null || code === null || reason === null || !r.done()) return null;
  const message = new FileRejectMessage();
  message.id = id;
  message.code = code;
  message.reason = reason;
  return message;
}

export function encodeFileChunk(id: number, offset: number, data: Uint8Array): Uint8Array {
  const w = new ByteWriter();
  w.u32(id);
  w.u64(offset);
  w.bytes(data);
  return w.toUint8Array();
}

export class FileChunkMessage {
  id: number = 0;
  offset: number = 0;
  data: Uint8Array = new Uint8Array(0);
}

export function decodeFileChunk(payload: Uint8Array): FileChunkMessage | null {
  const r = new ByteReader(payload);
  const id = r.u32();
  const offset = r.u64();
  if (id === null || offset === null || r.remaining() === 0) return null;
  const data = r.bytes(r.remaining());
  if (data === null) return null;
  const message = new FileChunkMessage();
  message.id = id;
  message.offset = offset;
  message.data = data;
  return message;
}

export function encodeFileDone(message: FileDoneMessage): Uint8Array {
  const w = new ByteWriter();
  w.u32(message.id);
  w.bytes(message.sha256);
  return w.toUint8Array();
}

export function decodeFileDone(payload: Uint8Array): FileDoneMessage | null {
  const r = new ByteReader(payload);
  const id = r.u32();
  const hash = r.bytes(32);
  if (id === null || hash === null || !r.done()) return null;
  const message = new FileDoneMessage();
  message.id = id;
  message.sha256 = hash;
  return message;
}

export function encodeFileResult(message: FileResultMessage): Uint8Array {
  const w = new ByteWriter();
  w.u32(message.id);
  w.u8(message.status);
  w.bytes(message.sha256);
  w.str(message.message);
  return w.toUint8Array();
}

export function decodeFileResult(payload: Uint8Array): FileResultMessage | null {
  const r = new ByteReader(payload);
  const id = r.u32();
  const status = r.u8();
  const hash = r.bytes(32);
  const text = r.str(512);
  if (id === null || status === null || hash === null || text === null || !r.done()) {
    return null;
  }
  const message = new FileResultMessage();
  message.id = id;
  message.status = status;
  message.sha256 = hash;
  message.message = text;
  return message;
}

export function encodeClipboard(message: ClipboardSetMessage): Uint8Array {
  const w = new ByteWriter();
  w.str(message.text);
  return w.toUint8Array();
}

export function decodeClipboard(payload: Uint8Array): ClipboardSetMessage | null {
  const r = new ByteReader(payload);
  const text = r.str(MAX_CLIPBOARD_BYTES);
  if (text === null || !r.done()) return null;
  const message = new ClipboardSetMessage();
  message.text = text;
  return message;
}

export function encodePing(message: PingMessage): Uint8Array {
  const w = new ByteWriter();
  w.u64(message.timestampMs);
  return w.toUint8Array();
}

export function decodePing(payload: Uint8Array): PingMessage | null {
  const r = new ByteReader(payload);
  const timestamp = r.u64();
  if (timestamp === null || !r.done()) return null;
  const message = new PingMessage();
  message.timestampMs = timestamp;
  return message;
}

// ---------------------------------------------------------------------------
// Safe file names (mirror of src/session.cpp)
// ---------------------------------------------------------------------------
export function sanitizeFilename(raw: string): string {
  let name: string = raw.replace(/\\/g, '/');
  const slash: number = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);
  let out: string = '';
  for (let i = 0; i < name.length; i++) {
    const code: number = name.charCodeAt(i);
    if (code < 0x20 || code === 0x7f) continue;
    if (name.charAt(i) === '/' || name.charAt(i) === '\\') continue;
    out += name.charAt(i);
  }
  while (out.length > 0 && (out.endsWith(' ') || out.endsWith('.'))) {
    out = out.substring(0, out.length - 1);
  }
  if (out === '' || out === '.' || out === '..') return '';
  if (out.length > 180) out = out.substring(0, 180);
  return out;
}
