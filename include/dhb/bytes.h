// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dhb {

// A non-owning view over bytes. The pointed-to memory must outlive the view.
struct ByteView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0;

  ByteView() = default;
  ByteView(const std::uint8_t* d, std::size_t s) : data(d), size(s) {}
  ByteView(const std::vector<std::uint8_t>& v) : data(v.data()), size(v.size()) {}
  ByteView(std::string_view s)
      : data(reinterpret_cast<const std::uint8_t*>(s.data())), size(s.size()) {}

  bool empty() const { return size == 0; }
};

// Little-endian writer used by every DHB control message. All multi-byte
// integers are explicitly encoded so the wire format does not depend on the
// host ABI or endianness.
class ByteWriter {
 public:
  void u8(std::uint8_t v) { buf_.push_back(v); }
  void u16(std::uint16_t v) {
    u8(static_cast<std::uint8_t>(v & 0xffu));
    u8(static_cast<std::uint8_t>((v >> 8) & 0xffu));
  }
  void u32(std::uint32_t v) {
    u8(static_cast<std::uint8_t>(v & 0xffu));
    u8(static_cast<std::uint8_t>((v >> 8) & 0xffu));
    u8(static_cast<std::uint8_t>((v >> 16) & 0xffu));
    u8(static_cast<std::uint8_t>((v >> 24) & 0xffu));
  }
  void u64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
      u8(static_cast<std::uint8_t>((v >> (8 * i)) & 0xffu));
    }
  }
  void bytes(const std::uint8_t* data, std::size_t size) {
    if (size == 0) return;
    buf_.insert(buf_.end(), data, data + size);
  }
  void bytes(ByteView v) { bytes(v.data, v.size); }
  // Length-prefixed UTF-8 string (u16 length). Rejects oversized input.
  void str(std::string_view s, std::size_t max_len = 4096) {
    if (s.size() > max_len) s = s.substr(0, max_len);
    u16(static_cast<std::uint16_t>(s.size()));
    bytes(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
  }

  const std::vector<std::uint8_t>& data() const { return buf_; }
  std::vector<std::uint8_t> take() { return std::move(buf_); }
  std::size_t size() const { return buf_.size(); }

 private:
  std::vector<std::uint8_t> buf_;
};

// Bounds-checked reader. Every accessor returns false instead of reading past
// the end of the buffer; callers must treat that as a protocol error.
class ByteReader {
 public:
  explicit ByteReader(ByteView data) : data_(data) {}

  bool u8(std::uint8_t& v) {
    if (remaining() < 1) return false;
    v = data_.data[pos_++];
    return true;
  }
  bool u16(std::uint16_t& v) {
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    if (!u8(a) || !u8(b)) return false;
    v = static_cast<std::uint16_t>(a | (static_cast<std::uint16_t>(b) << 8));
    return true;
  }
  bool u32(std::uint32_t& v) {
    std::uint8_t b[4] = {0, 0, 0, 0};
    if (!bytes(b, 4)) return false;
    v = static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8) |
        (static_cast<std::uint32_t>(b[2]) << 16) |
        (static_cast<std::uint32_t>(b[3]) << 24);
    return true;
  }
  bool u64(std::uint64_t& v) {
    std::uint8_t b[8] = {0};
    if (!bytes(b, 8)) return false;
    v = 0;
    for (int i = 7; i >= 0; --i) {
      v = (v << 8) | b[i];
    }
    return true;
  }
  bool bytes(std::uint8_t* out, std::size_t size) {
    if (remaining() < size) return false;
    if (size != 0) {
      for (std::size_t i = 0; i < size; ++i) out[i] = data_.data[pos_ + i];
    }
    pos_ += size;
    return true;
  }
  bool skip(std::size_t size) {
    if (remaining() < size) return false;
    pos_ += size;
    return true;
  }
  bool str(std::string& out, std::size_t max_len = 4096) {
    std::uint16_t len = 0;
    if (!u16(len)) return false;
    if (len > max_len || remaining() < len) return false;
    out.assign(reinterpret_cast<const char*>(data_.data + pos_), len);
    pos_ += len;
    return true;
  }

  std::size_t remaining() const { return data_.size - pos_; }
  bool done() const { return pos_ == data_.size; }
  ByteView rest() const { return ByteView(data_.data + pos_, data_.size - pos_); }

 private:
  ByteView data_;
  std::size_t pos_ = 0;
};

}  // namespace dhb
