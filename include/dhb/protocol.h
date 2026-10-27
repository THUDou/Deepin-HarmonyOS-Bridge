// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "dhb/bytes.h"

namespace dhb {

// "DHB1" on the wire, little-endian.
inline constexpr std::uint32_t kProtocolMagic = 0x31424844u;
inline constexpr std::uint8_t kProtocolVersion = 1;
inline constexpr std::size_t kFrameHeaderSize = 20;
inline constexpr std::uint32_t kMaxFramePayload = 16u * 1024u * 1024u;

enum class FrameType : std::uint8_t {
  kHello = 0x01,
  kHelloAck = 0x02,
  kAuthChallenge = 0x03,
  kAuthResponse = 0x04,
  kAuthOk = 0x05,
  kAuthFail = 0x06,

  kPing = 0x10,
  kPong = 0x11,

  kFileOffer = 0x20,
  kFileAccept = 0x21,
  kFileReject = 0x22,
  kFileChunk = 0x23,
  kFileDone = 0x24,
  kFileResult = 0x25,
  kFileCancel = 0x26,
  kFileProgress = 0x27,

  kClipboardSet = 0x30,
  kClipboardData = 0x31,

  kInputEvent = 0x40,

  kScreenConfig = 0x50,
  kScreenFrame = 0x51,
  kScreenKeyframeRequest = 0x52,

  kError = 0x60,
  kBye = 0x7f,
};

const char* frameTypeName(FrameType type);

enum FrameFlag : std::uint16_t {
  kFlagNone = 0,
  kFlagAckRequested = 1u << 0,
  kFlagMore = 1u << 1,
};

// HELLO.flags: which soft-bus generation the transport adapter speaks. These
// are metadata for the peer and for diagnostics; the actual transport is
// chosen before the DHB session starts.
enum HelloFlag : std::uint16_t {
  kHelloFlagSoftbus4x = 1u << 0,  // KLink / OpenHarmony 4.x lineage
  kHelloFlagSoftbus5x = 1u << 1,  // OpenHarmony 5.x / openEuler modern
  kHelloFlagSoftbus6x = 1u << 2,  // OpenHarmony/HarmonyOS 6.x lineage
};

enum Capability : std::uint32_t {
  kCapFile = 1u << 0,
  kCapClipboard = 1u << 1,
  kCapScreenSink = 1u << 2,    // can display a remote screen
  kCapScreenSource = 1u << 3,  // can capture and send a screen
  kCapInput = 1u << 4,
  kCapNotify = 1u << 5,
  kCapDiscovery = 1u << 6,
};

struct FrameHeader {
  std::uint8_t version = kProtocolVersion;
  FrameType type = FrameType::kError;
  std::uint16_t flags = 0;
  std::uint32_t stream = 0;
  std::uint32_t length = 0;
  std::uint32_t crc = 0;
};

struct Frame {
  FrameType type = FrameType::kError;
  std::uint16_t flags = 0;
  std::uint32_t stream = 0;
  std::vector<std::uint8_t> payload;
};

// Frames are: magic(4) version(1) type(1) flags(2) stream(4) length(4) crc(4)
// followed by payload. CRC-32C covers the payload only.
std::vector<std::uint8_t> encodeFrame(const Frame& frame);
bool decodeFrameHeader(const std::uint8_t* data, std::size_t size, FrameHeader& header,
                       std::string* err);
bool decodeFrame(const std::uint8_t* data, std::size_t size, Frame& frame,
                 std::size_t* consumed, std::string* err);

struct Hello {
  std::uint16_t proto = kProtocolVersion;
  std::uint16_t flags = 0;
  std::uint32_t caps = 0;
  std::array<std::uint8_t, 16> nonce{};
  std::string device_id;
  std::string device_name;
  std::string os;
  std::string app_version;
};

struct AuthChallenge {
  std::array<std::uint8_t, 16> nonce{};
};

struct AuthResponse {
  std::array<std::uint8_t, 32> proof{};
};

struct AuthFail {
  std::uint8_t code = 0;
  std::string reason;
};

struct FileOffer {
  std::uint32_t id = 0;
  std::uint64_t size = 0;
  std::uint64_t mtime = 0;
  std::uint32_t mode = 0;
  std::array<std::uint8_t, 32> sha256{};
  std::string name;
};

struct FileAccept {
  std::uint32_t id = 0;
  std::uint64_t offset = 0;
};

struct FileReject {
  std::uint32_t id = 0;
  std::uint8_t code = 0;
  std::string reason;
};

struct FileChunk {
  std::uint32_t id = 0;
  std::uint64_t offset = 0;
  ByteView data;
};

struct FileDone {
  std::uint32_t id = 0;
  std::array<std::uint8_t, 32> sha256{};
};

struct FileResult {
  std::uint32_t id = 0;
  std::uint8_t status = 0;  // 0 = ok
  std::array<std::uint8_t, 32> sha256{};
  std::string message;
};

struct FileProgressMsg {
  std::uint32_t id = 0;
  std::uint64_t transferred = 0;
  std::uint64_t total = 0;
};

struct ClipboardSet {
  std::string text;
};

struct PingMsg {
  std::uint64_t timestamp_ms = 0;
};

struct ErrorMsg {
  std::uint16_t code = 0;
  std::string message;
};

std::vector<std::uint8_t> encodeHello(const Hello& msg);
bool decodeHello(ByteView payload, Hello& msg, std::string* err);

std::vector<std::uint8_t> encodeAuthChallenge(const AuthChallenge& msg);
bool decodeAuthChallenge(ByteView payload, AuthChallenge& msg, std::string* err);

std::vector<std::uint8_t> encodeAuthResponse(const AuthResponse& msg);
bool decodeAuthResponse(ByteView payload, AuthResponse& msg, std::string* err);

std::vector<std::uint8_t> encodeAuthFail(const AuthFail& msg);
bool decodeAuthFail(ByteView payload, AuthFail& msg, std::string* err);

std::vector<std::uint8_t> encodeFileOffer(const FileOffer& msg);
bool decodeFileOffer(ByteView payload, FileOffer& msg, std::string* err);

std::vector<std::uint8_t> encodeFileAccept(const FileAccept& msg);
bool decodeFileAccept(ByteView payload, FileAccept& msg, std::string* err);

std::vector<std::uint8_t> encodeFileReject(const FileReject& msg);
bool decodeFileReject(ByteView payload, FileReject& msg, std::string* err);

std::vector<std::uint8_t> encodeFileChunk(const FileChunk& msg);
bool decodeFileChunk(ByteView payload, FileChunk& msg, std::string* err);

std::vector<std::uint8_t> encodeFileDone(const FileDone& msg);
bool decodeFileDone(ByteView payload, FileDone& msg, std::string* err);

std::vector<std::uint8_t> encodeFileResult(const FileResult& msg);
bool decodeFileResult(ByteView payload, FileResult& msg, std::string* err);

std::vector<std::uint8_t> encodeFileProgress(const FileProgressMsg& msg);
bool decodeFileProgress(ByteView payload, FileProgressMsg& msg, std::string* err);

std::vector<std::uint8_t> encodeClipboardSet(const ClipboardSet& msg);
bool decodeClipboardSet(ByteView payload, ClipboardSet& msg, std::string* err);

std::vector<std::uint8_t> encodePing(const PingMsg& msg);
bool decodePing(ByteView payload, PingMsg& msg, std::string* err);

std::vector<std::uint8_t> encodeError(const ErrorMsg& msg);
bool decodeError(ByteView payload, ErrorMsg& msg, std::string* err);

}  // namespace dhb
