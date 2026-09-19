#include "render/jpeg.hpp"

// Static: projectM's bundled SOIL2 already exports its own copy of these stb symbols.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace rm {

std::vector<std::uint8_t> encode_jpeg(const Frame& frame, int quality) {
  std::vector<std::uint8_t> out;
  if (frame.width <= 0 || frame.height <= 0 ||
      frame.rgba.size() != static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4) {
    return out;
  }
  const auto append = [](void* context, void* data, int size) {
    auto* bytes = static_cast<std::vector<std::uint8_t>*>(context);
    const auto* p = static_cast<const std::uint8_t*>(data);
    bytes->insert(bytes->end(), p, p + size);
  };
  if (!stbi_write_jpg_to_func(append, &out, frame.width, frame.height, 4, frame.rgba.data(), quality)) out.clear();
  return out;
}

}  // namespace rm
