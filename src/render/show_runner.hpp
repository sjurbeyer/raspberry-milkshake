#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "core/sequencer.hpp"
#include "render/projectm_host.hpp"

namespace rm {

// Where the visuals get their audio from. `start` may be negative or run past the end of the
// content; those samples are silence.
class IPcmSource {
 public:
  virtual ~IPcmSource() = default;
  virtual void read_stereo(SamplePos start, unsigned frames, float* interleaved_out) const = 0;
};

struct RunnerConfig {
  int fps = 60;
  // The screen shows frames later than the speakers play sound (or earlier). A positive lead makes
  // the visuals run ahead of the audio clock by this many samples: cues fire, and audio is analysed,
  // that much earlier so light and sound arrive together. From the A/V calibration screen.
  SamplePos visual_lead = 0;
  unsigned pcm_frames = 512;
  std::filesystem::path preset_root;  // cue preset paths are relative to this
};

// Drives one projectM host from one track's sequencer on a frame-stepped clock. It does not own
// any of them, and does not touch the wall clock: the caller decides when a frame happens.
class ShowRunner {
 public:
  ShowRunner(ProjectMHost& host, Sequencer& sequencer, const IPcmSource& source, RunnerConfig config);

  struct StepResult {
    std::int64_t frame = 0;      // index of the frame just rendered, from 0
    SamplePos media_pos = 0;     // audio position that frame was built for (includes the lead)
    std::size_t presets_loaded = 0;
  };

  // Applies the cues that are due, feeds the audio window ending at the frame's position, renders.
  StepResult step();

  // Jumps to `pos` (audio clock, without the lead): loads the preset that is active there.
  void seek(SamplePos pos);

  const std::vector<std::string>& errors() const { return errors_; }
  const std::string& current_preset() const { return current_preset_; }  // last one that loaded

 private:
  bool load(const LoadPreset& event);

  ProjectMHost& host_;
  Sequencer& sequencer_;
  const IPcmSource& source_;
  RunnerConfig config_;
  std::int64_t next_frame_ = 0;
  std::string current_preset_;
  std::vector<std::string> errors_;
  std::vector<float> pcm_;
};

}  // namespace rm
