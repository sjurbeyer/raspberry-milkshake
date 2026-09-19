#include "render/projectm_host.hpp"

#include <GLES3/gl3.h>
#include <projectM-4/projectM.h>

#include <algorithm>

namespace rm {

struct ProjectMHostImpl {
  projectm_handle projectm = nullptr;
  std::string last_failure;  // set by the projectM failure callback during load_preset

  ~ProjectMHostImpl() {
    if (projectm) projectm_destroy(projectm);
  }
};

namespace {

void on_preset_failed(const char* filename, const char* message, void* user_data) {
  auto* impl = static_cast<ProjectMHostImpl*>(user_data);
  impl->last_failure = std::string(filename ? filename : "?") + ": " + (message ? message : "unknown error");
}

}  // namespace

ProjectMHost::ProjectMHost() = default;

std::unique_ptr<ProjectMHost> ProjectMHost::create(const HostConfig& config, std::string& error) {
  std::unique_ptr<ProjectMHost> host(new ProjectMHost());
  host->config_ = config;
  host->impl_ = std::make_unique<ProjectMHostImpl>();
  ProjectMHostImpl& impl = *host->impl_;

  impl.projectm = projectm_create();
  if (!impl.projectm) {
    error = "projectm_create failed (no usable OpenGL ES 3 context?)";
    return nullptr;
  }
  projectm_set_window_size(impl.projectm, static_cast<size_t>(config.width), static_cast<size_t>(config.height));
  projectm_set_mesh_size(impl.projectm, static_cast<size_t>(config.mesh_width), static_cast<size_t>(config.mesh_height));
  projectm_set_fps(impl.projectm, config.fps);
  projectm_set_hard_cut_enabled(impl.projectm, false);
  projectm_set_preset_locked(impl.projectm, true);
  projectm_set_preset_switch_failed_event_callback(impl.projectm, &on_preset_failed, &impl);
  return host;
}

ProjectMHost::~ProjectMHost() = default;

bool ProjectMHost::load_preset(const std::string& path, const Transition& transition, std::string& error) {
  ProjectMHostImpl& impl = *impl_;
  impl.last_failure.clear();
  const bool blend = transition.kind == Transition::Kind::Blend;
  if (blend) projectm_set_soft_cut_duration(impl.projectm, transition.seconds);
  projectm_load_preset_file(impl.projectm, path.c_str(), blend);
  if (!impl.last_failure.empty()) {
    error = impl.last_failure;
    return false;
  }
  return true;
}

void ProjectMHost::add_pcm_stereo(const float* interleaved, unsigned frames) {
  projectm_pcm_add_float(impl_->projectm, interleaved, frames, PROJECTM_STEREO);
}

void ProjectMHost::render() {
  // projectM 4.1 always draws into the default framebuffer (the context's surface).
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, config_.width, config_.height);
  projectm_opengl_render_frame(impl_->projectm);
  glFinish();
}

Frame ProjectMHost::read_frame() const {
  Frame frame;
  frame.width = config_.width;
  frame.height = config_.height;
  const std::size_t stride = static_cast<std::size_t>(config_.width) * 4;
  std::vector<std::uint8_t> bottom_up(stride * static_cast<std::size_t>(config_.height));
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, config_.width, config_.height, GL_RGBA, GL_UNSIGNED_BYTE, bottom_up.data());

  frame.rgba.resize(bottom_up.size());
  for (int y = 0; y < config_.height; ++y) {
    const auto src = bottom_up.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(config_.height - 1 - y) * stride);
    std::copy(src, src + static_cast<std::ptrdiff_t>(stride),
              frame.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) * stride));
  }
  return frame;
}

}  // namespace rm
