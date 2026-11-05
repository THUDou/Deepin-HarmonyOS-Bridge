// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#include "dhb/crypto.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <random>

#ifdef __linux__
#include <sys/random.h>
#include <sys/types.h>
#endif

namespace dhb::crypto {
namespace {

constexpr std::uint32_t kSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

}  // namespace

Sha256::Sha256() { reset(); }

void Sha256::reset() {
  state_[0] = 0x6a09e667u;
  state_[1] = 0xbb67ae85u;
  state_[2] = 0x3c6ef372u;
  state_[3] = 0xa54ff53au;
  state_[4] = 0x510e527fu;
  state_[5] = 0x9b05688cu;
  state_[6] = 0x1f83d9abu;
  state_[7] = 0x5be0cd19u;
  bit_len_ = 0;
  buffer_len_ = 0;
}

void Sha256::transform(const std::uint8_t block[64]) {
  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
           (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
           static_cast<std::uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  std::uint32_t a = state_[0];
  std::uint32_t b = state_[1];
  std::uint32_t c = state_[2];
  std::uint32_t d = state_[3];
  std::uint32_t e = state_[4];
  std::uint32_t f = state_[5];
  std::uint32_t g = state_[6];
  std::uint32_t h = state_[7];

  for (int i = 0; i < 64; ++i) {
    const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const std::uint32_t ch = (e & f) ^ ((~e) & g);
    const std::uint32_t t1 = h + s1 + ch + kSha256K[i] + w[i];
    const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t t2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  state_[0] += a;
  state_[1] += b;
  state_[2] += c;
  state_[3] += d;
  state_[4] += e;
  state_[5] += f;
  state_[6] += g;
  state_[7] += h;
}

void Sha256::update(ByteView data) {
  const std::uint8_t* p = data.data;
  std::size_t n = data.size;
  bit_len_ += static_cast<std::uint64_t>(n) * 8u;

  if (buffer_len_ != 0) {
    const std::size_t need = 64 - buffer_len_;
    const std::size_t take = std::min(need, n);
    if (take != 0) std::memcpy(buffer_ + buffer_len_, p, take);
    buffer_len_ += take;
    p += take;
    n -= take;
    if (buffer_len_ == 64) {
      transform(buffer_);
      buffer_len_ = 0;
    }
  }

  while (n >= 64) {
    transform(p);
    p += 64;
    n -= 64;
  }

  if (n != 0) {
    std::memcpy(buffer_, p, n);
    buffer_len_ = n;
  }
}

std::array<std::uint8_t, 32> Sha256::finish() {
  const std::uint64_t bits = bit_len_;
  std::uint8_t pad[64] = {0};
  const std::size_t pad_len =
      (buffer_len_ < 56) ? (56 - buffer_len_) : (120 - buffer_len_);
  pad[0] = 0x80;
  update(ByteView(pad, pad_len));

  std::uint8_t length_bytes[8];
  for (int i = 0; i < 8; ++i) {
    length_bytes[7 - i] = static_cast<std::uint8_t>((bits >> (8 * i)) & 0xffu);
  }
  update(ByteView(length_bytes, 8));

  std::array<std::uint8_t, 32> out{};
  for (int i = 0; i < 8; ++i) {
    out[i * 4] = static_cast<std::uint8_t>((state_[i] >> 24) & 0xffu);
    out[i * 4 + 1] = static_cast<std::uint8_t>((state_[i] >> 16) & 0xffu);
    out[i * 4 + 2] = static_cast<std::uint8_t>((state_[i] >> 8) & 0xffu);
    out[i * 4 + 3] = static_cast<std::uint8_t>(state_[i] & 0xffu);
  }
  reset();
  return out;
}

std::array<std::uint8_t, 32> sha256(ByteView data) {
  Sha256 hasher;
  hasher.update(data);
  return hasher.finish();
}

std::array<std::uint8_t, 32> hmacSha256(ByteView key, ByteView data) {
  std::uint8_t normalized_key[64] = {0};
  if (key.size > 64) {
    const auto hashed = sha256(key);
    std::memcpy(normalized_key, hashed.data(), hashed.size());
  } else if (key.size != 0) {
    std::memcpy(normalized_key, key.data, key.size);
  }

  std::uint8_t inner_pad[64];
  std::uint8_t outer_pad[64];
  for (int i = 0; i < 64; ++i) {
    inner_pad[i] = static_cast<std::uint8_t>(normalized_key[i] ^ 0x36u);
    outer_pad[i] = static_cast<std::uint8_t>(normalized_key[i] ^ 0x5cu);
  }

  Sha256 inner;
  inner.update(ByteView(inner_pad, 64));
  inner.update(data);
  const auto inner_hash = inner.finish();

  Sha256 outer;
  outer.update(ByteView(outer_pad, 64));
  outer.update(ByteView(inner_hash.data(), inner_hash.size()));
  return outer.finish();
}

std::array<std::uint8_t, 32> hkdfSha256(ByteView ikm, ByteView salt, ByteView info) {
  const auto prk = hmacSha256(salt, ikm);
  std::vector<std::uint8_t> t;
  t.reserve(info.size + 1);
  t.insert(t.end(), info.data, info.data + info.size);
  t.push_back(0x01);
  return hmacSha256(ByteView(prk.data(), prk.size()), ByteView(t.data(), t.size()));
}

std::uint32_t crc32c(ByteView data) {
  static const std::array<std::uint32_t, 256> table = []() {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1u) ? (0x82f63b78u ^ (c >> 1)) : (c >> 1);
      }
      t[i] = c;
    }
    return t;
  }();

  std::uint32_t crc = 0xffffffffu;
  for (std::size_t i = 0; i < data.size; ++i) {
    crc = table[(crc ^ data.data[i]) & 0xffu] ^ (crc >> 8);
  }
  return ~crc;
}

bool constantTimeEqual(ByteView a, ByteView b) {
  if (a.size != b.size) return false;
  std::uint8_t diff = 0;
  for (std::size_t i = 0; i < a.size; ++i) {
    diff = static_cast<std::uint8_t>(diff | (a.data[i] ^ b.data[i]));
  }
  return diff == 0;
}

bool fillRandom(std::uint8_t* out, std::size_t size) {
  if (size == 0) return true;
#ifdef __linux__
  std::size_t offset = 0;
  while (offset < size) {
    const ssize_t n = getrandom(out + offset, size - offset, 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      break;
    }
    offset += static_cast<std::size_t>(n);
  }
  if (offset == size) return true;
#endif
  std::random_device rd;
  for (std::size_t i = 0; i < size; ++i) {
    out[i] = static_cast<std::uint8_t>(rd());
  }
  return true;
}

std::array<std::uint8_t, 32> randomBytes32() {
  std::array<std::uint8_t, 32> out{};
  fillRandom(out.data(), out.size());
  return out;
}

std::string toHex(ByteView data) {
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(data.size * 2);
  for (std::size_t i = 0; i < data.size; ++i) {
    out[i * 2] = kHex[(data.data[i] >> 4) & 0x0f];
    out[i * 2 + 1] = kHex[data.data[i] & 0x0f];
  }
  return out;
}

bool fromHex(std::string_view text, std::vector<std::uint8_t>& out) {
  if (text.size() % 2 != 0) return false;
  out.clear();
  out.reserve(text.size() / 2);
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (std::size_t i = 0; i < text.size(); i += 2) {
    const int hi = nibble(text[i]);
    const int lo = nibble(text[i + 1]);
    if (hi < 0 || lo < 0) return false;
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return true;
}

}  // namespace dhb::crypto
