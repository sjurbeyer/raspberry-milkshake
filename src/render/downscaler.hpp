#pragma once

#include "core/frame.hpp"

namespace rm {

// GPU-side downscale of the current default framebuffer into a small image (for the LED sink): one
// linear `glBlitFramebuffer` into a tiny FBO, then a read of only that tiny image. The picture is
// never evaluated a second time. Needs the same current context as the ProjectMHost.
class Downscaler {
 public:
  Downscaler(int width, int height);
  ~Downscaler();
  Downscaler(const Downscaler&) = delete;
  Downscaler& operator=(const Downscaler&) = delete;

  // Call after ProjectMHost::render() and before the frame is presented or replaced.
  Frame capture(int source_width, int source_height);

 private:
  int width_;
  int height_;
  unsigned texture_ = 0;
  unsigned fbo_ = 0;
};

}  // namespace rm
