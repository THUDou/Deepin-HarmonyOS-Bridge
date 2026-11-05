// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "dhb/crypto.h"
#include "dhb/discovery.h"
#include "dhb/protocol.h"
#include "dhb/session.h"
#include "dhb/transport.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

using namespace dhb;

constexpr const char* kVersion = "0.1.0";
constexpr std::uint16_t kDefaultPort = 47821;
constexpr std::uint16_t kDefaultDiscoveryPort = 47822;

void printUsage() {
  std::cout
      << "DHB - Deepin-HarmonyOS-Bridge  v" << kVersion << "\n\n"
      << "Usage:\n"
      << "  dhb serve   [--port 47821] [--discovery-port 47822] [--dir DIR]\n"
      << "              [--psk HEX] [--insecure] [--send-on-connect FILE]\n"
      << "  dhb send    --host IP [--port 47821] [--psk HEX|--insecure] FILE...\n"
      << "  dhb pair    --host IP [--port 47821] [--psk HEX|--insecure]\n"
      << "  dhb recv    --host IP [--port 47821] [--psk HEX|--insecure]\n"
      << "              [--dir DIR] [--timeout 60]\n"
      << "  dhb discover [--discovery-port 47822] [--timeout 3000]\n"
      << "  dhb doctor\n"
      << "  dhb selftest\n\n"
      << "Security: use the 64-hex pairing key printed by `dhb serve` on both ends.\n"
      << "`--insecure` disables authentication and is only for loopback development.\n";
}

std::string hostname() {
#ifdef _WIN32
  char buffer[256] = {0};
  DWORD size = sizeof(buffer);
  if (GetComputerNameA(buffer, &size)) return buffer;
  return "windows-device";
#else
  char buffer[256] = {0};
  if (gethostname(buffer, sizeof(buffer) - 1) == 0) return buffer;
  return "linux-device";
#endif
}

std::string localDeviceId() {
#ifndef _WIN32
  for (const char* path : {"/etc/machine-id", "/var/lib/dbus/machine-id"}) {
    std::ifstream input(path);
    std::string line;
    if (input && std::getline(input, line) && !line.empty()) {
      if (line.size() > 16) line.resize(16);
      return line;
    }
  }
#endif
  const std::string seed = hostname() + "-dhb";
  const auto digest = crypto::sha256(ByteView(seed));
  return crypto::toHex(ByteView(digest.data(), 8));
}

DeviceInfo makeLocalDevice() {
  DeviceInfo info;
  info.id = localDeviceId();
  info.name = hostname();
  info.os = "Deepin/UOS Linux";
  info.app_version = kVersion;
  info.caps = kCapFile | kCapClipboard;
  return info;
}

std::string defaultReceiveDir() {
#ifndef _WIN32
  const char* home = std::getenv("HOME");
  if (home != nullptr && *home != '\0') return std::string(home) + "/DHB";
#endif
  return "DHB-received";
}

std::string generatePsk() {
  const auto bytes = crypto::randomBytes32();
  return crypto::toHex(ByteView(bytes.data(), bytes.size()));
}

bool parsePort(const std::string& text, std::uint16_t* port) {
  try {
    const int value = std::stoi(text);
    if (value <= 0 || value > 65535) return false;
    *port = static_cast<std::uint16_t>(value);
    return true;
  } catch (...) {
    return false;
  }
}

struct ServeOptions {
  std::uint16_t port = kDefaultPort;
  std::uint16_t discovery_port = kDefaultDiscoveryPort;
  std::string dir = defaultReceiveDir();
  std::string psk;
  bool insecure = false;
  std::string send_on_connect;
};

int runServe(const std::vector<std::string>& args) {
  ServeOptions options;
  for (std::size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--port" && i + 1 < args.size()) {
      if (!parsePort(args[++i], &options.port)) {
        std::cerr << "invalid --port\n";
        return 2;
      }
    } else if (args[i] == "--discovery-port" && i + 1 < args.size()) {
      if (!parsePort(args[++i], &options.discovery_port)) {
        std::cerr << "invalid --discovery-port\n";
        return 2;
      }
    } else if (args[i] == "--dir" && i + 1 < args.size()) {
      options.dir = args[++i];
    } else if (args[i] == "--psk" && i + 1 < args.size()) {
      options.psk = args[++i];
    } else if (args[i] == "--insecure") {
      options.insecure = true;
    } else if (args[i] == "--send-on-connect" && i + 1 < args.size()) {
      options.send_on_connect = args[++i];
    } else {
      std::cerr << "unknown option: " << args[i] << "\n";
      return 2;
    }
  }

  if (options.psk.empty() && !options.insecure) options.psk = generatePsk();
  const DeviceInfo local = makeLocalDevice();

  std::string err;
  auto listener = TcpListener::bind(options.port, &err);
  if (!listener) {
    std::cerr << "cannot listen: " << err << "\n";
    return 1;
  }
  auto responder = std::make_unique<DiscoveryResponder>(options.discovery_port, local,
                                                        options.port);
  if (!responder->start(&err)) {
    std::cerr << "warning: discovery disabled: " << err << "\n";
  }

  std::cout << "DHB daemon ready\n"
            << "  device : " << local.name << " (" << local.id << ")\n"
            << "  listen : 0.0.0.0:" << options.port << "\n"
            << "  save   : " << options.dir << "\n";
  if (options.insecure) {
    std::cout << "  auth   : DISABLED (--insecure)\n";
  } else {
    std::cout << "  pairing key: " << options.psk << "\n"
              << "  (copy it to the HarmonyOS app / `dhb send --psk`)\n";
  }
  std::cout << "Waiting for devices...\n";

  while (true) {
    std::string accept_error;
    auto transport = listener->accept(&accept_error);
    if (!transport) {
      if (accept_error != "accept timeout") {
        std::cerr << "accept failed: " << accept_error << "\n";
      }
      continue;
    }

    SessionConfig config;
    config.local = local;
    config.psk = options.psk;
    config.insecure = options.insecure;
    config.recv_dir = options.dir;
    config.is_client = false;

    std::thread([transport = std::move(transport), config,
                 send_on_connect = options.send_on_connect]() mutable {
      Session session(std::move(transport), config);
      session.setLogHandler([](std::string message) {
        std::cout << "[session] " << message << "\n";
      });
      session.setFileReceivedHandler([](const std::filesystem::path& path) {
        std::cout << "[file] saved: " << path.string() << "\n";
      });
      session.setClipboardHandler([](std::string text) {
        std::string preview = text.substr(0, 80);
        std::cout << "[clipboard] " << preview;
        if (text.size() > preview.size()) std::cout << "...";
        std::cout << "\n";
      });

      std::string session_error;
      if (!session.handshake(&session_error)) {
        std::cerr << "[session] handshake failed: " << session_error << "\n";
        return;
      }
      if (!session.start(&session_error)) {
        std::cerr << "[session] start failed: " << session_error << "\n";
        return;
      }
      std::cout << "[session] connected: " << session.peer().name << " ("
                << session.peer().os << ")\n";

      if (!send_on_connect.empty()) {
        std::string send_error;
        std::cout << "[file] sending on connect: " << send_on_connect << "\n";
        if (session.sendFile(send_on_connect, nullptr, &send_error)) {
          std::cout << "[file] sent: " << send_on_connect << "\n";
        } else {
          std::cerr << "[file] send failed: " << send_error << "\n";
        }
      }

      while (session.connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
      std::cout << "[session] disconnected: " << session.peer().name << "\n";
    }).detach();
  }
}

struct SendOptions {
  std::string host;
  std::uint16_t port = kDefaultPort;
  std::string psk;
  bool insecure = false;
  std::vector<std::string> files;
};

int runSend(const std::vector<std::string>& args) {
  SendOptions options;
  for (std::size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--host" && i + 1 < args.size()) {
      options.host = args[++i];
    } else if (args[i] == "--port" && i + 1 < args.size()) {
      if (!parsePort(args[++i], &options.port)) {
        std::cerr << "invalid --port\n";
        return 2;
      }
    } else if (args[i] == "--psk" && i + 1 < args.size()) {
      options.psk = args[++i];
    } else if (args[i] == "--insecure") {
      options.insecure = true;
    } else if (args[i].rfind("--", 0) == 0) {
      std::cerr << "unknown option: " << args[i] << "\n";
      return 2;
    } else {
      options.files.push_back(args[i]);
    }
  }
  if (options.host.empty() || options.files.empty()) {
    printUsage();
    return 2;
  }
  if (options.psk.empty() && !options.insecure) {
    std::cerr << "missing --psk (or --insecure for loopback tests)\n";
    return 2;
  }

  std::string err;
  auto transport = TcpTransport::connect(options.host, options.port, 10000, &err);
  if (!transport) {
    std::cerr << "connect failed: " << err << "\n";
    return 1;
  }

  SessionConfig config;
  config.local = makeLocalDevice();
  config.psk = options.psk;
  config.insecure = options.insecure;
  config.recv_dir = defaultReceiveDir();
  config.is_client = true;

  Session session(std::move(transport), config);
  session.setLogHandler([](std::string message) {
    std::cout << "[session] " << message << "\n";
  });
  session.setClipboardHandler([](std::string text) {
    std::cout << "[clipboard] " << text.substr(0, 80) << "\n";
  });
  if (!session.handshake(&err)) {
    std::cerr << "handshake failed: " << err << "\n";
    return 1;
  }
  if (!session.start(&err)) {
    std::cerr << "start failed: " << err << "\n";
    return 1;
  }
  std::cout << "connected: " << session.peer().name << " (" << session.peer().os << ")\n";

  int failed = 0;
  for (const std::string& file : options.files) {
    int last_percent = -1;
    const auto progress = [&](const FileProgress& p) {
      if (p.total == 0) return;
      const int percent = static_cast<int>((p.transferred * 100) / p.total);
      if (percent != last_percent) {
        last_percent = percent;
        std::cout << "\r  " << p.name << " " << std::setw(3) << percent << "% "
                  << p.transferred << "/" << p.total << std::flush;
      }
    };
    std::string send_error;
    if (session.sendFile(file, progress, &send_error)) {
      std::cout << "\r  " << file << " 100% OK\n";
    } else {
      std::cout << "\r  " << file << " FAILED: " << send_error << "\n";
      ++failed;
    }
  }

  session.close();
  return failed == 0 ? 0 : 1;
}

std::string formatProfiles(std::uint16_t flags) {
  std::string out;
  const auto add = [&](std::uint16_t bit, const char* name) {
    if ((flags & bit) == 0) return;
    if (!out.empty()) out += "|";
    out += name;
  };
  add(kHelloFlagSoftbus4x, "softbus4x");
  add(kHelloFlagSoftbus5x, "softbus5x");
  add(kHelloFlagSoftbus6x, "softbus6x");
  return out.empty() ? "app/unspecified" : out;
}

std::string formatCaps(std::uint32_t caps) {
  std::string out;
  const auto add = [&](std::uint32_t bit, const char* name) {
    if ((caps & bit) == 0) return;
    if (!out.empty()) out += "|";
    out += name;
  };
  add(kCapFile, "file");
  add(kCapClipboard, "clipboard");
  add(kCapScreenSink, "screen-sink");
  add(kCapScreenSource, "screen-source");
  add(kCapInput, "input");
  add(kCapNotify, "notify");
  add(kCapDiscovery, "discovery");
  return out.empty() ? "none" : out;
}

struct PairOptions {
  std::string host;
  std::uint16_t port = kDefaultPort;
  std::string psk;
  bool insecure = false;
};

int runPair(const std::vector<std::string>& args) {
  PairOptions options;
  for (std::size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--host" && i + 1 < args.size()) {
      options.host = args[++i];
    } else if (args[i] == "--port" && i + 1 < args.size()) {
      if (!parsePort(args[++i], &options.port)) {
        std::cerr << "invalid --port\n";
        return 2;
      }
    } else if (args[i] == "--psk" && i + 1 < args.size()) {
      options.psk = args[++i];
    } else if (args[i] == "--insecure") {
      options.insecure = true;
    } else {
      std::cerr << "unknown option: " << args[i] << "\n";
      return 2;
    }
  }
  if (options.host.empty()) {
    printUsage();
    return 2;
  }
  if (options.psk.empty() && !options.insecure) {
    std::cerr << "missing --psk (or --insecure for loopback tests)\n";
    return 2;
  }

  std::string err;
  auto transport = TcpTransport::connect(options.host, options.port, 10000, &err);
  if (!transport) {
    std::cerr << "connect failed: " << err << "\n";
    return 1;
  }

  SessionConfig config;
  config.local = makeLocalDevice();
  config.psk = options.psk;
  config.insecure = options.insecure;
  config.recv_dir = defaultReceiveDir();
  config.is_client = true;

  Session session(std::move(transport), config);
  session.setLogHandler([](std::string message) {
    std::cout << "[session] " << message << "\n";
  });
  if (!session.handshake(&err)) {
    std::cerr << "authentication failed: " << err << "\n";
    return 1;
  }

  std::cout << "authenticated: " << session.peer().name << " (" << session.peer().os
            << ")\n"
            << "  device id: " << session.peer().id << "\n"
            << "  caps     : " << formatCaps(session.peer().caps) << "\n"
            << "  profile  : " << formatProfiles(session.peerFlags()) << "\n";
  session.close();
  return 0;
}

struct RecvOptions {
  std::string host;
  std::uint16_t port = kDefaultPort;
  std::string psk;
  bool insecure = false;
  std::string dir = defaultReceiveDir();
  int timeout_seconds = 60;
};

int runRecv(const std::vector<std::string>& args) {
  RecvOptions options;
  for (std::size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--host" && i + 1 < args.size()) {
      options.host = args[++i];
    } else if (args[i] == "--port" && i + 1 < args.size()) {
      if (!parsePort(args[++i], &options.port)) {
        std::cerr << "invalid --port\n";
        return 2;
      }
    } else if (args[i] == "--psk" && i + 1 < args.size()) {
      options.psk = args[++i];
    } else if (args[i] == "--insecure") {
      options.insecure = true;
    } else if (args[i] == "--dir" && i + 1 < args.size()) {
      options.dir = args[++i];
    } else if (args[i] == "--timeout" && i + 1 < args.size()) {
      options.timeout_seconds = std::stoi(args[++i]);
    } else {
      std::cerr << "unknown option: " << args[i] << "\n";
      return 2;
    }
  }
  if (options.host.empty()) {
    printUsage();
    return 2;
  }
  if (options.psk.empty() && !options.insecure) {
    std::cerr << "missing --psk (or --insecure for loopback tests)\n";
    return 2;
  }

  std::string err;
  auto transport = TcpTransport::connect(options.host, options.port, 10000, &err);
  if (!transport) {
    std::cerr << "connect failed: " << err << "\n";
    return 1;
  }

  SessionConfig config;
  config.local = makeLocalDevice();
  config.psk = options.psk;
  config.insecure = options.insecure;
  config.recv_dir = options.dir;
  config.is_client = true;

  Session session(std::move(transport), config);
  std::mutex mutex;
  std::condition_variable cv;
  bool received = false;
  std::filesystem::path saved_path;
  session.setLogHandler([](std::string message) {
    std::cout << "[session] " << message << "\n";
  });
  session.setFileReceivedHandler([&](const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex);
    saved_path = path;
    received = true;
    cv.notify_all();
  });
  session.setClipboardHandler([](std::string text) {
    std::cout << "[clipboard] " << text.substr(0, 80) << "\n";
  });

  if (!session.handshake(&err)) {
    std::cerr << "handshake failed: " << err << "\n";
    return 1;
  }
  if (!session.start(&err)) {
    std::cerr << "start failed: " << err << "\n";
    return 1;
  }
  std::cout << "connected: " << session.peer().name << "; waiting "
            << options.timeout_seconds << "s for a file...\n";

  std::unique_lock<std::mutex> lock(mutex);
  const bool ok = cv.wait_for(lock, std::chrono::seconds(options.timeout_seconds),
                              [&] { return received; });
  if (!ok) {
    std::cerr << "timed out waiting for a file\n";
    lock.unlock();
    session.close();
    return 1;
  }
  const std::filesystem::path result = saved_path;
  lock.unlock();
  std::cout << "received: " << result.string() << "\n";
  session.close();
  return 0;
}

int runDiscover(const std::vector<std::string>& args) {
  std::uint16_t port = kDefaultDiscoveryPort;
  int timeout_ms = 3000;
  for (std::size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--discovery-port" && i + 1 < args.size()) {
      if (!parsePort(args[++i], &port)) {
        std::cerr << "invalid --discovery-port\n";
        return 2;
      }
    } else if (args[i] == "--timeout" && i + 1 < args.size()) {
      timeout_ms = std::stoi(args[++i]);
    }
  }

  std::string err;
  const auto peers = discover(port, timeout_ms, &err);
  if (!err.empty()) std::cerr << "discovery warning: " << err << "\n";
  if (peers.empty()) {
    std::cout << "no DHB peers found on UDP " << port << "\n";
    return 1;
  }
  std::cout << "DHB peers:\n";
  for (const auto& peer : peers) {
    std::cout << "  " << peer.name << "  " << peer.host << ":" << peer.port << "  "
              << peer.os << "  " << peer.id << "\n";
  }
  return 0;
}

int runDoctor() {
  const DeviceInfo local = makeLocalDevice();
  std::cout << "DHB doctor\n"
            << "  version : " << kVersion << "\n"
            << "  device  : " << local.name << "\n"
            << "  id      : " << local.id << "\n"
            << "  os      : " << local.os << "\n"
            << "  save dir: " << defaultReceiveDir() << "\n";

  const auto empty = crypto::sha256(ByteView(""));
  std::cout << "  sha256  : " << crypto::toHex(ByteView(empty.data(), empty.size()))
            << "\n";
  std::cout << "  crc32c  : 0x" << std::hex << crypto::crc32c(ByteView("123456789"))
            << std::dec << " (expect 0xe3069283)\n";

  std::string err;
  auto listener = TcpListener::bind(0, &err);
  if (listener) {
    std::cout << "  tcp bind: ok (ephemeral port " << listener->port() << ")\n";
  } else {
    std::cout << "  tcp bind: FAILED: " << err << "\n";
  }
  return 0;
}

int runSelftest() {
  bool ok = true;
  const auto empty = crypto::sha256(ByteView(""));
  const std::string empty_hex = crypto::toHex(ByteView(empty.data(), empty.size()));
  const std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
  if (empty_hex != expected) {
    std::cerr << "sha256 self-test failed: " << empty_hex << "\n";
    ok = false;
  }
  if (crypto::crc32c(ByteView("123456789")) != 0xe3069283u) {
    std::cerr << "crc32c self-test failed\n";
    ok = false;
  }

  Frame frame;
  frame.type = FrameType::kPing;
  const std::vector<std::uint8_t> payload = encodePing(PingMsg{12345});
  frame.payload = payload;
  const std::vector<std::uint8_t> bytes = encodeFrame(frame);
  Frame decoded;
  std::size_t consumed = 0;
  std::string err;
  if (!decodeFrame(bytes.data(), bytes.size(), decoded, &consumed, &err) ||
      decoded.type != FrameType::kPing || decoded.payload != payload ||
      consumed != bytes.size()) {
    std::cerr << "frame self-test failed: " << err << "\n";
    ok = false;
  }
  std::cout << (ok ? "selftest OK" : "selftest FAILED") << "\n";
  return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  std::cout.setf(std::ios::unitbuf);
  std::cerr.setf(std::ios::unitbuf);
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) args.emplace_back(argv[i] != nullptr ? argv[i] : "");
  if (args.size() < 2) {
    printUsage();
    return 2;
  }
  const std::string command = args[1];
  if (command == "--help" || command == "-h" || command == "help") {
    printUsage();
    return 0;
  }
  if (command == "serve") return runServe(args);
  if (command == "send") return runSend(args);
  if (command == "pair") return runPair(args);
  if (command == "recv") return runRecv(args);
  if (command == "discover") return runDiscover(args);
  if (command == "doctor") return runDoctor();
  if (command == "selftest") return runSelftest();

  std::cerr << "unknown command: " << command << "\n";
  printUsage();
  return 2;
}
