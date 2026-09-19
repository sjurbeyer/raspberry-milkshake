#include "render/downscaler.hpp"

#include <GLES3/gl3.h>

#include <algorithm>

namespace rm {

Downscaler::Downscaler(int width, int height) : width_(width), height_(height) {
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Downscaler::~Downscaler() {
  glDeleteFramebuffers(1, &fbo_);
  glDeleteTextures(1, &texture_);
}

Frame Downscaler::capture(int source_width, int source_height) {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_);
  glBlitFramebuffer(0, 0, source_width, source_height, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_LINEAR);

  const std::size_t stride = static_cast<std::size_t>(width_) * 4;
  std::vector<std::uint8_t> bottom_up(stride * static_cast<std::size_t>(height_));
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, bottom_up.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  Frame frame;
  frame.width = width_;
  frame.height = height_;
  frame.rgba.resize(bottom_up.size());
  for (int y = 0; y < height_; ++y) {
    const auto src = bottom_up.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(height_ - 1 - y) * stride);
    std::copy(src, src + static_cast<std::ptrdiff_t>(stride),
              frame.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) * stride));
  }
  return frame;
}

}  // namespace rm
