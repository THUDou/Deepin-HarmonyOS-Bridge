// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "dhb/session.h"

namespace dhb {

struct DiscoveredPeer {
  std::string id;
  std::string name;
  std::string os;
  std::string app_version;
  std::string host;
  std::uint16_t port = 0;
};

// UDP broadcast discovery. This is deliberately dependency-free: it works on
// Deepin/UOS, in containers, and from the HarmonyOS app without mDNS/Avahi.
std::vector<DiscoveredPeer> discover(std::uint16_t discovery_port, int timeout_ms,
                                     std::string* err);

class DiscoveryResponder {
 public:
  DiscoveryResponder(std::uint16_t discovery_port, DeviceInfo local,
                     std::uint16_t tcp_port);
  ~DiscoveryResponder();

  DiscoveryResponder(const DiscoveryResponder&) = delete;
  DiscoveryResponder& operator=(const DiscoveryResponder&) = delete;

  bool start(std::string* err);
  void stop();

 private:
  void loop();

  std::uint16_t discovery_port_;
  DeviceInfo local_;
  std::uint16_t tcp_port_;
  std::atomic<int> fd_{-1};
  std::atomic<bool> running_{false};
  std::thread thread_;
};

}  // namespace dhb
