#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/manifest.hpp"

namespace rm {

struct HostConfig {
  int width = 640;
  int height = 360;
  int mesh_width = 32;
  int mesh_height = 24;
  int fps = 60;
};

struct Frame {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;  // top row first
};

struct ProjectMHostImpl;

// One projectM instance. projectM 4.1 always draws into the default framebuffer, so the current
// OpenGL ES 3 context's surface must be `config.width` x `config.height`. That context must exist on
// the calling thread for the whole lifetime of the host, and all calls must come from that thread.
// projectM never switches presets by itself here: the sequencer decides.
class ProjectMHost {
 public:
  static std::unique_ptr<ProjectMHost> create(const HostConfig& config, std::string& error);
  ~ProjectMHost();
  ProjectMHost(const ProjectMHost&) = delete;
  ProjectMHost& operator=(const ProjectMHost&) = delete;

  // Returns false and fills `error` if the preset could not be loaded; the previous preset stays.
  bool load_preset(const std::string& path, const Transition& transition, std::string& error);

  // Interleaved stereo float samples in [-1, 1].
  void add_pcm_stereo(const float* interleaved, unsigned frames);

  void render();
  Frame read_frame() const;

  const HostConfig& config() const { return config_; }

 private:
  ProjectMHost();

  std::unique_ptr<ProjectMHostImpl> impl_;
  HostConfig config_;
};

}  // namespace rm
