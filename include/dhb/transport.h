// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "dhb/bytes.h"

namespace dhb {

// Transport abstraction. The same session/protocol code must run over:
//   - TCP/TLS (stock HarmonyOS app, development hosts)
//   - Klink / dsoftbus byte streams (OpenHarmony system tier, Deepin)
//   - an in-memory pair (tests)
class ITransport {
 public:
  virtual ~ITransport() = default;
  virtual bool sendAll(ByteView data, std::string* err) = 0;
  virtual bool recvExact(std::uint8_t* data, std::size_t size, std::string* err) = 0;
  // 0 means blocking; a positive value is a receive/send timeout in ms.
  virtual void setReadTimeout(int milliseconds) = 0;
  virtual void close() = 0;
  virtual std::string peer() const = 0;
};

class TcpTransport final : public ITransport {
 public:
  ~TcpTransport() override;

  static std::unique_ptr<TcpTransport> connect(const std::string& host, std::uint16_t port,
                                               int timeout_ms, std::string* err);

  bool sendAll(ByteView data, std::string* err) override;
  bool recvExact(std::uint8_t* data, std::size_t size, std::string* err) override;
  void setReadTimeout(int milliseconds) override;
  void close() override;
  std::string peer() const override { return peer_; }

 private:
  friend class TcpListener;
  explicit TcpTransport(std::intptr_t fd, std::string peer);

  std::atomic<int> fd_{-1};
  std::string peer_;
  std::mutex write_mutex_;
};

class TcpListener final {
 public:
  ~TcpListener();

  static std::unique_ptr<TcpListener> bind(std::uint16_t port, std::string* err);
  std::unique_ptr<TcpTransport> accept(std::string* err);
  std::uint16_t port() const { return port_; }

 private:
  TcpListener(std::intptr_t fd, std::uint16_t port);

  std::atomic<int> fd_{-1};
  std::uint16_t port_ = 0;
};

// In-memory byte-stream pair used by unit tests and the loopback demo.
std::pair<std::unique_ptr<ITransport>, std::unique_ptr<ITransport>>
makeMemoryTransportPair();

}  // namespace dhb
