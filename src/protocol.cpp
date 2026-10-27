// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#include "dhb/protocol.h"

#include <utility>

#include "dhb/crypto.h"

namespace dhb {
namespace {

bool fail(std::string* err, std::string message) {
  if (err != nullptr) *err = std::move(message);
  return false;
}

bool requireDone(ByteReader& reader, std::string* err) {
  if (!reader.done()) return fail(err, "trailing bytes in control message");
  return true;
}

}  // namespace

const char* frameTypeName(FrameType type) {
  switch (type) {
    case FrameType::kHello:
      return "HELLO";
    case FrameType::kHelloAck:
      return "HELLO_ACK";
    case FrameType::kAuthChallenge:
      return "AUTH_CHALLENGE";
    case FrameType::kAuthResponse:
      return "AUTH_RESPONSE";
    case FrameType::kAuthOk:
      return "AUTH_OK";
    case FrameType::kAuthFail:
      return "AUTH_FAIL";
    case FrameType::kPing:
      return "PING";
    case FrameType::kPong:
      return "PONG";
    case FrameType::kFileOffer:
      return "FILE_OFFER";
    case FrameType::kFileAccept:
      return "FILE_ACCEPT";
    case FrameType::kFileReject:
      return "FILE_REJECT";
    case FrameType::kFileChunk:
      return "FILE_CHUNK";
    case FrameType::kFileDone:
      return "FILE_DONE";
    case FrameType::kFileResult:
      return "FILE_RESULT";
    case FrameType::kFileCancel:
      return "FILE_CANCEL";
    case FrameType::kFileProgress:
      return "FILE_PROGRESS";
    case FrameType::kClipboardSet:
      return "CLIPBOARD_SET";
    case FrameType::kClipboardData:
      return "CLIPBOARD_DATA";
    case FrameType::kInputEvent:
      return "INPUT_EVENT";
    case FrameType::kScreenConfig:
      return "SCREEN_CONFIG";
    case FrameType::kScreenFrame:
      return "SCREEN_FRAME";
    case FrameType::kScreenKeyframeRequest:
      return "SCREEN_KEYFRAME_REQUEST";
    case FrameType::kError:
      return "ERROR";
    case FrameType::kBye:
      return "BYE";
  }
  return "UNKNOWN";
}

std::vector<std::uint8_t> encodeFrame(const Frame& frame) {
  if (frame.payload.size() > kMaxFramePayload) return {};

  ByteWriter writer;
  writer.u32(kProtocolMagic);
  writer.u8(kProtocolVersion);
  writer.u8(static_cast<std::uint8_t>(frame.type));
  writer.u16(frame.flags);
  writer.u32(frame.stream);
  writer.u32(static_cast<std::uint32_t>(frame.payload.size()));
  writer.u32(0);  // CRC placeholder, patched below.

  std::vector<std::uint8_t> out = writer.take();
  out.insert(out.end(), frame.payload.begin(), frame.payload.end());

  const std::uint32_t crc = crypto::crc32c(ByteView(frame.payload));
  out[16] = static_cast<std::uint8_t>(crc & 0xffu);
  out[17] = static_cast<std::uint8_t>((crc >> 8) & 0xffu);
  out[18] = static_cast<std::uint8_t>((crc >> 16) & 0xffu);
  out[19] = static_cast<std::uint8_t>((crc >> 24) & 0xffu);
  return out;
}

bool decodeFrameHeader(const std::uint8_t* data, std::size_t size, FrameHeader& header,
                       std::string* err) {
  if (data == nullptr || size < kFrameHeaderSize) {
    return fail(err, "frame header truncated");
  }

  ByteReader reader(ByteView(data, size));
  std::uint32_t magic = 0;
  std::uint8_t version = 0;
  std::uint8_t type = 0;
  std::uint16_t flags = 0;
  std::uint32_t stream = 0;
  std::uint32_t length = 0;
  std::uint32_t crc = 0;
  if (!reader.u32(magic) || !reader.u8(version) || !reader.u8(type) || !reader.u16(flags) ||
      !reader.u32(stream) || !reader.u32(length) || !reader.u32(crc)) {
    return fail(err, "malformed frame header");
  }
  if (magic != kProtocolMagic) return fail(err, "bad frame magic");
  if (version != kProtocolVersion) return fail(err, "unsupported protocol version");
  if (length > kMaxFramePayload) return fail(err, "frame payload too large");

  header.version = version;
  header.type = static_cast<FrameType>(type);
  header.flags = flags;
  header.stream = stream;
  header.length = length;
  header.crc = crc;
  return true;
}

bool decodeFrame(const std::uint8_t* data, std::size_t size, Frame& frame,
                 std::size_t* consumed, std::string* err) {
  FrameHeader header;
  if (!decodeFrameHeader(data, size, header, err)) return false;
  const std::size_t total = kFrameHeaderSize + static_cast<std::size_t>(header.length);
  if (size < total) return fail(err, "frame payload truncated");

  ByteView payload(data + kFrameHeaderSize, header.length);
  if (crypto::crc32c(payload) != header.crc) return fail(err, "frame CRC mismatch");

  frame.type = header.type;
  frame.flags = header.flags;
  frame.stream = header.stream;
  frame.payload.assign(payload.data, payload.data + payload.size);
  if (consumed != nullptr) *consumed = total;
  return true;
}

std::vector<std::uint8_t> encodeHello(const Hello& msg) {
  ByteWriter w;
  w.u16(msg.proto);
  w.u16(msg.flags);
  w.u32(msg.caps);
  w.bytes(msg.nonce.data(), msg.nonce.size());
  w.str(msg.device_id, 256);
  w.str(msg.device_name, 256);
  w.str(msg.os, 128);
  w.str(msg.app_version, 128);
  return w.take();
}

bool decodeHello(ByteView payload, Hello& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u16(msg.proto) || !r.u16(msg.flags) || !r.u32(msg.caps) ||
      !r.bytes(msg.nonce.data(), msg.nonce.size()) || !r.str(msg.device_id, 256) ||
      !r.str(msg.device_name, 256) || !r.str(msg.os, 128) ||
      !r.str(msg.app_version, 128)) {
    return fail(err, "malformed HELLO");
  }
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeAuthChallenge(const AuthChallenge& msg) {
  ByteWriter w;
  w.bytes(msg.nonce.data(), msg.nonce.size());
  return w.take();
}

bool decodeAuthChallenge(ByteView payload, AuthChallenge& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.bytes(msg.nonce.data(), msg.nonce.size())) {
    return fail(err, "malformed AUTH_CHALLENGE");
  }
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeAuthResponse(const AuthResponse& msg) {
  ByteWriter w;
  w.bytes(msg.proof.data(), msg.proof.size());
  return w.take();
}

bool decodeAuthResponse(ByteView payload, AuthResponse& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.bytes(msg.proof.data(), msg.proof.size())) {
    return fail(err, "malformed AUTH_RESPONSE");
  }
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeAuthFail(const AuthFail& msg) {
  ByteWriter w;
  w.u8(msg.code);
  w.str(msg.reason, 256);
  return w.take();
}

bool decodeAuthFail(ByteView payload, AuthFail& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u8(msg.code) || !r.str(msg.reason, 256)) {
    return fail(err, "malformed AUTH_FAIL");
  }
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeFileOffer(const FileOffer& msg) {
  ByteWriter w;
  w.u32(msg.id);
  w.u64(msg.size);
  w.u64(msg.mtime);
  w.u32(msg.mode);
  w.bytes(msg.sha256.data(), msg.sha256.size());
  w.str(msg.name, 1024);
  return w.take();
}

bool decodeFileOffer(ByteView payload, FileOffer& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u32(msg.id) || !r.u64(msg.size) || !r.u64(msg.mtime) || !r.u32(msg.mode) ||
      !r.bytes(msg.sha256.data(), msg.sha256.size()) || !r.str(msg.name, 1024)) {
    return fail(err, "malformed FILE_OFFER");
  }
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeFileAccept(const FileAccept& msg) {
  ByteWriter w;
  w.u32(msg.id);
  w.u64(msg.offset);
  return w.take();
}

bool decodeFileAccept(ByteView payload, FileAccept& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u32(msg.id) || !r.u64(msg.offset)) return fail(err, "malformed FILE_ACCEPT");
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeFileReject(const FileReject& msg) {
  ByteWriter w;
  w.u32(msg.id);
  w.u8(msg.code);
  w.str(msg.reason, 512);
  return w.take();
}

bool decodeFileReject(ByteView payload, FileReject& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u32(msg.id) || !r.u8(msg.code) || !r.str(msg.reason, 512)) {
    return fail(err, "malformed FILE_REJECT");
  }
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeFileChunk(const FileChunk& msg) {
  ByteWriter w;
  w.u32(msg.id);
  w.u64(msg.offset);
  w.bytes(msg.data);
  return w.take();
}

bool decodeFileChunk(ByteView payload, FileChunk& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u32(msg.id) || !r.u64(msg.offset)) return fail(err, "malformed FILE_CHUNK");
  msg.data = r.rest();
  if (msg.data.empty()) return fail(err, "empty FILE_CHUNK");
  return true;
}

std::vector<std::uint8_t> encodeFileDone(const FileDone& msg) {
  ByteWriter w;
  w.u32(msg.id);
  w.bytes(msg.sha256.data(), msg.sha256.size());
  return w.take();
}

bool decodeFileDone(ByteView payload, FileDone& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u32(msg.id) || !r.bytes(msg.sha256.data(), msg.sha256.size())) {
    return fail(err, "malformed FILE_DONE");
  }
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeFileResult(const FileResult& msg) {
  ByteWriter w;
  w.u32(msg.id);
  w.u8(msg.status);
  w.bytes(msg.sha256.data(), msg.sha256.size());
  w.str(msg.message, 512);
  return w.take();
}

bool decodeFileResult(ByteView payload, FileResult& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u32(msg.id) || !r.u8(msg.status) ||
      !r.bytes(msg.sha256.data(), msg.sha256.size()) || !r.str(msg.message, 512)) {
    return fail(err, "malformed FILE_RESULT");
  }
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeFileProgress(const FileProgressMsg& msg) {
  ByteWriter w;
  w.u32(msg.id);
  w.u64(msg.transferred);
  w.u64(msg.total);
  return w.take();
}

bool decodeFileProgress(ByteView payload, FileProgressMsg& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u32(msg.id) || !r.u64(msg.transferred) || !r.u64(msg.total)) {
    return fail(err, "malformed FILE_PROGRESS");
  }
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeClipboardSet(const ClipboardSet& msg) {
  ByteWriter w;
  w.str(msg.text, 60000);
  return w.take();
}

bool decodeClipboardSet(ByteView payload, ClipboardSet& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.str(msg.text, 60000)) return fail(err, "malformed CLIPBOARD_SET");
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodePing(const PingMsg& msg) {
  ByteWriter w;
  w.u64(msg.timestamp_ms);
  return w.take();
}

bool decodePing(ByteView payload, PingMsg& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u64(msg.timestamp_ms)) return fail(err, "malformed PING");
  return requireDone(r, err);
}

std::vector<std::uint8_t> encodeError(const ErrorMsg& msg) {
  ByteWriter w;
  w.u16(msg.code);
  w.str(msg.message, 512);
  return w.take();
}

bool decodeError(ByteView payload, ErrorMsg& msg, std::string* err) {
  ByteReader r(payload);
  if (!r.u16(msg.code) || !r.str(msg.message, 512)) return fail(err, "malformed ERROR");
  return requireDone(r, err);
}

}  // namespace dhb
