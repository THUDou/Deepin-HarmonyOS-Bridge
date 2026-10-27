// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#include "dhb/transport.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace dhb {
namespace {

bool setError(std::string* err, std::string message) {
  if (err != nullptr) *err = std::move(message);
  return false;
}

#ifndef _WIN32

bool setSocketTimeout(int fd, int milliseconds) {
  timeval tv{};
  if (milliseconds > 0) {
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
  }
  const bool ok_recv =
      ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
  const bool ok_send =
      ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
  return ok_recv && ok_send;
}

std::string socketPeer(int fd, const sockaddr_storage& addr, socklen_t addr_len) {
  char host[NI_MAXHOST] = {0};
  char service[NI_MAXSERV] = {0};
  if (::getnameinfo(reinterpret_cast<const sockaddr*>(&addr), addr_len, host, sizeof(host),
                    service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    return std::string(host) + ":" + service;
  }
  return "unknown";
}

#endif  // !_WIN32

// ---------------------------------------------------------------------------
// Memory transport
// ---------------------------------------------------------------------------
struct MemoryState {
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::vector<std::uint8_t>> chunks;
  std::size_t front_offset = 0;
  bool closed = false;
};

class MemoryTransport final : public ITransport {
 public:
  MemoryTransport(std::shared_ptr<MemoryState> incoming, std::shared_ptr<MemoryState> outgoing)
      : incoming_(std::move(incoming)), outgoing_(std::move(outgoing)) {}

  bool sendAll(ByteView data, std::string* err) override {
    if (data.empty()) return true;
    {
      std::lock_guard<std::mutex> lock(outgoing_->mutex);
      if (outgoing_->closed) return setError(err, "memory transport closed");
      outgoing_->chunks.emplace_back(data.data, data.data + data.size);
    }
    outgoing_->cv.notify_all();
    return true;
  }

  bool recvExact(std::uint8_t* data, std::size_t size, std::string* err) override {
    std::unique_lock<std::mutex> lock(incoming_->mutex);
    std::size_t filled = 0;
    while (filled < size) {
      if (incoming_->chunks.empty()) {
        if (incoming_->closed) return setError(err, "memory transport closed");
        incoming_->cv.wait(lock);
        continue;
      }
      auto& front = incoming_->chunks.front();
      const std::size_t available = front.size() - incoming_->front_offset;
      const std::size_t take = std::min(available, size - filled);
      if (take != 0) {
        std::memcpy(data + filled, front.data() + incoming_->front_offset, take);
      }
      filled += take;
      incoming_->front_offset += take;
      if (incoming_->front_offset == front.size()) {
        incoming_->chunks.pop_front();
        incoming_->front_offset = 0;
      }
    }
    return true;
  }

  void setReadTimeout(int /*milliseconds*/) override {}

  void close() override {
    {
      std::scoped_lock lock(incoming_->mutex, outgoing_->mutex);
      incoming_->closed = true;
      outgoing_->closed = true;
    }
    incoming_->cv.notify_all();
    outgoing_->cv.notify_all();
  }

  std::string peer() const override { return "memory"; }

 private:
  std::shared_ptr<MemoryState> incoming_;
  std::shared_ptr<MemoryState> outgoing_;
};

}  // namespace

std::pair<std::unique_ptr<ITransport>, std::unique_ptr<ITransport>>
makeMemoryTransportPair() {
  auto a_to_b = std::make_shared<MemoryState>();
  auto b_to_a = std::make_shared<MemoryState>();
  std::unique_ptr<ITransport> a(new MemoryTransport(a_to_b, b_to_a));
  std::unique_ptr<ITransport> b(new MemoryTransport(b_to_a, a_to_b));
  return {std::move(a), std::move(b)};
}

// ---------------------------------------------------------------------------
// TCP transport (POSIX)
// ---------------------------------------------------------------------------
#ifndef _WIN32

TcpTransport::TcpTransport(std::intptr_t fd, std::string peer)
    : fd_(static_cast<int>(fd)), peer_(std::move(peer)) {}

TcpTransport::~TcpTransport() { close(); }

std::unique_ptr<TcpTransport> TcpTransport::connect(const std::string& host,
                                                    std::uint16_t port, int timeout_ms,
                                                    std::string* err) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  addrinfo* results = nullptr;
  const std::string service = std::to_string(port);
  const int rc = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &results);
  if (rc != 0 || results == nullptr) {
    setError(err, "cannot resolve " + host + ": " + ::gai_strerror(rc));
    return nullptr;
  }

  for (addrinfo* it = results; it != nullptr; it = it->ai_next) {
    const int fd = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (fd < 0) continue;
    setSocketTimeout(fd, timeout_ms);
    if (::connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
      const int one = 1;
      ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
      sockaddr_storage addr{};
      socklen_t addr_len = sizeof(addr);
      ::getpeername(fd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
      std::string peer = socketPeer(fd, addr, addr_len);
      ::freeaddrinfo(results);
      return std::unique_ptr<TcpTransport>(new TcpTransport(fd, std::move(peer)));
    }
    ::close(fd);
  }

  ::freeaddrinfo(results);
  setError(err, "cannot connect to " + host + ":" + service);
  return nullptr;
}

bool TcpTransport::sendAll(ByteView data, std::string* err) {
  std::lock_guard<std::mutex> lock(write_mutex_);
  const int fd = fd_.load();
  if (fd < 0) return setError(err, "socket closed");

  std::size_t offset = 0;
  while (offset < data.size) {
    const ssize_t n =
        ::send(fd, data.data + offset, data.size - offset, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) return setError(err, "send timeout");
      return setError(err, std::string("send failed: ") + std::strerror(errno));
    }
    if (n == 0) return setError(err, "connection closed while sending");
    offset += static_cast<std::size_t>(n);
  }
  return true;
}

bool TcpTransport::recvExact(std::uint8_t* data, std::size_t size, std::string* err) {
  const int fd = fd_.load();
  if (fd < 0) return setError(err, "socket closed");

  std::size_t offset = 0;
  while (offset < size) {
    const ssize_t n = ::recv(fd, data + offset, size - offset, 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) return setError(err, "receive timeout");
      return setError(err, std::string("receive failed: ") + std::strerror(errno));
    }
    if (n == 0) return setError(err, "connection closed");
    offset += static_cast<std::size_t>(n);
  }
  return true;
}

void TcpTransport::setReadTimeout(int milliseconds) {
  const int fd = fd_.load();
  if (fd >= 0) setSocketTimeout(fd, milliseconds);
}

void TcpTransport::close() {
  const int fd = fd_.exchange(-1);
  if (fd >= 0) {
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
  }
}

TcpListener::TcpListener(std::intptr_t fd, std::uint16_t port)
    : fd_(static_cast<int>(fd)), port_(port) {}

TcpListener::~TcpListener() {
  const int fd = fd_.exchange(-1);
  if (fd >= 0) ::close(fd);
}

std::unique_ptr<TcpListener> TcpListener::bind(std::uint16_t port, std::string* err) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    setError(err, std::string("socket failed: ") + std::strerror(errno));
    return nullptr;
  }
  const int one = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    setError(err, std::string("bind failed: ") + std::strerror(errno));
    ::close(fd);
    return nullptr;
  }
  if (::listen(fd, 16) != 0) {
    setError(err, std::string("listen failed: ") + std::strerror(errno));
    ::close(fd);
    return nullptr;
  }

  sockaddr_in bound{};
  socklen_t bound_len = sizeof(bound);
  std::uint16_t actual_port = port;
  if (::getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &bound_len) == 0) {
    actual_port = ntohs(bound.sin_port);
  }
  return std::unique_ptr<TcpListener>(new TcpListener(fd, actual_port));
}

std::unique_ptr<TcpTransport> TcpListener::accept(std::string* err) {
  const int listen_fd = fd_.load();
  if (listen_fd < 0) {
    setError(err, "listener closed");
    return nullptr;
  }
  sockaddr_storage addr{};
  socklen_t addr_len = sizeof(addr);
  const int fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
  if (fd < 0) {
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      setError(err, "accept timeout");
    } else {
      setError(err, std::string("accept failed: ") + std::strerror(errno));
    }
    return nullptr;
  }
  const int one = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  setSocketTimeout(fd, 10000);
  return std::unique_ptr<TcpTransport>(
      new TcpTransport(fd, socketPeer(fd, addr, addr_len)));
}

#else  // _WIN32

// The runtime target is Linux. Keep the core testable on Windows, but do not
// pretend that the Windows socket path is production-ready.
TcpTransport::TcpTransport(std::intptr_t fd, std::string peer)
    : fd_(static_cast<int>(fd)), peer_(std::move(peer)) {}
TcpTransport::~TcpTransport() = default;

std::unique_ptr<TcpTransport> TcpTransport::connect(const std::string&, std::uint16_t, int,
                                                    std::string* err) {
  setError(err, "TCP transport is not implemented on Windows in this build");
  return nullptr;
}
bool TcpTransport::sendAll(ByteView, std::string* err) {
  return setError(err, "TCP transport is not implemented on Windows in this build");
}
bool TcpTransport::recvExact(std::uint8_t*, std::size_t, std::string* err) {
  return setError(err, "TCP transport is not implemented on Windows in this build");
}
void TcpTransport::setReadTimeout(int) {}
void TcpTransport::close() { fd_.store(-1); }

TcpListener::TcpListener(std::intptr_t fd, std::uint16_t port)
    : fd_(static_cast<int>(fd)), port_(port) {}
TcpListener::~TcpListener() = default;
std::unique_ptr<TcpListener> TcpListener::bind(std::uint16_t, std::string* err) {
  setError(err, "TCP transport is not implemented on Windows in this build");
  return nullptr;
}
std::unique_ptr<TcpTransport> TcpListener::accept(std::string* err) {
  setError(err, "TCP transport is not implemented on Windows in this build");
  return nullptr;
}

#endif  // _WIN32

}  // namespace dhb
