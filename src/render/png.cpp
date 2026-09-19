#include "render/png.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <vector>

namespace rm {
namespace {

std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0) {
  static const auto table = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t n = 0; n < 256; ++n) {
      std::uint32_t c = n;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320U ^ (c >> 1) : c >> 1;
      t[n] = c;
    }
    return t;
  }();
  crc = ~crc;
  for (std::size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(v >> shift));
}

void write_chunk(std::ofstream& file, const char type[4], const std::vector<std::uint8_t>& data) {
  std::vector<std::uint8_t> chunk;
  put_u32(chunk, static_cast<std::uint32_t>(data.size()));
  chunk.insert(chunk.end(), type, type + 4);
  chunk.insert(chunk.end(), data.begin(), data.end());
  // CRC covers the type and data, not the length.
  put_u32(chunk, crc32(chunk.data() + 4, chunk.size() - 4));
  file.write(reinterpret_cast<const char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
}

// zlib stream made of stored (uncompressed) deflate blocks.
std::vector<std::uint8_t> zlib_store(const std::vector<std::uint8_t>& raw) {
  std::vector<std::uint8_t> out = {0x78, 0x01};
  std::uint32_t a = 1;
  std::uint32_t b = 0;
  for (const std::uint8_t byte : raw) {
    a = (a + byte) % 65521;
    b = (b + a) % 65521;
  }
  std::size_t pos = 0;
  do {
    const std::size_t n = std::min<std::size_t>(65535, raw.size() - pos);
    const bool last = pos + n >= raw.size();
    out.push_back(last ? 1 : 0);
    out.push_back(static_cast<std::uint8_t>(n & 0xFF));
    out.push_back(static_cast<std::uint8_t>(n >> 8));
    out.push_back(static_cast<std::uint8_t>(~n & 0xFF));
    out.push_back(static_cast<std::uint8_t>((~n >> 8) & 0xFF));
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
               raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
    pos += n;
  } while (pos < raw.size());
  put_u32(out, (b << 16) | a);
  return out;
}

}  // namespace

bool write_png(const std::string& path, const Frame& frame) {
  if (frame.width <= 0 || frame.height <= 0 ||
      frame.rgba.size() != static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4) {
    return false;
  }
  std::ofstream file(path, std::ios::binary);
  if (!file) return false;

  const std::uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  file.write(reinterpret_cast<const char*>(signature), sizeof signature);

  std::vector<std::uint8_t> header;
  put_u32(header, static_cast<std::uint32_t>(frame.width));
  put_u32(header, static_cast<std::uint32_t>(frame.height));
  header.insert(header.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA, no interlace
  write_chunk(file, "IHDR", header);

  const std::size_t stride = static_cast<std::size_t>(frame.width) * 4;
  std::vector<std::uint8_t> raw;
  raw.reserve((stride + 1) * static_cast<std::size_t>(frame.height));
  for (int y = 0; y < frame.height; ++y) {
    raw.push_back(0);  // filter: none
    const auto row = frame.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) * stride);
    raw.insert(raw.end(), row, row + static_cast<std::ptrdiff_t>(stride));
  }
  write_chunk(file, "IDAT", zlib_store(raw));
  write_chunk(file, "IEND", {});
  return file.good();
}

}  // namespace rm
