#pragma once

#include <memory>
#include <string>

namespace rm {

// Small full-screen passes drawn on top of what projectM drew into the default framebuffer: holding
// and dimming a picture (the fade to black while paused), a black veil (the fade back in) and the
// pause icon. Needs the same current OpenGL ES 3 context as the ProjectMHost; every call leaves the
// GL state the way projectM expects to find it.
class Overlay {
 public:
  // Fails (null, with `error`) if the shaders do not compile.
  static std::unique_ptr<Overlay> create(int width, int height, std::string& error);
  ~Overlay();
  Overlay(const Overlay&) = delete;
  Overlay& operator=(const Overlay&) = delete;

  // Copies the current framebuffer into a held texture.
  void capture();
  // Clears to black, then draws the held picture at the given brightness (1 = as captured).
  void draw_captured(float brightness);
  // Draws black over what is there; alpha 0 leaves it alone, 1 hides it.
  void draw_black(float alpha);
  // Two soft-white bars in the middle of the picture.
  void draw_pause_icon();

 private:
  Overlay() = default;
  void draw_rect(float x0, float y0, float x1, float y1, const float color[4], bool textured, bool blend);

  int width_ = 0;
  int height_ = 0;
  unsigned program_ = 0;
  unsigned texture_ = 0;
  unsigned vao_ = 0;
  int rect_location_ = -1;
  int color_location_ = -1;
  int use_texture_location_ = -1;
};

}  // namespace rm
