#include "render/overlay.hpp"

#include <GLES3/gl3.h>

#include <algorithm>

namespace rm {
namespace {

const char* const kVertexShader = R"(#version 300 es
uniform vec4 rect;  // x0 y0 x1 y1 in 0..1, origin bottom left
out vec2 uv;
void main() {
  vec2 p = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
  uv = p;
  gl_Position = vec4(mix(rect.xy, rect.zw, p) * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char* const kFragmentShader = R"(#version 300 es
precision mediump float;
uniform sampler2D tex;
uniform vec4 color;
uniform float use_tex;
in vec2 uv;
out vec4 frag;
void main() {
  vec4 c = use_tex > 0.5 ? texture(tex, uv) : vec4(1.0);
  frag = c * color;
}
)";

GLuint compile(GLenum type, const char* source, std::string& error) {
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512] = {};
    glGetShaderInfoLog(shader, sizeof log, nullptr, log);
    error = std::string("overlay shader: ") + log;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

std::unique_ptr<Overlay> Overlay::create(int width, int height, std::string& error) {
  const GLuint vertex = compile(GL_VERTEX_SHADER, kVertexShader, error);
  const GLuint fragment = vertex ? compile(GL_FRAGMENT_SHADER, kFragmentShader, error) : 0;
  if (!vertex || !fragment) {
    if (vertex) glDeleteShader(vertex);
    return nullptr;
  }
  const GLuint program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, fragment);
  glLinkProgram(program);
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (!linked) {
    error = "overlay program did not link";
    glDeleteProgram(program);
    return nullptr;
  }

  std::unique_ptr<Overlay> overlay(new Overlay());
  overlay->width_ = width;
  overlay->height_ = height;
  overlay->program_ = program;
  overlay->rect_location_ = glGetUniformLocation(program, "rect");
  overlay->color_location_ = glGetUniformLocation(program, "color");
  overlay->use_texture_location_ = glGetUniformLocation(program, "use_tex");

  glGenTextures(1, &overlay->texture_);
  glBindTexture(GL_TEXTURE_2D, overlay->texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  glGenVertexArrays(1, &overlay->vao_);
  return overlay;
}

Overlay::~Overlay() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteTextures(1, &texture_);
  glDeleteProgram(program_);
}

void Overlay::capture() {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width_, height_);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void Overlay::draw_rect(float x0, float y0, float x1, float y1, const float color[4], bool textured, bool blend) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width_, height_);
  glUseProgram(program_);
  glUniform4f(rect_location_, x0, y0, x1, y1);
  glUniform4fv(color_location_, 1, color);
  glUniform1f(use_texture_location_, textured ? 1.0f : 0.0f);
  if (textured) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glUniform1i(glGetUniformLocation(program_, "tex"), 0);
  }
  if (blend) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  } else {
    glDisable(GL_BLEND);
  }
  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Leave things as projectM expects to find them.
  glBindVertexArray(0);
  glDisable(GL_BLEND);
  glBindTexture(GL_TEXTURE_2D, 0);
  glUseProgram(0);
}

void Overlay::draw_captured(float brightness) {
  const float b = std::clamp(brightness, 0.0f, 1.0f);
  glClearColor(0, 0, 0, 1);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  const float color[4] = {b, b, b, 1.0f};
  draw_rect(0, 0, 1, 1, color, true, false);
}

void Overlay::draw_black(float alpha) {
  const float color[4] = {0.0f, 0.0f, 0.0f, std::clamp(alpha, 0.0f, 1.0f)};
  draw_rect(0, 0, 1, 1, color, false, true);
}

void Overlay::draw_pause_icon() {
  const float w = static_cast<float>(width_);
  const float h = static_cast<float>(height_);
  const float m = std::min(w, h);
  const float bar_w = 0.05f * m;
  const float bar_h = 0.20f * m;
  const float gap = 0.05f * m;
  const float y0 = 0.5f - bar_h / 2.0f / h;
  const float y1 = 0.5f + bar_h / 2.0f / h;
  const float soft_white[4] = {0.949f, 0.953f, 0.961f, 1.0f};  // the brand's soft white, #F2F3F5
  draw_rect(0.5f - (gap / 2.0f + bar_w) / w, y0, 0.5f - (gap / 2.0f) / w, y1, soft_white, false, true);
  draw_rect(0.5f + (gap / 2.0f) / w, y0, 0.5f + (gap / 2.0f + bar_w) / w, y1, soft_white, false, true);
}

}  // namespace rm
