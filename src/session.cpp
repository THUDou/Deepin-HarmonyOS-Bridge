// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#include "dhb/session.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

#include "dhb/crypto.h"

namespace dhb {
namespace {

using SteadyClock = std::chrono::steady_clock;

std::uint64_t nowMs() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          SteadyClock::now().time_since_epoch())
          .count());
}

bool fail(std::string* err, std::string message) {
  if (err != nullptr) *err = std::move(message);
  return false;
}

std::vector<std::uint8_t> normalizePsk(const std::string& psk) {
  std::vector<std::uint8_t> bytes;
  if (psk.size() == 64 && crypto::fromHex(psk, bytes) && bytes.size() == 32) {
    return bytes;
  }
  const auto digest = crypto::sha256(ByteView(psk));
  return std::vector<std::uint8_t>(digest.begin(), digest.end());
}

std::vector<std::uint8_t> authTranscript(
    const std::array<std::uint8_t, 16>& client_nonce,
    const std::array<std::uint8_t, 16>& server_nonce,
    const std::array<std::uint8_t, 16>& challenge_nonce, const std::string& client_id,
    const std::string& server_id) {
  static constexpr char kLabel[] = "DHB1-AUTH";
  ByteWriter writer;
  writer.bytes(reinterpret_cast<const std::uint8_t*>(kLabel), sizeof(kLabel) - 1);
  writer.bytes(client_nonce.data(), client_nonce.size());
  writer.bytes(server_nonce.data(), server_nonce.size());
  writer.bytes(challenge_nonce.data(), challenge_nonce.size());
  writer.str(client_id, 256);
  writer.str(server_id, 256);
  return writer.take();
}

std::array<std::uint8_t, 32> deriveSessionKey(
    const std::vector<std::uint8_t>& psk,
    const std::array<std::uint8_t, 16>& client_nonce,
    const std::array<std::uint8_t, 16>& server_nonce) {
  std::vector<std::uint8_t> salt;
  salt.reserve(32);
  salt.insert(salt.end(), client_nonce.begin(), client_nonce.end());
  salt.insert(salt.end(), server_nonce.begin(), server_nonce.end());
  static constexpr char kInfo[] = "DHB1-SESSION";
  return crypto::hkdfSha256(ByteView(psk.data(), psk.size()),
                            ByteView(salt.data(), salt.size()),
                            ByteView(reinterpret_cast<const std::uint8_t*>(kInfo),
                                     sizeof(kInfo) - 1));
}

}  // namespace

struct Session::Receiver {
  std::uint32_t id = 0;
  std::string name;
  std::filesystem::path part;
  std::filesystem::path final_path;
  std::uint64_t size = 0;
  std::uint64_t offset = 0;
  std::ofstream out;
  crypto::Sha256 hasher;
  bool active = false;
};

std::string sanitizeFilename(std::string_view raw) {
  std::string name(raw);
  std::replace(name.begin(), name.end(), '\\', '/');
  const std::size_t slash = name.find_last_of('/');
  if (slash != std::string::npos) name = name.substr(slash + 1);

  std::string out;
  out.reserve(name.size());
  for (unsigned char c : name) {
    if (c < 0x20 || c == 0x7f) continue;
    if (c == '/' || c == '\\') continue;
    out.push_back(static_cast<char>(c));
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
  if (out.empty() || out == "." || out == "..") return {};
  if (out.size() > 180) out.resize(180);
  return out;
}

std::filesystem::path uniqueDestination(const std::filesystem::path& dir,
                                        const std::string& name) {
  std::error_code ec;
  std::filesystem::path candidate = dir / name;
  if (!std::filesystem::exists(candidate, ec)) return candidate;

  const std::string stem = candidate.stem().string();
  const std::string extension = candidate.extension().string();
  for (int i = 1; i < 10000; ++i) {
    candidate = dir / (stem + " (" + std::to_string(i) + ")" + extension);
    if (!std::filesystem::exists(candidate, ec)) return candidate;
  }
  return dir / (name + "." + std::to_string(nowMs()));
}

Session::Session(std::unique_ptr<ITransport> transport, SessionConfig config)
    : transport_(std::move(transport)), config_(std::move(config)) {
  if (config_.recv_dir.empty()) {
    config_.recv_dir = std::filesystem::current_path() / "DHB-received";
  }
  if (config_.local.caps == 0) {
    config_.local.caps = kCapFile | kCapClipboard;
  }
  std::error_code ec;
  std::filesystem::create_directories(config_.recv_dir, ec);
}

Session::~Session() { close(); }

bool Session::sendFrame(FrameType type, std::uint32_t stream, ByteView payload,
                        std::string* err) {
  if (payload.size > kMaxFramePayload) return fail(err, "control payload too large");
  Frame frame;
  frame.type = type;
  frame.stream = stream;
  if (payload.size != 0) {
    frame.payload.assign(payload.data, payload.data + payload.size);
  }
  std::vector<std::uint8_t> bytes = encodeFrame(frame);
  if (bytes.empty()) return fail(err, "cannot encode frame");

  std::lock_guard<std::mutex> lock(write_mutex_);
  if (!transport_ || !transport_->sendAll(ByteView(bytes), err)) {
    running_.store(false);
    return false;
  }
  return true;
}

bool Session::readFrame(Frame& frame, std::string* err) {
  if (!transport_) return fail(err, "transport is closed");

  std::uint8_t header[kFrameHeaderSize];
  if (!transport_->recvExact(header, sizeof(header), err)) return false;

  FrameHeader decoded;
  if (!decodeFrameHeader(header, sizeof(header), decoded, err)) return false;

  Frame incoming;
  incoming.type = decoded.type;
  incoming.flags = decoded.flags;
  incoming.stream = decoded.stream;
  incoming.payload.resize(decoded.length);
  if (decoded.length != 0 &&
      !transport_->recvExact(incoming.payload.data(), decoded.length, err)) {
    return false;
  }
  if (crypto::crc32c(ByteView(incoming.payload)) != decoded.crc) {
    return fail(err, "frame CRC mismatch");
  }
  frame = std::move(incoming);
  return true;
}

bool Session::handshake(std::string* err) {
  if (!transport_) return fail(err, "no transport");
  transport_->setReadTimeout(10000);

  Hello local;
  local.flags = config_.hello_flags;
  local.caps = config_.local.caps;
  local.device_id = config_.local.id;
  local.device_name = config_.local.name;
  local.os = config_.local.os;
  local.app_version = config_.local.app_version;
  crypto::fillRandom(local.nonce.data(), local.nonce.size());

  const std::vector<std::uint8_t> hello_payload = encodeHello(local);
  if (!sendFrame(FrameType::kHello, 0, ByteView(hello_payload), err)) return false;

  Frame frame;
  if (!readFrame(frame, err)) return false;
  if (frame.type != FrameType::kHello) {
    return fail(err, "expected HELLO, got " + std::string(frameTypeName(frame.type)));
  }

  Hello remote;
  if (!decodeHello(ByteView(frame.payload), remote, err)) return false;
  if (remote.proto != kProtocolVersion) {
    return fail(err, "peer uses unsupported protocol version");
  }

  local_nonce_ = local.nonce;
  peer_nonce_ = remote.nonce;
  peer_flags_ = remote.flags;
  peer_.id = remote.device_id;
  peer_.name = remote.device_name;
  peer_.os = remote.os;
  peer_.app_version = remote.app_version;
  peer_.caps = remote.caps;

  const bool has_psk = !config_.psk.empty();
  if (has_psk) {
    const std::vector<std::uint8_t> psk = normalizePsk(config_.psk);
    if (config_.is_client) {
      if (!readFrame(frame, err)) return false;
      if (frame.type == FrameType::kAuthOk) {
        return fail(err, "server skipped authentication; refusing downgrade");
      }
      if (frame.type != FrameType::kAuthChallenge) {
        return fail(err, "expected AUTH_CHALLENGE");
      }
      AuthChallenge challenge;
      if (!decodeAuthChallenge(ByteView(frame.payload), challenge, err)) return false;

      const std::vector<std::uint8_t> transcript = authTranscript(
          local_nonce_, peer_nonce_, challenge.nonce, config_.local.id, peer_.id);
      AuthResponse response;
      response.proof = crypto::hmacSha256(ByteView(psk), ByteView(transcript));
      const std::vector<std::uint8_t> proof_payload = encodeAuthResponse(response);
      if (!sendFrame(FrameType::kAuthResponse, 0, ByteView(proof_payload), err)) {
        return false;
      }
      if (!readFrame(frame, err)) return false;
      if (frame.type == FrameType::kAuthFail) {
        AuthFail failure;
        if (!decodeAuthFail(ByteView(frame.payload), failure, err)) return false;
        return fail(err, "authentication failed: " + failure.reason);
      }
      if (frame.type != FrameType::kAuthOk) return fail(err, "expected AUTH_OK");
      session_key_ = deriveSessionKey(psk, local_nonce_, peer_nonce_);
    } else {
      AuthChallenge challenge;
      crypto::fillRandom(challenge.nonce.data(), challenge.nonce.size());
      const std::vector<std::uint8_t> challenge_payload = encodeAuthChallenge(challenge);
      if (!sendFrame(FrameType::kAuthChallenge, 0, ByteView(challenge_payload), err)) {
        return false;
      }
      if (!readFrame(frame, err)) return false;
      if (frame.type != FrameType::kAuthResponse) return fail(err, "expected AUTH_RESPONSE");

      AuthResponse response;
      if (!decodeAuthResponse(ByteView(frame.payload), response, err)) return false;
      const std::vector<std::uint8_t> transcript = authTranscript(
          peer_nonce_, local_nonce_, challenge.nonce, peer_.id, config_.local.id);
      const auto expected = crypto::hmacSha256(ByteView(psk), ByteView(transcript));
      if (!crypto::constantTimeEqual(ByteView(expected.data(), expected.size()),
                                     ByteView(response.proof.data(), response.proof.size()))) {
        AuthFail failure;
        failure.code = 1;
        failure.reason = "pairing key mismatch";
        const std::vector<std::uint8_t> failure_payload = encodeAuthFail(failure);
        sendFrame(FrameType::kAuthFail, 0, ByteView(failure_payload), nullptr);
        return fail(err, "pairing key mismatch");
      }
      if (!sendFrame(FrameType::kAuthOk, 0, ByteView{}, err)) return false;
      session_key_ = deriveSessionKey(psk, peer_nonce_, local_nonce_);
    }
  } else {
    if (!config_.insecure) return fail(err, "PSK required (or pass --insecure)");
    if (config_.is_client) {
      if (!readFrame(frame, err)) return false;
      if (frame.type != FrameType::kAuthOk) {
        return fail(err, "server requires authentication");
      }
    } else {
      if (!sendFrame(FrameType::kAuthOk, 0, ByteView{}, err)) return false;
    }
  }

  handshaked_.store(true);
  transport_->setReadTimeout(0);
  return true;
}

bool Session::start(std::string* err) {
  if (!handshaked_.load()) return fail(err, "handshake has not completed");
  if (running_.load()) return true;
  running_.store(true);
  reader_ = std::thread(&Session::readerLoop, this);
  return true;
}

void Session::readerLoop() {
  std::string err;
  while (running_.load()) {
    Frame frame;
    if (!readFrame(frame, &err)) {
      if (running_.load()) log("connection closed: " + err);
      break;
    }
    std::string handler_error;
    if (!handleFrame(frame, &handler_error)) {
      log("protocol error: " + handler_error);
      break;
    }
  }
  running_.store(false);
  sender_cv_.notify_all();
  ping_cv_.notify_all();
}

bool Session::handleFrame(const Frame& frame, std::string* err) {
  switch (frame.type) {
    case FrameType::kPing: {
      PingMsg ping;
      if (!decodePing(ByteView(frame.payload), ping, err)) return false;
      const std::vector<std::uint8_t> pong = encodePing(ping);
      return sendFrame(FrameType::kPong, 0, ByteView(pong), err);
    }
    case FrameType::kPong: {
      {
        std::lock_guard<std::mutex> lock(ping_mutex_);
        pong_received_ = true;
      }
      ping_cv_.notify_all();
      return true;
    }
    case FrameType::kClipboardSet: {
      ClipboardSet clipboard;
      if (!decodeClipboardSet(ByteView(frame.payload), clipboard, err)) return false;
      log("clipboard update received (" + std::to_string(clipboard.text.size()) + " bytes)");
      if (on_clipboard_) on_clipboard_(clipboard.text);
      return true;
    }
    case FrameType::kFileOffer:
      return handleFileOffer(frame, err);
    case FrameType::kFileChunk:
      return handleFileChunk(frame, err);
    case FrameType::kFileDone:
      return handleFileDone(frame, err);
    case FrameType::kFileAccept:
      return handleFileAccept(frame, err);
    case FrameType::kFileReject:
      return handleFileReject(frame, err);
    case FrameType::kFileResult:
      return handleFileResult(frame, err);
    case FrameType::kFileCancel:
      return handleFileCancel(frame, err);
    case FrameType::kFileProgress:
      return true;
    case FrameType::kBye:
      running_.store(false);
      return true;
    case FrameType::kError: {
      ErrorMsg error;
      if (!decodeError(ByteView(frame.payload), error, err)) return false;
      log("peer error " + std::to_string(error.code) + ": " + error.message);
      return true;
    }
    case FrameType::kHello:
    case FrameType::kHelloAck:
      return true;
    default:
      log("unhandled frame type: " + std::string(frameTypeName(frame.type)));
      return true;
  }
}

bool Session::handleFileOffer(const Frame& frame, std::string* err) {
  FileOffer offer;
  if (!decodeFileOffer(ByteView(frame.payload), offer, err)) return false;

  auto reject = [&](std::uint8_t code, const std::string& reason) {
    FileReject message;
    message.id = offer.id;
    message.code = code;
    message.reason = reason;
    const std::vector<std::uint8_t> payload = encodeFileReject(message);
    sendFrame(FrameType::kFileReject, 0, ByteView(payload), nullptr);
    return true;
  };

  const std::string safe_name = sanitizeFilename(offer.name);
  if (safe_name.empty()) return reject(1, "invalid file name");

  std::error_code ec;
  std::filesystem::create_directories(config_.recv_dir, ec);
  if (ec) return reject(2, "cannot create receive directory");

  std::lock_guard<std::mutex> lock(receiver_mutex_);
  if (receiver_ && receiver_->active) return reject(3, "another transfer is active");

  const std::filesystem::path part =
      config_.recv_dir / (".dhb-" + std::to_string(offer.id) + ".part");
  std::uint64_t offset = 0;
  if (std::filesystem::exists(part, ec)) {
    const auto existing_size = std::filesystem::file_size(part, ec);
    if (!ec && existing_size <= offer.size) {
      offset = existing_size;
    } else {
      std::filesystem::remove(part, ec);
    }
  }

  auto receiver = std::make_unique<Receiver>();
  receiver->id = offer.id;
  receiver->name = safe_name;
  receiver->part = part;
  receiver->final_path = uniqueDestination(config_.recv_dir, safe_name);
  receiver->size = offer.size;
  receiver->offset = offset;
  receiver->active = true;

  std::ios::openmode mode = std::ios::binary;
  mode |= (offset == 0) ? std::ios::trunc : std::ios::app;
  receiver->out.open(part.string(), mode);
  if (!receiver->out) {
    std::filesystem::remove(part, ec);
    return reject(4, "cannot open destination file");
  }

  if (offset != 0) {
    std::ifstream existing(part.string(), std::ios::binary);
    std::vector<char> buffer(64 * 1024);
    while (existing) {
      existing.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const std::streamsize n = existing.gcount();
      if (n > 0) {
        receiver->hasher.update(ByteView(
            reinterpret_cast<const std::uint8_t*>(buffer.data()),
            static_cast<std::size_t>(n)));
      }
    }
  }

  receiver_ = std::move(receiver);

  FileAccept accept;
  accept.id = offer.id;
  accept.offset = offset;
  const std::vector<std::uint8_t> payload = encodeFileAccept(accept);
  if (!sendFrame(FrameType::kFileAccept, 0, ByteView(payload), err)) {
    receiver_.reset();
    return false;
  }

  log("incoming file: " + safe_name + " (" + std::to_string(offer.size) + " bytes, resume at " +
      std::to_string(offset) + ")");
  return true;
}

bool Session::handleFileChunk(const Frame& frame, std::string* err) {
  FileChunk chunk;
  if (!decodeFileChunk(ByteView(frame.payload), chunk, err)) return false;

  std::lock_guard<std::mutex> lock(receiver_mutex_);
  if (!receiver_ || receiver_->id != chunk.id) return true;
  if (chunk.offset != receiver_->offset) {
    log("dropping out-of-order file chunk");
    return true;
  }
  if (chunk.data.size > receiver_->size - receiver_->offset) {
    log("dropping oversized file chunk");
    return true;
  }

  std::vector<std::uint8_t> data(chunk.data.data, chunk.data.data + chunk.data.size);
  receiver_->out.write(reinterpret_cast<const char*>(data.data()),
                       static_cast<std::streamsize>(data.size()));
  if (!receiver_->out) {
    log("failed to write incoming file");
    receiver_.reset();
    return true;
  }
  receiver_->hasher.update(ByteView(data));
  receiver_->offset += data.size();
  return true;
}

bool Session::handleFileDone(const Frame& frame, std::string* err) {
  FileDone done;
  if (!decodeFileDone(ByteView(frame.payload), done, err)) return false;

  std::unique_lock<std::mutex> lock(receiver_mutex_);
  if (!receiver_ || receiver_->id != done.id) {
    lock.unlock();
    FileResult result;
    result.id = done.id;
    result.status = 1;
    result.message = "unknown transfer";
    const std::vector<std::uint8_t> payload = encodeFileResult(result);
    return sendFrame(FrameType::kFileResult, 0, ByteView(payload), nullptr);
  }

  receiver_->out.flush();
  receiver_->out.close();
  const auto actual_hash = receiver_->hasher.finish();
  bool ok = receiver_->offset == receiver_->size &&
            crypto::constantTimeEqual(ByteView(actual_hash.data(), actual_hash.size()),
                                      ByteView(done.sha256.data(), done.sha256.size()));
  std::string message = ok ? "ok" : "size or hash mismatch";
  std::filesystem::path final_path;

  if (ok) {
    std::error_code ec;
    std::filesystem::rename(receiver_->part, receiver_->final_path, ec);
    if (ec) {
      std::filesystem::copy_file(receiver_->part, receiver_->final_path,
                                 std::filesystem::copy_options::overwrite_existing, ec);
      if (!ec) std::filesystem::remove(receiver_->part, ec);
    }
    if (ec) {
      ok = false;
      message = "cannot finalize file: " + ec.message();
    } else {
      final_path = receiver_->final_path;
    }
  }

  if (!ok) {
    std::error_code ec;
    std::filesystem::remove(receiver_->part, ec);
  }

  const std::uint32_t id = receiver_->id;
  receiver_.reset();
  lock.unlock();

  FileResult result;
  result.id = id;
  result.status = static_cast<std::uint8_t>(ok ? 0 : 1);
  result.sha256 = actual_hash;
  result.message = message;
  const std::vector<std::uint8_t> payload = encodeFileResult(result);
  if (!sendFrame(FrameType::kFileResult, 0, ByteView(payload), err)) return false;

  if (ok && on_file_received_) on_file_received_(final_path);
  return true;
}

bool Session::handleFileAccept(const Frame& frame, std::string* err) {
  FileAccept accept;
  if (!decodeFileAccept(ByteView(frame.payload), accept, err)) return false;
  {
    std::lock_guard<std::mutex> lock(sender_mutex_);
    if (sender_.id == accept.id && !sender_.finished) {
      sender_.accepted = true;
      sender_.offset = accept.offset;
    }
  }
  sender_cv_.notify_all();
  return true;
}

bool Session::handleFileReject(const Frame& frame, std::string* err) {
  FileReject reject;
  if (!decodeFileReject(ByteView(frame.payload), reject, err)) return false;
  {
    std::lock_guard<std::mutex> lock(sender_mutex_);
    if (sender_.id == reject.id) {
      sender_.finished = true;
      sender_.ok = false;
      sender_.message = reject.reason;
    }
  }
  sender_cv_.notify_all();
  return true;
}

bool Session::handleFileResult(const Frame& frame, std::string* err) {
  FileResult result;
  if (!decodeFileResult(ByteView(frame.payload), result, err)) return false;
  {
    std::lock_guard<std::mutex> lock(sender_mutex_);
    if (sender_.id == result.id) {
      sender_.finished = true;
      sender_.ok = result.status == 0;
      sender_.message = result.message;
      sender_.hash = result.sha256;
    }
  }
  sender_cv_.notify_all();
  return true;
}

bool Session::handleFileCancel(const Frame& frame, std::string* err) {
  (void)frame;
  (void)err;
  return true;
}

bool Session::sendFile(const std::filesystem::path& path, const ProgressFn& progress,
                       std::string* err) {
  if (!running_.load()) return fail(err, "session is not started");
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    return fail(err, "not a regular file: " + path.string());
  }
  const std::uint64_t size = std::filesystem::file_size(path, ec);
  if (ec) return fail(err, "cannot stat file: " + ec.message());

  std::ifstream input(path.string(), std::ios::binary);
  if (!input) return fail(err, "cannot open file: " + path.string());

  crypto::Sha256 hasher;
  std::vector<char> hash_buffer(1u << 20);
  while (true) {
    input.read(hash_buffer.data(), static_cast<std::streamsize>(hash_buffer.size()));
    const std::streamsize n = input.gcount();
    if (n > 0) {
      hasher.update(ByteView(reinterpret_cast<const std::uint8_t*>(hash_buffer.data()),
                             static_cast<std::size_t>(n)));
    }
    if (!input) break;
  }
  if (!input.eof()) return fail(err, "read error while hashing file");
  const auto hash = hasher.finish();

  std::lock_guard<std::mutex> send_lock(send_file_mutex_);
  const std::uint32_t id = next_transfer_id_.fetch_add(1);
  {
    std::lock_guard<std::mutex> lock(sender_mutex_);
    sender_ = SenderState{};
    sender_.id = id;
  }

  FileOffer offer;
  offer.id = id;
  offer.size = size;
  offer.mtime = static_cast<std::uint64_t>(std::time(nullptr));
  offer.mode = 0644;
  offer.sha256 = hash;
  offer.name = path.filename().string();
  const std::vector<std::uint8_t> offer_payload = encodeFileOffer(offer);
  if (!sendFrame(FrameType::kFileOffer, 0, ByteView(offer_payload), err)) return false;

  std::uint64_t offset = 0;
  {
    std::unique_lock<std::mutex> lock(sender_mutex_);
    if (!sender_cv_.wait_for(lock, std::chrono::seconds(30),
                             [&] { return sender_.accepted || sender_.finished; })) {
      return fail(err, "timed out waiting for FILE_ACCEPT");
    }
    if (!sender_.accepted) {
      return fail(err, sender_.message.empty() ? "transfer rejected" : sender_.message);
    }
    offset = sender_.offset;
  }
  if (offset > size) return fail(err, "peer requested an invalid resume offset");

  input.clear();
  input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!input) return fail(err, "cannot seek to resume offset");

  std::vector<char> chunk(256 * 1024);
  std::uint64_t sent = offset;
  if (progress) progress(FileProgress{path.filename().string(), sent, size});
  while (sent < size) {
    input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const std::streamsize n = input.gcount();
    if (n <= 0) return fail(err, "short read while sending file");

    ByteWriter writer;
    writer.u32(id);
    writer.u64(sent);
    writer.bytes(reinterpret_cast<const std::uint8_t*>(chunk.data()),
                 static_cast<std::size_t>(n));
    if (!sendFrame(FrameType::kFileChunk, 1, ByteView(writer.data()), err)) return false;
    sent += static_cast<std::uint64_t>(n);
    if (progress) progress(FileProgress{path.filename().string(), sent, size});
  }

  FileDone done;
  done.id = id;
  done.sha256 = hash;
  const std::vector<std::uint8_t> done_payload = encodeFileDone(done);
  if (!sendFrame(FrameType::kFileDone, 0, ByteView(done_payload), err)) return false;

  {
    std::unique_lock<std::mutex> lock(sender_mutex_);
    if (!sender_cv_.wait_for(lock, std::chrono::minutes(5),
                             [&] { return sender_.finished; })) {
      return fail(err, "timed out waiting for FILE_RESULT");
    }
    if (!sender_.ok) {
      return fail(err, sender_.message.empty() ? "transfer failed" : sender_.message);
    }
    if (!crypto::constantTimeEqual(ByteView(sender_.hash.data(), sender_.hash.size()),
                                   ByteView(hash.data(), hash.size()))) {
      return fail(err, "peer reported a different file hash");
    }
  }
  return true;
}

bool Session::sendClipboard(std::string_view text, std::string* err) {
  if (!running_.load()) return fail(err, "session is not started");
  if (text.size() > 60000) return fail(err, "clipboard text exceeds 60000 bytes");
  ClipboardSet message;
  message.text = std::string(text);
  const std::vector<std::uint8_t> payload = encodeClipboardSet(message);
  return sendFrame(FrameType::kClipboardSet, 0, ByteView(payload), err);
}

bool Session::ping(std::uint64_t* rtt_ms, std::string* err) {
  if (!running_.load()) return fail(err, "session is not started");
  {
    std::lock_guard<std::mutex> lock(ping_mutex_);
    pong_received_ = false;
  }
  PingMsg message;
  message.timestamp_ms = nowMs();
  const std::vector<std::uint8_t> payload = encodePing(message);
  if (!sendFrame(FrameType::kPing, 0, ByteView(payload), err)) return false;

  std::unique_lock<std::mutex> lock(ping_mutex_);
  if (!ping_cv_.wait_for(lock, std::chrono::seconds(3), [&] { return pong_received_; })) {
    return fail(err, "ping timed out");
  }
  lock.unlock();
  if (rtt_ms != nullptr) *rtt_ms = nowMs() - message.timestamp_ms;
  return true;
}

void Session::close() {
  if (closed_.exchange(true)) return;
  running_.store(false);
  if (transport_) transport_->close();
  sender_cv_.notify_all();
  ping_cv_.notify_all();
  if (reader_.joinable() && reader_.get_id() != std::this_thread::get_id()) {
    reader_.join();
  }
}

void Session::log(const std::string& message) {
  if (on_log_) on_log_(message);
}

}  // namespace dhb
