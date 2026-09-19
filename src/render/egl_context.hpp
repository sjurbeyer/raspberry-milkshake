#pragma once

#include <memory>
#include <string>

namespace rm {

// An OpenGL ES 3 context with an offscreen pbuffer as its default framebuffer (no window, no display
// server). projectM 4.1 always draws into framebuffer 0, so this pbuffer is what it renders to.
// Used by tests, the preset bench and the dev container, where Mesa's llvmpipe stands in for the
// Pi's GPU.
class EglContext {
 public:
  // Creates the context and makes it current on the calling thread. On failure returns null and
  // fills `error`.
  static std::unique_ptr<EglContext> create_headless(int width, int height, std::string& error);
  ~EglContext();
  EglContext(const EglContext&) = delete;
  EglContext& operator=(const EglContext&) = delete;

  std::string renderer() const;  // GL_RENDERER, e.g. "llvmpipe (LLVM 19.1.7, 256 bits)"
  std::string version() const;   // GL_VERSION

 private:
  EglContext() = default;

  void* display_ = nullptr;  // EGLDisplay
  void* context_ = nullptr;  // EGLContext
  void* surface_ = nullptr;  // EGLSurface (pbuffer)
};

}  // namespace rm
