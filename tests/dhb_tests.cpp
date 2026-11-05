// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "dhb/crypto.h"
#include "dhb/protocol.h"
#include "dhb/session.h"
#include "dhb/transport.h"

namespace {

std::atomic<int> g_failures{0};

#define CHECK(condition)                                                          \
  do {                                                                            \
    if (!(condition)) {                                                           \
      std::cerr << __FILE__ << ":" << __LINE__ << " CHECK failed: " #condition    \
                << "\n";                                                          \
      g_failures.fetch_add(1);                                                    \
    }                                                                             \
  } while (false)

std::string hex32(const std::array<std::uint8_t, 32>& value) {
  return dhb::crypto::toHex(dhb::ByteView(value.data(), value.size()));
}

std::string randomSuffix() {
  const auto bytes = dhb::crypto::randomBytes32();
  return dhb::crypto::toHex(dhb::ByteView(bytes.data(), 4));
}

std::filesystem::path makeTempDir(const std::string& label) {
  // Keep test data inside the workspace: some sandboxes deny writes to the
  // system temp directory even when the workspace itself is writable.
  const auto dir = std::filesystem::current_path() / ".dhb-test" /
                   (label + "-" + randomSuffix());
  std::filesystem::create_directories(dir);
  return dir;
}

std::vector<std::uint8_t> makePattern(std::size_t size) {
  std::vector<std::uint8_t> data(size);
  for (std::size_t i = 0; i < size; ++i) {
    data[i] = static_cast<std::uint8_t>((i * 31u + 7u) & 0xffu);
  }
  return data;
}

void testCrypto() {
  CHECK(dhb::crypto::crc32c(dhb::ByteView("123456789")) == 0xe3069283u);

  CHECK(hex32(dhb::crypto::sha256(dhb::ByteView(""))) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(hex32(dhb::crypto::sha256(dhb::ByteView("abc"))) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  std::vector<std::uint8_t> million_a(1000000, 'a');
  CHECK(hex32(dhb::crypto::sha256(dhb::ByteView(million_a))) ==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

  std::vector<std::uint8_t> key1(20, 0x0b);
  CHECK(hex32(dhb::crypto::hmacSha256(dhb::ByteView(key1), dhb::ByteView("Hi There"))) ==
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
  CHECK(hex32(dhb::crypto::hmacSha256(dhb::ByteView("Jefe"),
                                      dhb::ByteView("what do ya want for nothing?"))) ==
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

  std::vector<std::uint8_t> ikm(22, 0x0b);
  std::uint8_t salt_bytes[13] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};
  std::uint8_t info_bytes[10] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4,
                                 0xf5, 0xf6, 0xf7, 0xf8, 0xf9};
  const auto okm = dhb::crypto::hkdfSha256(
      dhb::ByteView(ikm), dhb::ByteView(salt_bytes, sizeof(salt_bytes)),
      dhb::ByteView(info_bytes, sizeof(info_bytes)));
  CHECK(hex32(okm) ==
        "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf");

  const std::string encoded = dhb::crypto::toHex(dhb::ByteView("DHB"));
  CHECK(encoded == "444842");
  std::vector<std::uint8_t> decoded;
  CHECK(dhb::crypto::fromHex(encoded, decoded));
  CHECK(decoded.size() == 3 && decoded[0] == 'D' && decoded[2] == 'B');
  CHECK(!dhb::crypto::fromHex("xyz", decoded));
}

void testByteIo() {
  dhb::ByteWriter writer;
  writer.u8(0x12);
  writer.u16(0x3456);
  writer.u32(0x789abcdeu);
  writer.u64(0x1122334455667788ull);
  writer.str("hello");
  writer.bytes(reinterpret_cast<const std::uint8_t*>("\x00\xff"), 2);

  dhb::ByteReader reader(dhb::ByteView(writer.data()));
  std::uint8_t v8 = 0;
  std::uint16_t v16 = 0;
  std::uint32_t v32 = 0;
  std::uint64_t v64 = 0;
  std::string text;
  std::uint8_t raw[2] = {0, 0};
  CHECK(reader.u8(v8) && v8 == 0x12);
  CHECK(reader.u16(v16) && v16 == 0x3456);
  CHECK(reader.u32(v32) && v32 == 0x789abcdeu);
  CHECK(reader.u64(v64) && v64 == 0x1122334455667788ull);
  CHECK(reader.str(text) && text == "hello");
  CHECK(reader.bytes(raw, 2) && raw[0] == 0x00 && raw[1] == 0xff);
  CHECK(reader.done());
  CHECK(!reader.u8(v8));
}

void testFrames() {
  dhb::Frame frame;
  frame.type = dhb::FrameType::kFileChunk;
  frame.flags = dhb::kFlagMore;
  frame.stream = 7;
  frame.payload = {1, 2, 3, 4, 5};

  const std::vector<std::uint8_t> encoded = dhb::encodeFrame(frame);
  CHECK(encoded.size() == dhb::kFrameHeaderSize + frame.payload.size());

  dhb::Frame decoded;
  std::size_t consumed = 0;
  std::string err;
  CHECK(dhb::decodeFrame(encoded.data(), encoded.size(), decoded, &consumed, &err));
  CHECK(consumed == encoded.size());
  CHECK(decoded.type == frame.type);
  CHECK(decoded.stream == frame.stream);
  CHECK(decoded.payload == frame.payload);

  std::vector<std::uint8_t> corrupted = encoded;
  corrupted.back() ^= 0xff;
  CHECK(!dhb::decodeFrame(corrupted.data(), corrupted.size(), decoded, &consumed, &err));

  CHECK(!dhb::decodeFrame(encoded.data(), encoded.size() - 1, decoded, &consumed, &err));

  std::vector<std::uint8_t> bad_magic = encoded;
  bad_magic[0] ^= 0xff;
  CHECK(!dhb::decodeFrame(bad_magic.data(), bad_magic.size(), decoded, &consumed, &err));

  std::vector<std::uint8_t> too_large = encoded;
  too_large[12] = 0xff;
  too_large[13] = 0xff;
  too_large[14] = 0xff;
  too_large[15] = 0x7f;
  CHECK(!dhb::decodeFrame(too_large.data(), too_large.size(), decoded, &consumed, &err));
}

void testMessages() {
  dhb::Hello hello;
  hello.caps = dhb::kCapFile | dhb::kCapClipboard;
  hello.flags = dhb::kHelloFlagSoftbus4x | dhb::kHelloFlagSoftbus6x;
  hello.device_id = "device-1";
  hello.device_name = "Deepin 25";
  hello.os = "Deepin/UOS Linux";
  hello.app_version = "0.1.0";
  dhb::crypto::fillRandom(hello.nonce.data(), hello.nonce.size());

  dhb::Hello hello_out;
  std::string err;
  CHECK(dhb::decodeHello(dhb::ByteView(dhb::encodeHello(hello)), hello_out, &err));
  CHECK(hello_out.device_name == hello.device_name);
  CHECK(hello_out.caps == hello.caps);
  CHECK(hello_out.flags == hello.flags);
  CHECK(hello_out.nonce == hello.nonce);

  dhb::FileOffer offer;
  offer.id = 42;
  offer.size = 123456789;
  offer.mtime = 1700000000;
  offer.mode = 0644;
  offer.name = "报告 2026.pdf";
  for (std::size_t i = 0; i < offer.sha256.size(); ++i) {
    offer.sha256[i] = static_cast<std::uint8_t>(i);
  }
  dhb::FileOffer offer_out;
  CHECK(dhb::decodeFileOffer(dhb::ByteView(dhb::encodeFileOffer(offer)), offer_out, &err));
  CHECK(offer_out.id == offer.id);
  CHECK(offer_out.size == offer.size);
  CHECK(offer_out.name == offer.name);
  CHECK(offer_out.sha256 == offer.sha256);

  dhb::ClipboardSet clipboard;
  clipboard.text = std::string(1000, 'x');
  dhb::ClipboardSet clipboard_out;
  CHECK(dhb::decodeClipboardSet(dhb::ByteView(dhb::encodeClipboardSet(clipboard)),
                                 clipboard_out, &err));
  CHECK(clipboard_out.text == clipboard.text);

  std::vector<std::uint8_t> trailing = dhb::encodeHello(hello);
  trailing.push_back(0);
  CHECK(!dhb::decodeHello(dhb::ByteView(trailing), hello_out, &err));
}

void testFilenameSafety() {
  CHECK(dhb::sanitizeFilename("../../etc/passwd") == "passwd");
  CHECK(dhb::sanitizeFilename("..\\..\\boot.ini") == "boot.ini");
  CHECK(dhb::sanitizeFilename("..") == "");
  CHECK(dhb::sanitizeFilename("foo/bar.txt") == "bar.txt");
  CHECK(dhb::sanitizeFilename("hello.txt") == "hello.txt");
  CHECK(dhb::sanitizeFilename("a\tb.txt") == "ab.txt");
  CHECK(dhb::sanitizeFilename("中文文件.txt") == "中文文件.txt");
  CHECK(dhb::sanitizeFilename(std::string_view("a\0b", 3)) == "ab");

  const auto dir = makeTempDir("names");
  std::ofstream((dir / "a.txt").string()) << "one";
  const auto unique = dhb::uniqueDestination(dir, "a.txt");
  CHECK(unique.filename().string() == "a (1).txt");
  std::filesystem::remove_all(dir);
}

struct ReceivedFile {
  std::mutex mutex;
  std::condition_variable cv;
  bool ready = false;
  std::filesystem::path path;
};

void testSessionAndFileTransfer() {
  const auto dir = makeTempDir("transfer");
  const auto recv_dir = dir / "recv";
  std::filesystem::create_directories(recv_dir);

  const auto source = dir / "sample.bin";
  const std::vector<std::uint8_t> content = makePattern(1536 * 1024 + 17);
  {
    std::ofstream out(source.string(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(content.data()),
              static_cast<std::streamsize>(content.size()));
  }
  const auto expected_hash = dhb::crypto::sha256(dhb::ByteView(content));

  const auto reply_source = dir / "reply.bin";
  const std::vector<std::uint8_t> reply_content = makePattern(64 * 1024 + 3);
  {
    std::ofstream out(reply_source.string(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(reply_content.data()),
              static_cast<std::streamsize>(reply_content.size()));
  }
  ReceivedFile reply_received;

  auto pair = dhb::makeMemoryTransportPair();

  dhb::SessionConfig server_config;
  server_config.local.id = "server-id";
  server_config.local.name = "Deepin Test";
  server_config.local.os = "Deepin/UOS Linux";
  server_config.local.app_version = "0.1.0";
  server_config.psk = "correct horse battery staple";
  server_config.recv_dir = recv_dir;
  server_config.is_client = false;
  server_config.hello_flags = dhb::kHelloFlagSoftbus4x;

  dhb::Session server(std::move(pair.second), server_config);
  ReceivedFile received;
  std::mutex clipboard_mutex;
  std::condition_variable clipboard_cv;
  std::string clipboard_text;
  server.setFileReceivedHandler([&](const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(received.mutex);
    received.path = path;
    received.ready = true;
    received.cv.notify_all();
  });
  server.setClipboardHandler([&](std::string text) {
    std::lock_guard<std::mutex> lock(clipboard_mutex);
    clipboard_text = std::move(text);
    clipboard_cv.notify_all();
  });

  std::thread server_thread([&] {
    std::string err;
    CHECK(server.handshake(&err));
    CHECK(server.start(&err));
  });

  dhb::SessionConfig client_config = server_config;
  client_config.local.id = "client-id";
  client_config.local.name = "HarmonyOS Test";
  client_config.local.os = "HarmonyOS";
  client_config.is_client = true;
  client_config.hello_flags = dhb::kHelloFlagSoftbus6x;
  dhb::Session client(std::move(pair.first), client_config);
  client.setFileReceivedHandler([&](const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(reply_received.mutex);
    reply_received.path = path;
    reply_received.ready = true;
    reply_received.cv.notify_all();
  });

  std::string err;
  CHECK(client.handshake(&err));
  CHECK(client.start(&err));
  server_thread.join();
  CHECK(client.peerFlags() == dhb::kHelloFlagSoftbus4x);
  CHECK(server.peerFlags() == dhb::kHelloFlagSoftbus6x);

  std::uint64_t rtt = 0;
  CHECK(client.ping(&rtt, &err));

  CHECK(client.sendClipboard("hello from Deepin", &err));
  {
    std::unique_lock<std::mutex> lock(clipboard_mutex);
    CHECK(clipboard_cv.wait_for(lock, std::chrono::seconds(2),
                                [&] { return !clipboard_text.empty(); }));
    CHECK(clipboard_text == "hello from Deepin");
  }

  CHECK(client.sendFile(source, nullptr, &err));
  {
    std::unique_lock<std::mutex> lock(received.mutex);
    CHECK(received.cv.wait_for(lock, std::chrono::seconds(5),
                               [&] { return received.ready; }));
    CHECK(received.ready);
    if (received.ready) {
      std::ifstream input(received.path.string(), std::ios::binary);
      const std::vector<std::uint8_t> saved((std::istreambuf_iterator<char>(input)),
                                            std::istreambuf_iterator<char>());
      CHECK(saved.size() == content.size());
      CHECK(saved == content);
      CHECK(hex32(dhb::crypto::sha256(dhb::ByteView(saved))) == hex32(expected_hash));
    }
  }

  // Reverse direction: the server (Linux) pushes a file to the client (OHOS).
  CHECK(server.sendFile(reply_source, nullptr, &err));
  {
    std::unique_lock<std::mutex> lock(reply_received.mutex);
    CHECK(reply_received.cv.wait_for(lock, std::chrono::seconds(5),
                                     [&] { return reply_received.ready; }));
    CHECK(reply_received.ready);
    if (reply_received.ready) {
      std::ifstream input(reply_received.path.string(), std::ios::binary);
      const std::vector<std::uint8_t> saved((std::istreambuf_iterator<char>(input)),
                                            std::istreambuf_iterator<char>());
      CHECK(saved == reply_content);
    }
  }

  client.close();
  server.close();
  std::filesystem::remove_all(dir);
}

void testResumeFileTransfer() {
  const auto dir = makeTempDir("resume");
  const auto recv_dir = dir / "recv";
  std::filesystem::create_directories(recv_dir);

  const std::vector<std::uint8_t> content = makePattern(700 * 1024 + 13);
  const std::uint64_t resume_at = 200 * 1024 + 7;
  const auto source = dir / "resume.bin";
  {
    std::ofstream out(source.string(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(content.data()),
              static_cast<std::streamsize>(content.size()));
  }

  const auto hash = dhb::crypto::sha256(dhb::ByteView(content));
  const std::string hash_hex = hex32(hash);
  const auto part = recv_dir / (".dhb-" + hash_hex + ".part");
  {
    std::ofstream out(part.string(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(content.data()),
              static_cast<std::streamsize>(resume_at));
  }

  auto pair = dhb::makeMemoryTransportPair();

  dhb::SessionConfig server_config;
  server_config.local.id = "resume-server";
  server_config.local.name = "Deepin Resume Test";
  server_config.local.os = "Deepin/UOS Linux";
  server_config.local.app_version = "0.1.0";
  server_config.psk = "resume test psk";
  server_config.recv_dir = recv_dir;
  server_config.is_client = false;

  dhb::Session server(std::move(pair.second), server_config);
  ReceivedFile received;
  std::atomic<bool> saw_resume{false};
  server.setLogHandler([&](std::string message) {
    if (message.find("resume at " + std::to_string(resume_at)) != std::string::npos) {
      saw_resume.store(true);
    }
  });
  server.setFileReceivedHandler([&](const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(received.mutex);
    received.path = path;
    received.ready = true;
    received.cv.notify_all();
  });

  std::thread server_thread([&] {
    std::string err;
    CHECK(server.handshake(&err));
    CHECK(server.start(&err));
  });

  dhb::SessionConfig client_config = server_config;
  client_config.local.id = "resume-client";
  client_config.local.name = "HarmonyOS Resume Test";
  client_config.local.os = "HarmonyOS";
  client_config.is_client = true;
  dhb::Session client(std::move(pair.first), client_config);

  std::string err;
  CHECK(client.handshake(&err));
  CHECK(client.start(&err));
  server_thread.join();

  CHECK(client.sendFile(source, nullptr, &err));
  {
    std::unique_lock<std::mutex> lock(received.mutex);
    CHECK(received.cv.wait_for(lock, std::chrono::seconds(5),
                               [&] { return received.ready; }));
    CHECK(received.ready);
    if (received.ready) {
      std::ifstream input(received.path.string(), std::ios::binary);
      const std::vector<std::uint8_t> saved((std::istreambuf_iterator<char>(input)),
                                            std::istreambuf_iterator<char>());
      CHECK(saved == content);
    }
  }
  CHECK(saw_resume.load());
  CHECK(!std::filesystem::exists(part));

  client.close();
  server.close();
  std::filesystem::remove_all(dir);
}

#ifndef _WIN32
void testTcpSession() {
  auto dir = makeTempDir("tcp");
  std::string err;
  auto listener = dhb::TcpListener::bind(0, &err);
  CHECK(listener != nullptr);
  if (!listener) {
    std::filesystem::remove_all(dir);
    return;
  }
  const std::uint16_t port = listener->port();

  const auto client_file_source = dir / "tcp-client.bin";
  const auto server_file_source = dir / "tcp-server.bin";
  const auto client_data = makePattern(320 * 1024 + 7);
  const auto server_data = makePattern(64 * 1024 + 11);
  const auto write_file = [](const std::filesystem::path& path,
                             const std::vector<std::uint8_t>& data) {
    std::ofstream out(path.string(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
  };
  const auto read_file = [](const std::filesystem::path& path) {
    std::ifstream input(path.string(), std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)),
                                     std::istreambuf_iterator<char>());
  };
  write_file(client_file_source, client_data);
  write_file(server_file_source, server_data);

  std::mutex mutex;
  std::condition_variable cv;
  bool clipboard_received = false;
  ReceivedFile server_file_received;
  ReceivedFile client_file_received;

  std::thread server_thread([&] {
    std::string server_error;
    auto transport = listener->accept(&server_error);
    CHECK(transport != nullptr);
    if (!transport) return;

    dhb::SessionConfig config;
    config.local.id = "tcp-server";
    config.local.name = "TCP Server";
    config.psk = "tcp-test-key";
    config.recv_dir = dir;
    config.is_client = false;
    dhb::Session server(std::move(transport), config);
    server.setClipboardHandler([&](std::string text) {
      std::lock_guard<std::mutex> lock(mutex);
      clipboard_received = text == "tcp hello";
      cv.notify_all();
    });
    server.setFileReceivedHandler([&](const std::filesystem::path& path) {
      std::lock_guard<std::mutex> lock(server_file_received.mutex);
      server_file_received.path = path;
      server_file_received.ready = true;
      server_file_received.cv.notify_all();
    });
    CHECK(server.handshake(&server_error));
    CHECK(server.start(&server_error));

    {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait_for(lock, std::chrono::seconds(5), [&] { return clipboard_received; });
      CHECK(clipboard_received);
    }
    {
      std::unique_lock<std::mutex> lock(server_file_received.mutex);
      const bool got = server_file_received.cv.wait_for(
          lock, std::chrono::seconds(5), [&] { return server_file_received.ready; });
      CHECK(got);
      if (!got) return;
    }
    CHECK(server.sendFile(server_file_source, nullptr, &server_error));
  });

  auto transport = dhb::TcpTransport::connect("127.0.0.1", port, 5000, &err);
  CHECK(transport != nullptr);
  if (transport) {
    dhb::SessionConfig config;
    config.local.id = "tcp-client";
    config.local.name = "TCP Client";
    config.psk = "tcp-test-key";
    config.recv_dir = dir;
    config.is_client = true;
    dhb::Session client(std::move(transport), config);
    client.setFileReceivedHandler([&](const std::filesystem::path& path) {
      std::lock_guard<std::mutex> lock(client_file_received.mutex);
      client_file_received.path = path;
      client_file_received.ready = true;
      client_file_received.cv.notify_all();
    });
    CHECK(client.handshake(&err));
    CHECK(client.start(&err));
    CHECK(client.sendClipboard("tcp hello", &err));
    CHECK(client.sendFile(client_file_source, nullptr, &err));

    {
      std::unique_lock<std::mutex> lock(server_file_received.mutex);
      CHECK(server_file_received.cv.wait_for(
          lock, std::chrono::seconds(5), [&] { return server_file_received.ready; }));
      CHECK(server_file_received.ready);
      if (server_file_received.ready) {
        CHECK(read_file(server_file_received.path) == client_data);
      }
    }
    {
      std::unique_lock<std::mutex> lock(client_file_received.mutex);
      CHECK(client_file_received.cv.wait_for(
          lock, std::chrono::seconds(5), [&] { return client_file_received.ready; }));
      CHECK(client_file_received.ready);
      if (client_file_received.ready) {
        CHECK(read_file(client_file_received.path) == server_data);
      }
    }
    client.close();
  }

  server_thread.join();
  std::filesystem::remove_all(dir);
}
#endif

void testWrongPsk() {
  auto pair = dhb::makeMemoryTransportPair();
  dhb::SessionConfig server_config;
  server_config.local.id = "server";
  server_config.local.name = "server";
  server_config.psk = "server-key";
  server_config.is_client = false;

  dhb::Session server(std::move(pair.second), server_config);
  std::atomic<bool> server_ok{false};
  std::thread server_thread([&] {
    std::string err;
    server_ok.store(server.handshake(&err));
  });

  dhb::SessionConfig client_config = server_config;
  client_config.local.id = "client";
  client_config.local.name = "client";
  client_config.psk = "client-key";
  client_config.is_client = true;
  dhb::Session client(std::move(pair.first), client_config);

  std::string err;
  const bool client_ok = client.handshake(&err);
  CHECK(!client_ok);
  server_thread.join();
  CHECK(!server_ok.load());
  client.close();
  server.close();
}

}  // namespace

int main() {
  testCrypto();
  testByteIo();
  testFrames();
  testMessages();
  testFilenameSafety();
  testSessionAndFileTransfer();
  testResumeFileTransfer();
#ifndef _WIN32
  testTcpSession();
#endif
  testWrongPsk();

  if (g_failures.load() != 0) {
    std::cerr << g_failures.load() << " check(s) failed\n";
    return 1;
  }
  std::cout << "all DHB tests passed\n";
  return 0;
}
