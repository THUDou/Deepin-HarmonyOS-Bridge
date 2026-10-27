// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include "dhb/protocol.h"
#include "dhb/transport.h"

namespace dhb {

struct DeviceInfo {
  std::string id;
  std::string name;
  std::string os;
  std::string app_version;
  std::uint32_t caps = 0;
};

struct SessionConfig {
  DeviceInfo local;
  // 64 hex chars (32 bytes) or a passphrase. Empty is only valid together with
  // insecure = true, which is intended for loopback development.
  std::string psk;
  bool insecure = false;
  std::filesystem::path recv_dir;
  bool is_client = true;
  // Advertised in HELLO.flags; set by the soft-bus transport adapter.
  std::uint16_t hello_flags = 0;
};

struct FileProgress {
  std::string name;
  std::uint64_t transferred = 0;
  std::uint64_t total = 0;
};

using ProgressFn = std::function<void(const FileProgress&)>;

// One authenticated, multiplexed DHB session over any ITransport.
//
// Lifetime: handshake() -> start() -> sendFile()/sendClipboard() -> close().
// The reader thread is started by start(); it owns all inbound frame handling.
class Session {
 public:
  Session(std::unique_ptr<ITransport> transport, SessionConfig config);
  ~Session();

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  // Performs the HELLO + PIN/PSK authentication handshake synchronously.
  bool handshake(std::string* err);
  // Starts the reader thread. Must be called after a successful handshake.
  bool start(std::string* err);

  bool sendFile(const std::filesystem::path& path, const ProgressFn& progress,
                std::string* err);
  bool sendClipboard(std::string_view text, std::string* err);
  bool ping(std::uint64_t* rtt_ms, std::string* err);

  void close();
  bool connected() const { return running_.load(); }
  const DeviceInfo& peer() const { return peer_; }
  const DeviceInfo& local() const { return config_.local; }
  std::uint16_t peerFlags() const { return peer_flags_; }

  void setFileReceivedHandler(std::function<void(const std::filesystem::path&)> handler) {
    on_file_received_ = std::move(handler);
  }
  void setClipboardHandler(std::function<void(std::string)> handler) {
    on_clipboard_ = std::move(handler);
  }
  void setLogHandler(std::function<void(std::string)> handler) {
    on_log_ = std::move(handler);
  }

 private:
  struct Receiver;
  struct SenderState {
    std::uint32_t id = 0;
    bool accepted = false;
    std::uint64_t offset = 0;
    bool finished = false;
    bool ok = false;
    std::string message;
    std::array<std::uint8_t, 32> hash{};
  };

  bool sendFrame(FrameType type, std::uint32_t stream, ByteView payload, std::string* err);
  bool readFrame(Frame& frame, std::string* err);
  void readerLoop();
  bool handleFrame(const Frame& frame, std::string* err);
  bool handleFileOffer(const Frame& frame, std::string* err);
  bool handleFileChunk(const Frame& frame, std::string* err);
  bool handleFileDone(const Frame& frame, std::string* err);
  bool handleFileAccept(const Frame& frame, std::string* err);
  bool handleFileReject(const Frame& frame, std::string* err);
  bool handleFileResult(const Frame& frame, std::string* err);
  bool handleFileCancel(const Frame& frame, std::string* err);
  void log(const std::string& message);

  std::unique_ptr<ITransport> transport_;
  SessionConfig config_;
  DeviceInfo peer_;
  std::array<std::uint8_t, 16> local_nonce_{};
  std::array<std::uint8_t, 16> peer_nonce_{};
  std::array<std::uint8_t, 32> session_key_{};
  std::uint16_t peer_flags_ = 0;

  std::atomic<bool> handshaked_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> closed_{false};
  std::thread reader_;

  std::mutex write_mutex_;
  std::mutex sender_mutex_;
  std::condition_variable sender_cv_;
  SenderState sender_;
  std::mutex send_file_mutex_;
  std::atomic<std::uint32_t> next_transfer_id_{1};

  std::unique_ptr<Receiver> receiver_;
  std::mutex receiver_mutex_;

  std::mutex ping_mutex_;
  std::condition_variable ping_cv_;
  bool pong_received_ = false;

  std::function<void(const std::filesystem::path&)> on_file_received_;
  std::function<void(std::string)> on_clipboard_;
  std::function<void(std::string)> on_log_;
};

// Exposed for tests and for the CLI's safe-destination logic.
std::string sanitizeFilename(std::string_view raw);
std::filesystem::path uniqueDestination(const std::filesystem::path& dir,
                                        const std::string& name);

}  // namespace dhb
