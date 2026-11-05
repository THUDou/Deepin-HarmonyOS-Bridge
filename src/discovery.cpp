// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#include "dhb/discovery.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace dhb {
namespace {

bool setError(std::string* err, std::string message) {
  if (err != nullptr) *err = std::move(message);
  return false;
}

#ifndef _WIN32
std::string sanitizeField(std::string value) {
  for (char& c : value) {
    if (c == '\t' || c == '\n' || c == '\r' || c == '|') c = ' ';
  }
  if (value.size() > 120) value.resize(120);
  return value;
}

std::vector<std::string> splitTabs(const std::string& line) {
  std::vector<std::string> fields;
  std::size_t start = 0;
  while (start <= line.size()) {
    const std::size_t tab = line.find('\t', start);
    if (tab == std::string::npos) {
      fields.push_back(line.substr(start));
      break;
    }
    fields.push_back(line.substr(start, tab - start));
    start = tab + 1;
  }
  return fields;
}

void setReceiveTimeout(int fd, int milliseconds) {
  timeval tv{};
  tv.tv_sec = milliseconds / 1000;
  tv.tv_usec = (milliseconds % 1000) * 1000;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}
#endif

}  // namespace

std::vector<DiscoveredPeer> discover(std::uint16_t discovery_port, int timeout_ms,
                                     std::string* err) {
  std::vector<DiscoveredPeer> peers;
#ifdef _WIN32
  (void)discovery_port;
  (void)timeout_ms;
  setError(err, "UDP discovery is not implemented on Windows in this build");
  return peers;
#else
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    setError(err, std::string("discovery socket failed: ") + std::strerror(errno));
    return peers;
  }
  const int one = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
  setReceiveTimeout(fd, 200);

  sockaddr_in target{};
  target.sin_family = AF_INET;
  target.sin_port = htons(discovery_port);
  target.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  const char kQuery[] = "DHB1?\n";
  ::sendto(fd, kQuery, sizeof(kQuery) - 1, 0, reinterpret_cast<sockaddr*>(&target),
           sizeof(target));
  target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  ::sendto(fd, kQuery, sizeof(kQuery) - 1, 0, reinterpret_cast<sockaddr*>(&target),
           sizeof(target));

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  std::map<std::string, std::size_t> seen;

  while (std::chrono::steady_clock::now() < deadline) {
    sockaddr_in from{};
    socklen_t from_len = sizeof(from);
    char buffer[1024] = {0};
    const ssize_t n = ::recvfrom(fd, buffer, sizeof(buffer) - 1, 0,
                                 reinterpret_cast<sockaddr*>(&from), &from_len);
    if (n <= 0) continue;
    buffer[n] = '\0';

    std::string line(buffer);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    if (line.rfind("DHB1!", 0) != 0) continue;

    const std::string body = line.substr(5);
    const std::vector<std::string> fields = splitTabs(body);
    if (fields.size() < 5) continue;

    DiscoveredPeer peer;
    peer.port = static_cast<std::uint16_t>(std::strtoul(fields[0].c_str(), nullptr, 10));
    peer.id = fields[1];
    peer.name = fields[2];
    peer.os = fields[3];
    peer.app_version = fields[4];
    if (fields.size() >= 6 && fields[5] != "0.0.0.0") {
      peer.host = fields[5];
    } else {
      char host[NI_MAXHOST] = {0};
      if (::getnameinfo(reinterpret_cast<sockaddr*>(&from), from_len, host, sizeof(host),
                        nullptr, 0, NI_NUMERICHOST) == 0) {
        peer.host = host;
      }
    }
    if (peer.id.empty() || peer.port == 0 || peer.host.empty()) continue;

    const auto it = seen.find(peer.id);
    if (it == seen.end()) {
      seen[peer.id] = peers.size();
      peers.push_back(std::move(peer));
    } else {
      peers[it->second] = std::move(peer);
    }
  }
  ::close(fd);
  return peers;
#endif
}

DiscoveryResponder::DiscoveryResponder(std::uint16_t discovery_port, DeviceInfo local,
                                       std::uint16_t tcp_port)
    : discovery_port_(discovery_port), local_(std::move(local)), tcp_port_(tcp_port) {}

DiscoveryResponder::~DiscoveryResponder() { stop(); }

bool DiscoveryResponder::start(std::string* err) {
#ifdef _WIN32
  return setError(err, "UDP discovery is not implemented on Windows in this build");
#else
  if (running_.load()) return true;
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return setError(err, std::string("discovery socket failed: ") + std::strerror(errno));
  }
  const int one = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(discovery_port_);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    const std::string message = std::string("discovery bind failed: ") + std::strerror(errno);
    ::close(fd);
    return setError(err, message);
  }
  setReceiveTimeout(fd, 500);
  fd_.store(fd);
  running_.store(true);
  thread_ = std::thread(&DiscoveryResponder::loop, this);
  return true;
#endif
}

void DiscoveryResponder::stop() {
#ifndef _WIN32
  running_.store(false);
  const int fd = fd_.exchange(-1);
  if (fd >= 0) ::close(fd);
  if (thread_.joinable()) thread_.join();
#endif
}

void DiscoveryResponder::loop() {
#ifndef _WIN32
  while (running_.load()) {
    sockaddr_in from{};
    socklen_t from_len = sizeof(from);
    char buffer[128] = {0};
    const ssize_t n = ::recvfrom(fd_.load(), buffer, sizeof(buffer) - 1, 0,
                                 reinterpret_cast<sockaddr*>(&from), &from_len);
    if (n <= 0) continue;
    buffer[n] = '\0';
    if (std::strncmp(buffer, "DHB1?", 5) != 0) continue;

    // Find the local address that reaches this peer, so the client does not
    // have to trust SocketRemoteInfo (whose address field is ambiguous on
    // some HarmonyOS versions).
    std::string local_host = "0.0.0.0";
    const int probe = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (probe >= 0) {
      if (::connect(probe, reinterpret_cast<sockaddr*>(&from), from_len) == 0) {
        sockaddr_in local_addr{};
        socklen_t local_len = sizeof(local_addr);
        if (::getsockname(probe, reinterpret_cast<sockaddr*>(&local_addr), &local_len) == 0) {
          char ip[INET_ADDRSTRLEN] = {0};
          if (::inet_ntop(AF_INET, &local_addr.sin_addr, ip, sizeof(ip)) != nullptr) {
            local_host = ip;
          }
        }
      }
      ::close(probe);
    }

    std::ostringstream reply;
    reply << "DHB1!" << tcp_port_ << '\t' << sanitizeField(local_.id) << '\t'
          << sanitizeField(local_.name) << '\t' << sanitizeField(local_.os) << '\t'
          << sanitizeField(local_.app_version) << '\t' << local_host << '\n';
    const std::string text = reply.str();
    ::sendto(fd_.load(), text.c_str(), text.size(), 0, reinterpret_cast<sockaddr*>(&from),
             from_len);
  }
#endif
}

}  // namespace dhb
