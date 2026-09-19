#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "core/clock.hpp"
#include "core/manifest.hpp"
#include "core/sequencer.hpp"
#include "render/projectm_host.hpp"

namespace rm {

enum class EngineMode { Idle, Previewing, Playing };

struct EngineStatus {
  EngineMode mode = EngineMode::Idle;
  std::string id;      // collection folder name, empty when idle
  std::string title;
  int track = 0;       // 1-based, 0 when idle
  int track_count = 0;
  std::string preset;  // the last preset that loaded
  double media_seconds = 0;
  std::size_t error_count = 0;
  std::string last_error;
};

struct EngineConfig {
  int fps = 30;
  std::filesystem::path preset_root;
  // Until real audio decoding exists a track has no known length: every track plays this long.
  SamplePos dev_track_length = 60 * kSampleRate;
  // After the last track ends the visuals carry on for this many frames, so a queued collection
  // that starts right away does not flash the idle screen in between.
  int finish_grace_frames = 15;
};

// Decides what the main output shows: idle, a looping preview, or a playing collection (tracks in
// order, cues from the sequencer, synthetic audio for now). Commands may come from any thread; they
// are applied by tick(), which must be called on the thread that owns the projectM context, once per
// frame. Time is the frame count, never the wall clock, so it is deterministic to test.
class Engine {
 public:
  Engine(ProjectMHost& host, const IPresetCatalog& catalog, EngineConfig config);
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // Ignored while a collection is playing: previews only happen when idle.
  void preview(std::string id, Collection collection);
  void stop_preview();
  // Interrupts whatever is running.
  void play(std::string id, Collection collection);
  void stop();
  // Re-plans the current track from the catalog as it is now, carrying on from the same position.
  // Use after a rating change: pool choices are made up front, so they must be redone.
  void refresh_schedule();

  // Called on the render thread when the last track of a playing collection ends.
  void set_finished_callback(std::function<void()> callback);

  void tick();
  EngineStatus status() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rm
