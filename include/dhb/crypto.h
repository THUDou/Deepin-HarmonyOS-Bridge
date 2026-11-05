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

namespace dhb::crypto {

// Incremental SHA-256. Used both for the pairing proof and for streaming file
// hashes (so a 20 GiB file never has to be held in memory).
class Sha256 {
 public:
  Sha256();
  void update(ByteView data);
  std::array<std::uint8_t, 32> finish();

 private:
  void reset();
  void transform(const std::uint8_t block[64]);

  std::uint32_t state_[8];
  std::uint64_t bit_len_;
  std::uint8_t buffer_[64];
  std::size_t buffer_len_;
};

std::array<std::uint8_t, 32> sha256(ByteView data);
std::array<std::uint8_t, 32> hmacSha256(ByteView key, ByteView data);
// HKDF-SHA256 (RFC 5869), 32-byte output. Good enough for the DHB session-key
// derivation; the transport is still responsible for confidentiality.
std::array<std::uint8_t, 32> hkdfSha256(ByteView ikm, ByteView salt, ByteView info);

// CRC-32C (Castagnoli), used to reject corrupted frames early.
std::uint32_t crc32c(ByteView data);

bool constantTimeEqual(ByteView a, ByteView b);
bool fillRandom(std::uint8_t* out, std::size_t size);
std::array<std::uint8_t, 32> randomBytes32();

std::string toHex(ByteView data);
bool fromHex(std::string_view text, std::vector<std::uint8_t>& out);

}  // namespace dhb::crypto
