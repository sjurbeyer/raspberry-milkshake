#include "render/egl_context.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

namespace rm {

std::unique_ptr<EglContext> EglContext::create_headless(int width, int height, std::string& error) {
  const auto get_platform_display =
      reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
  EGLDisplay display = get_platform_display
                           ? get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                           : eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) {
    error = "no EGL display available";
    return nullptr;
  }
  EGLint major = 0;
  EGLint minor = 0;
  if (!eglInitialize(display, &major, &minor)) {
    error = "eglInitialize failed";
    return nullptr;
  }
  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    error = "OpenGL ES API not available";
    eglTerminate(display);
    return nullptr;
  }

  const EGLint config_attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                                   EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
                                   EGL_RED_SIZE,        8,
                                   EGL_GREEN_SIZE,      8,
                                   EGL_BLUE_SIZE,       8,
                                   EGL_ALPHA_SIZE,      8,
                                   EGL_NONE};
  EGLConfig config = nullptr;
  EGLint config_count = 0;
  if (!eglChooseConfig(display, config_attribs, &config, 1, &config_count) || config_count < 1) {
    error = "no OpenGL ES 3 EGL config";
    eglTerminate(display);
    return nullptr;
  }

  const EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
  EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
  if (context == EGL_NO_CONTEXT) {
    error = "eglCreateContext failed";
    eglTerminate(display);
    return nullptr;
  }
  const EGLint surface_attribs[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
  EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attribs);
  if (surface == EGL_NO_SURFACE) {
    error = "eglCreatePbufferSurface failed";
    eglDestroyContext(display, context);
    eglTerminate(display);
    return nullptr;
  }
  if (!eglMakeCurrent(display, surface, surface, context)) {
    error = "eglMakeCurrent failed";
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    return nullptr;
  }

  std::unique_ptr<EglContext> result(new EglContext());
  result->display_ = display;
  result->context_ = context;
  result->surface_ = surface;
  return result;
}

EglContext::~EglContext() {
  auto display = static_cast<EGLDisplay>(display_);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(display, static_cast<EGLSurface>(surface_));
  eglDestroyContext(display, static_cast<EGLContext>(context_));
  eglTerminate(display);
}

std::string EglContext::renderer() const {
  const auto* s = glGetString(GL_RENDERER);
  return s ? reinterpret_cast<const char*>(s) : "";
}

std::string EglContext::version() const {
  const auto* s = glGetString(GL_VERSION);
  return s ? reinterpret_cast<const char*>(s) : "";
}

}  // namespace rm
