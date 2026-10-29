/*
 * DHB - Deepin-HarmonyOS-Bridge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Core client: always connects out to the Deepin/UOS daemon, then reuses the
 * same session for bidirectional file transfer, clipboard and (later) screen.
 */

import { common } from '@kit.AbilityKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { BusinessError, deviceInfo } from '@kit.BasicServicesKit';
import { socket } from '@kit.NetworkKit';
import * as P from './Protocol';

class Waiter {
  type: number;
  resolve: (frame: P.Frame) => void;
  reject: (error: Error) => void;
  timer: number;

  constructor(type: number, resolve: (frame: P.Frame) => void,
              reject: (error: Error) => void, timer: number) {
    this.type = type;
    this.resolve = resolve;
    this.reject = reject;
    this.timer = timer;
  }
}

class ReceiveState {
  id: number = 0;
  name: string = '';
  partPath: string = '';
  finalPath: string = '';
  size: number = 0;
  offset: number = 0;
  fd: number = -1;
  hasher: P.Sha256 = new P.Sha256();
}

export class BridgeClient {
  private tcp: socket.TCPSocket | null = null;
  private parser: P.FrameParser = new P.FrameParser();
  private connected: boolean = false;
  private psk: Uint8Array = new Uint8Array(0);
  private localNonce: Uint8Array = new Uint8Array(16);
  private peerNonce: Uint8Array = new Uint8Array(16);
  private peerName: string = '';
  private localId: string = '';
  private localName: string = '';
  private receivedDir: string = '';
  private waiters: Map<number, Waiter> = new Map<number, Waiter>();
  private earlyFrames: Map<number, P.Frame[]> = new Map<number, P.Frame[]>();
  private authError: string = '';
  private fileRejectReason: string = '';
  private receive: ReceiveState | null = null;
  private nextTransferId: number = 1;

  onLog: (message: string) => void = (message: string): void => {};
  onState: (state: string) => void = (state: string): void => {};
  onProgress: (sent: number, total: number) => void =
    (sent: number, total: number): void => {};
  onFileReceived: (path: string) => void = (path: string): void => {};
  onClipboard: (text: string) => void = (text: string): void => {};

  constructor(context: common.UIAbilityContext) {
    this.receivedDir = context.filesDir + '/received';
    try {
      fs.mkdirSync(this.receivedDir);
    } catch (error) {
      // Already exists or cannot be created; the offer path will report it.
    }
    const seed = deviceInfo.productModel + '|' + deviceInfo.brand + '|' + deviceInfo.sdkApiVersion;
    this.localId = P.bytesToHex(P.sha256(P.utf8Encode(seed))).substring(0, 16);
    this.localName = deviceInfo.productModel;
  }

  async connect(host: string, port: number, psk: string): Promise<void> {
    if (this.connected) await this.close();
    this.psk = P.normalizePsk(psk);
    if (this.psk.length !== 32) throw new Error('invalid pairing key');

    const tcp = socket.constructTCPSocketInstance();
    this.tcp = tcp;
    tcp.on('message', (info: socket.SocketMessageInfo): void => {
      this.onData(info.message);
    });
    tcp.on('close', (): void => {
      this.connected = false;
      this.onState('disconnected');
      this.rejectAll(new Error('connection closed'));
    });
    tcp.on('error', (error: BusinessError): void => {
      this.onLog('socket error: ' + error.message);
      this.rejectAll(new Error(error.message));
    });

    await tcp.connect({
      address: { address: host, port: port, family: 1 },
      timeout: 6000
    });
    this.connected = true;
    this.onState('connected');
    this.onLog('TCP connected to ' + host + ':' + port);

    this.localNonce = this.randomBytes(16);
    const hello = new P.HelloMessage();
    hello.caps = P.Capability.FILE | P.Capability.CLIPBOARD |
      P.Capability.SCREEN_SINK | P.Capability.INPUT;
    hello.nonce = this.localNonce;
    hello.deviceId = this.localId;
    hello.deviceName = this.localName;
    hello.os = 'HarmonyOS';
    hello.appVersion = '0.1.0';
    await this.sendFrame(P.FrameType.HELLO, 0, P.encodeHello(hello));

    const helloFrame = await this.waitFor(P.FrameType.HELLO, 5000);
    const peer = P.decodeHello(helloFrame.payload);
    if (peer === null) throw new Error('malformed HELLO from peer');
    this.peerNonce = peer.nonce;
    this.peerName = peer.deviceName;
    this.onLog('peer: ' + peer.deviceName + ' (' + peer.os + ')');

    const challengeFrame = await this.waitFor(P.FrameType.AUTH_CHALLENGE, 5000);
    const reader = new P.ByteReader(challengeFrame.payload);
    const challenge = reader.bytes(16);
    if (challenge === null) throw new Error('malformed AUTH_CHALLENGE');

    const proof = P.authProof(this.psk, this.localNonce, this.peerNonce, challenge,
                              this.localId, peer.deviceId);
    await this.sendFrame(P.FrameType.AUTH_RESPONSE, 0, proof);

    await this.waitFor(P.FrameType.AUTH_OK, 5000);
    this.onLog('authenticated with ' + this.peerName);
    this.onState('authenticated');
  }

  async close(): Promise<void> {
    this.connected = false;
    this.rejectAll(new Error('closed by user'));
    if (this.tcp !== null) {
      const tcp = this.tcp;
      this.tcp = null;
      try {
        await tcp.close();
      } catch (error) {
        // Already closed.
      }
    }
    this.onState('disconnected');
  }

  isConnected(): boolean {
    return this.connected;
  }

  async sendClipboard(text: string): Promise<void> {
    if (!this.connected) throw new Error('not connected');
    if (text.length === 0) return;
    const message = new P.ClipboardSetMessage();
    message.text = text;
    await this.sendFrame(P.FrameType.CLIPBOARD_SET, 0, P.encodeClipboard(message));
  }

  async sendFile(uri: string): Promise<void> {
    if (!this.connected) throw new Error('not connected');
    const file = fs.openSync(uri, fs.OpenMode.READ_ONLY);
    try {
      const stat = fs.statSync(file.fd);
      const size = stat.size;
      const hasher = new P.Sha256();
      const buffer = new ArrayBuffer(256 * 1024);
      let hashed = 0;
      while (hashed < size) {
        const want = Math.min(256 * 1024, size - hashed);
        const n = fs.readSync(file.fd, buffer, { offset: hashed, length: want });
        if (n <= 0) break;
        hasher.update(new Uint8Array(buffer, 0, n));
        hashed += n;
      }
      if (hashed !== size) throw new Error('cannot read the complete file');
      const hash = hasher.digest();

      const id = this.nextTransferId++;
      const offer = new P.FileOfferMessage();
      offer.id = id;
      offer.size = size;
      offer.mtime = Math.floor(Date.now() / 1000);
      offer.mode = 0o644;
      offer.sha256 = hash;
      offer.name = uri.substring(uri.lastIndexOf('/') + 1);
      this.fileRejectReason = '';
      await this.sendFrame(P.FrameType.FILE_OFFER, 0, P.encodeFileOffer(offer));

      const acceptFrame = await this.waitFor(P.FrameType.FILE_ACCEPT, 30000);
      const accept = P.decodeFileAccept(acceptFrame.payload);
      if (accept === null || accept.id !== id) throw new Error('invalid FILE_ACCEPT');
      let offset = accept.offset;
      if (offset > size) throw new Error('peer requested an invalid offset');

      this.onProgress(offset, size);
      while (offset < size) {
        const want = Math.min(256 * 1024, size - offset);
        const n = fs.readSync(file.fd, buffer, { offset: offset, length: want });
        if (n <= 0) throw new Error('short read while sending');
        const chunk = new Uint8Array(buffer, 0, n);
        await this.sendFrame(P.FrameType.FILE_CHUNK, 1, P.encodeFileChunk(id, offset, chunk));
        offset += n;
        this.onProgress(offset, size);
      }

      const done = new P.FileDoneMessage();
      done.id = id;
      done.sha256 = hash;
      await this.sendFrame(P.FrameType.FILE_DONE, 0, P.encodeFileDone(done));

      const resultFrame = await this.waitFor(P.FrameType.FILE_RESULT, 300000);
      const result = P.decodeFileResult(resultFrame.payload);
      if (result === null || result.id !== id) throw new Error('invalid FILE_RESULT');
      if (result.status !== 0) throw new Error(result.message);
      this.onLog('file sent: ' + offer.name);
    } finally {
      fs.closeSync(file.fd);
    }
  }

  private onData(data: ArrayBuffer): void {
    try {
      this.parser.append(new Uint8Array(data));
      while (true) {
        const frame = this.parser.next();
        if (frame === null) break;
        this.handleFrame(frame);
      }
    } catch (error) {
      const message = (error as Error).message;
      this.onLog('protocol error: ' + message);
      this.close();
    }
  }

  private handleFrame(frame: P.Frame): void {
    if (frame.type === P.FrameType.PING) {
      const ping = P.decodePing(frame.payload);
      if (ping !== null) {
        this.sendFrame(P.FrameType.PONG, 0, P.encodePing(ping)).catch((error: Error): void => {
          this.onLog('pong failed: ' + error.message);
        });
      }
      return;
    }
    if (frame.type === P.FrameType.AUTH_FAIL) {
      const reader = new P.ByteReader(frame.payload);
      const code = reader.u8();
      const reason = reader.str(256);
      const message = reason === null ? 'authentication failed' : reason;
      this.authError = message;
      this.rejectWaiter(P.FrameType.AUTH_OK, new Error(message));
      this.onLog('auth failed (' + code + '): ' + message);
      return;
    }
    if (frame.type === P.FrameType.FILE_OFFER) {
      this.handleOffer(frame).catch((error: Error): void => {
        this.onLog('receive failed: ' + error.message);
      });
      return;
    }
    if (frame.type === P.FrameType.FILE_CHUNK) {
      this.handleChunk(frame);
      return;
    }
    if (frame.type === P.FrameType.FILE_DONE) {
      this.handleDone(frame).catch((error: Error): void => {
        this.onLog('finalize failed: ' + error.message);
      });
      return;
    }
    if (frame.type === P.FrameType.FILE_REJECT) {
      const reject = P.decodeFileReject(frame.payload);
      const message = reject === null ? 'transfer rejected' : reject.reason;
      this.fileRejectReason = message;
      this.rejectWaiter(P.FrameType.FILE_ACCEPT, new Error(message));
      return;
    }
    if (frame.type === P.FrameType.CLIPBOARD_SET) {
      const clipboard = P.decodeClipboard(frame.payload);
      if (clipboard !== null) this.onClipboard(clipboard.text);
      return;
    }
    if (frame.type === P.FrameType.ERROR) {
      const reader = new P.ByteReader(frame.payload);
      const code = reader.u16();
      const message = reader.str(512);
      this.onLog('peer error ' + code + ': ' + (message === null ? '' : message));
      return;
    }
    const waiter = this.waiters.get(frame.type);
    if (waiter !== undefined) {
      this.waiters.delete(frame.type);
      clearTimeout(waiter.timer);
      waiter.resolve(frame);
      return;
    }
    // The peer can answer before the next await installs a waiter (for
    // example HELLO -> HELLO and AUTH_RESPONSE -> AUTH_OK). Buffer the early
    // control frame so waitFor() can consume it immediately.
    if (frame.type === P.FrameType.HELLO ||
        frame.type === P.FrameType.AUTH_CHALLENGE ||
        frame.type === P.FrameType.AUTH_OK ||
        frame.type === P.FrameType.FILE_ACCEPT ||
        frame.type === P.FrameType.FILE_RESULT) {
      let queue = this.earlyFrames.get(frame.type);
      if (queue === undefined) {
        queue = [];
        this.earlyFrames.set(frame.type, queue);
      }
      queue.push(frame);
    }
  }

  private async handleOffer(frame: P.Frame): Promise<void> {
    const offer = P.decodeFileOffer(frame.payload);
    if (offer === null) throw new Error('malformed FILE_OFFER');
    if (this.receive !== null) {
      await this.sendFileReject(offer.id, 3, 'another transfer is active');
      return;
    }
    const safeName = P.sanitizeFilename(offer.name);
    if (safeName.length === 0) {
      await this.sendFileReject(offer.id, 1, 'invalid file name');
      return;
    }

    const partPath = this.receivedDir + '/.dhb-' + offer.id + '.part';
    const finalPath = this.uniquePath(this.receivedDir, safeName);
    let offset = 0;
    try {
      const existing = fs.statSync(partPath);
      if (existing.size <= offer.size) offset = existing.size;
      else fs.unlinkSync(partPath);
    } catch (error) {
      offset = 0;
    }

    const state = new ReceiveState();
    state.id = offer.id;
    state.name = safeName;
    state.partPath = partPath;
    state.finalPath = finalPath;
    state.size = offer.size;
    state.offset = offset;
    state.fd = fs.openSync(partPath, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE).fd;
    if (offset === 0) {
      fs.truncateSync(partPath, 0);
    } else {
      const existingFile = fs.openSync(partPath, fs.OpenMode.READ_ONLY);
      try {
        const buffer = new ArrayBuffer(64 * 1024);
        let hashed = 0;
        while (hashed < offset) {
          const n = fs.readSync(existingFile.fd, buffer,
                                { offset: hashed, length: Math.min(64 * 1024, offset - hashed) });
          if (n <= 0) break;
          state.hasher.update(new Uint8Array(buffer, 0, n));
          hashed += n;
        }
      } finally {
        fs.closeSync(existingFile.fd);
      }
    }
    this.receive = state;

    const accept = new P.FileAcceptMessage();
    accept.id = offer.id;
    accept.offset = offset;
    await this.sendFrame(P.FrameType.FILE_ACCEPT, 0, P.encodeFileAccept(accept));
    this.onLog('incoming file: ' + safeName + ' (' + offer.size + ' bytes)');
  }

  private handleChunk(frame: P.Frame): void {
    const chunk = P.decodeFileChunk(frame.payload);
    if (chunk === null || this.receive === null) return;
    const state = this.receive;
    if (chunk.id !== state.id || chunk.offset !== state.offset) return;
    if (chunk.offset + chunk.data.length > state.size) return;
    const buffer = chunk.data.buffer.slice(chunk.data.byteOffset,
                                           chunk.data.byteOffset + chunk.data.length);
    fs.writeSync(state.fd, buffer, { offset: chunk.offset });
    state.hasher.update(chunk.data);
    state.offset += chunk.data.length;
  }

  private async handleDone(frame: P.Frame): Promise<void> {
    const done = P.decodeFileDone(frame.payload);
    if (done === null || this.receive === null) return;
    const state = this.receive;
    if (done.id !== state.id) return;

    fs.closeSync(state.fd);
    const actual = state.hasher.digest();
    let ok = state.offset === state.size && this.bytesEqual(actual, done.sha256);
    let message = ok ? 'ok' : 'size or hash mismatch';
    if (ok) {
      try {
        fs.renameSync(state.partPath, state.finalPath);
      } catch (error) {
        ok = false;
        message = 'cannot finalize: ' + (error as Error).message;
      }
    } else {
      try {
        fs.unlinkSync(state.partPath);
      } catch (error) {
        // Ignore.
      }
    }
    this.receive = null;

    const result = new P.FileResultMessage();
    result.id = state.id;
    result.status = ok ? 0 : 1;
    result.sha256 = actual;
    result.message = message;
    await this.sendFrame(P.FrameType.FILE_RESULT, 0, P.encodeFileResult(result));
    if (ok) this.onFileReceived(state.finalPath);
  }

  private async sendFileReject(id: number, code: number, reason: string): Promise<void> {
    const reject = new P.FileRejectMessage();
    reject.id = id;
    reject.code = code;
    reject.reason = reason;
    await this.sendFrame(P.FrameType.FILE_REJECT, 0, P.encodeFileReject(reject));
  }

  private sendFrame(type: P.FrameType, stream: number, payload: Uint8Array): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      const tcp = this.tcp;
      if (tcp === null || !this.connected) {
        reject(new Error('not connected'));
        return;
      }
      const frame = new P.Frame();
      frame.type = type;
      frame.stream = stream;
      frame.payload = payload;
      const bytes = P.encodeFrame(frame);
      const data = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
      tcp.send({ data: data }).then((): void => {
        resolve();
      }).catch((error: Error): void => {
        reject(error);
      });
    });
  }

  private waitFor(type: P.FrameType, timeoutMs: number): Promise<P.Frame> {
    return new Promise<P.Frame>((resolve, reject) => {
      if (type === P.FrameType.AUTH_OK && this.authError.length > 0) {
        reject(new Error(this.authError));
        return;
      }
      if (type === P.FrameType.FILE_ACCEPT && this.fileRejectReason.length > 0) {
        reject(new Error(this.fileRejectReason));
        return;
      }
      const early = this.earlyFrames.get(type);
      if (early !== undefined && early.length > 0) {
        const frame = early[0];
        early.splice(0, 1);
        resolve(frame);
        return;
      }
      const existing = this.waiters.get(type);
      if (existing !== undefined) {
        clearTimeout(existing.timer);
        existing.reject(new Error('superseded waiter'));
      }
      const timer = setTimeout((): void => {
        this.waiters.delete(type);
        reject(new Error('timeout waiting for frame ' + type));
      }, timeoutMs);
      this.waiters.set(type, new Waiter(type, resolve, reject, timer));
    });
  }

  private rejectWaiter(type: P.FrameType, error: Error): void {
    const waiter = this.waiters.get(type);
    if (waiter !== undefined) {
      this.waiters.delete(type);
      clearTimeout(waiter.timer);
      waiter.reject(error);
    }
  }

  private rejectAll(error: Error): void {
    this.waiters.forEach((waiter: Waiter): void => {
      clearTimeout(waiter.timer);
      waiter.reject(error);
    });
    this.waiters.clear();
  }

  private randomBytes(length: number): Uint8Array {
    try {
      const random = cryptoFramework.createRandom();
      const blob = random.generateRandomSync(length);
      return new Uint8Array(blob.data);
    } catch (error) {
      const out = new Uint8Array(length);
      for (let i = 0; i < length; i++) {
        out[i] = Math.floor(Math.random() * 256);
      }
      return out;
    }
  }

  private uniquePath(dir: string, name: string): string {
    let candidate = dir + '/' + name;
    if (!this.exists(candidate)) return candidate;
    const dot = name.lastIndexOf('.');
    const stem = dot > 0 ? name.substring(0, dot) : name;
    const extension = dot > 0 ? name.substring(dot) : '';
    for (let i = 1; i < 10000; i++) {
      candidate = dir + '/' + stem + ' (' + i + ')' + extension;
      if (!this.exists(candidate)) return candidate;
    }
    return dir + '/' + name + '.' + Date.now();
  }

  private exists(path: string): boolean {
    try {
      fs.accessSync(path);
      return true;
    } catch (error) {
      return false;
    }
  }

  private bytesEqual(a: Uint8Array, b: Uint8Array): boolean {
    if (a.length !== b.length) return false;
    let diff = 0;
    for (let i = 0; i < a.length; i++) diff |= a[i] ^ b[i];
    return diff === 0;
  }
}
