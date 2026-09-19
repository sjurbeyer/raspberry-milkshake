#include "render/show_runner.hpp"

namespace rm {

ShowRunner::ShowRunner(ProjectMHost& host, Sequencer& sequencer, const IPcmSource& source, RunnerConfig config)
    : host_(host), sequencer_(sequencer), source_(source), config_(std::move(config)),
      pcm_(static_cast<std::size_t>(config_.pcm_frames) * 2) {}

bool ShowRunner::load(const LoadPreset& event) {
  std::string error;
  if (host_.load_preset((config_.preset_root / event.preset).string(), event.transition, error)) {
    current_preset_ = event.preset;
    return true;
  }
  errors_.push_back(error);
  return false;
}

ShowRunner::StepResult ShowRunner::step() {
  StepResult result;
  result.frame = next_frame_++;
  result.media_pos = result.frame * kSampleRate / config_.fps + config_.visual_lead;

  for (const LoadPreset& event : sequencer_.advance(result.media_pos)) {
    if (load(event)) ++result.presets_loaded;
  }

  source_.read_stereo(result.media_pos - static_cast<SamplePos>(config_.pcm_frames), config_.pcm_frames,
                      pcm_.data());
  host_.add_pcm_stereo(pcm_.data(), config_.pcm_frames);
  host_.render();
  return result;
}

void ShowRunner::seek(SamplePos pos) {
  next_frame_ = pos * config_.fps / kSampleRate;
  if (const auto active = sequencer_.seek(pos + config_.visual_lead)) {
    LoadPreset cut = *active;
    cut.transition = Transition{Transition::Kind::Cut, 0.0};
    load(cut);
  }
}

}  // namespace rm
