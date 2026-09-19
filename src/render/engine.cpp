#include "render/engine.hpp"

#include <algorithm>
#include <mutex>
#include <optional>
#include <vector>

#include "core/cue_time.hpp"
#include "core/demo_audio.hpp"
#include "render/show_runner.hpp"

namespace rm {
namespace {

constexpr Transition kCut{Transition::Kind::Cut, 0.0};
constexpr SamplePos kEndless = 24LL * 3600 * kSampleRate;
constexpr double kDefaultBpm = 120.0;
constexpr SamplePos kDefaultPreviewLength = 20 * kSampleRate;

class SyntheticSource final : public IPcmSource {
 public:
  explicit SyntheticSource(double bpm) : music_(bpm > 0 ? bpm : kDefaultBpm) {}
  void read_stereo(SamplePos start, unsigned frames, float* out) const override {
    music_.read_stereo(start, frames, out);
  }

 private:
  SyntheticMusic music_;
};

// One track that shows a single preset for as long as it runs.
Collection single_preset_show(const Collection& base, const std::string& preset, double bpm) {
  Collection show = base;
  show.type = CollectionType::Album;
  Track track;
  track.file = "(synthetic)";
  track.grid.bpm = bpm;
  Cue cue;
  cue.at = 0;
  cue.preset = preset;
  cue.transition = Transition{Transition::Kind::Blend, 1.0};
  track.cues = {cue};
  show.tracks = {track};
  return show;
}

// An interactive collection has no tracks: it plays its preset pool, switching every 8 bars.
Collection interactive_show(const Collection& base) {
  Collection show = base;
  show.type = CollectionType::Album;
  Track track;
  track.file = "(live)";
  track.grid.bpm = kDefaultBpm;
  Cue cue;
  cue.at = 0;
  cue.pool = base.preset_pool;
  if (const auto every = parse_duration("8 bars", track.grid)) cue.every = *every;
  track.cues = {cue};
  show.tracks = {track};
  return show;
}

struct Command {
  enum class Kind { Preview, StopPreview, Play, Stop, Refresh };
  Kind kind = Kind::Stop;
  std::string id;
  Collection collection;
};

}  // namespace

struct Engine::Impl {
  struct Active {
    std::string id;
    Collection show;  // what actually runs: the collection itself, or a synthetic one
    std::string title;
    bool preview = false;
    std::size_t track = 0;
    SamplePos length = 0;
    SamplePos loop_start = 0;
    SamplePos loop_end = 0;  // 0: no loop
    SamplePos media_pos = 0;  // where the last rendered frame was
    std::unique_ptr<SyntheticSource> source;
    std::unique_ptr<Sequencer> sequencer;
    std::unique_ptr<ShowRunner> runner;
    std::size_t errors_before = 0;
    std::string last_error;
    bool finished = false;
    std::int64_t finished_tick = 0;
  };

  Impl(ProjectMHost& h, const IPresetCatalog& c, EngineConfig cfg)
      : host(h), catalog(c), config(std::move(cfg)), silence(512 * 2, 0.0f) {
    load_idle();
  }

  void load_idle() {
    std::string error;
    host.load_preset("idle://", kCut, error);
  }

  void go_idle() {
    active.reset();
    load_idle();
  }

  // Starts a track; `from` is the position to start at (a preview starts at its window start).
  void start_track(std::size_t index, std::optional<SamplePos> from = std::nullopt) {
    Active& a = *active;
    if (a.runner) {
      a.errors_before += a.runner->errors().size();
      if (!a.runner->errors().empty()) a.last_error = a.runner->errors().back();
    }
    a.track = index;
    const double bpm = a.show.tracks[index].grid.bpm;
    a.source = std::make_unique<SyntheticSource>(bpm);
    a.sequencer = std::make_unique<Sequencer>(a.show, index, a.length, catalog);
    a.runner = std::make_unique<ShowRunner>(host, *a.sequencer, *a.source,
                                            RunnerConfig{config.fps, 0, 512, config.preset_root});
    const SamplePos start = from.value_or(a.preview ? a.loop_start : 0);
    if (start > 0) a.runner->seek(start);
  }

  void begin_preview(std::string id, const Collection& c) {
    auto a = std::make_unique<Active>();
    a->id = std::move(id);
    a->title = c.title;
    a->preview = true;

    const bool has_preset = c.preview && !c.preview->preset.empty();
    if (c.type == CollectionType::Interactive) {
      a->show = has_preset ? single_preset_show(c, c.preview->preset, kDefaultBpm) : interactive_show(c);
      a->length = kEndless;
    } else {
      const std::size_t track = c.preview ? static_cast<std::size_t>(c.preview->track - 1) : 0;
      const double bpm = c.tracks[track].grid.bpm;
      a->show = has_preset ? single_preset_show(c, c.preview->preset, bpm) : c;
      a->track = has_preset ? 0 : track;
      const SamplePos start = c.preview ? c.preview->start : 0;
      const SamplePos length = c.preview ? c.preview->length : kDefaultPreviewLength;
      a->loop_start = start;
      a->loop_end = start + length;
      a->length = std::max(config.dev_track_length, a->loop_end + kSampleRate);
    }
    active = std::move(a);
    start_track(active->track);
  }

  void begin_play(std::string id, const Collection& c) {
    auto a = std::make_unique<Active>();
    a->id = std::move(id);
    a->title = c.title;
    if (c.type == CollectionType::Interactive) {
      a->show = interactive_show(c);
      a->length = kEndless;
    } else {
      a->show = c;
      a->length = config.dev_track_length;
    }
    active = std::move(a);
    start_track(0);
  }

  void apply(Command& command) {
    switch (command.kind) {
      case Command::Kind::Preview:
        if (active && !active->preview) return;  // something is playing: no previews
        begin_preview(std::move(command.id), command.collection);
        break;
      case Command::Kind::StopPreview:
        if (active && active->preview) go_idle();
        break;
      case Command::Kind::Play:
        begin_play(std::move(command.id), command.collection);
        break;
      case Command::Kind::Stop:
        go_idle();
        break;
      case Command::Kind::Refresh:
        // Rebuild the schedule of the current track (pool choices depend on the catalog) and carry on
        // from the same position.
        if (active) start_track(active->track, active->media_pos);
        break;
    }
  }

  void publish_status(double media_seconds) {
    EngineStatus s;
    if (active) {
      s.mode = active->preview ? EngineMode::Previewing : EngineMode::Playing;
      s.id = active->id;
      s.title = active->title;
      s.track = static_cast<int>(active->track) + 1;
      s.track_count = static_cast<int>(active->show.tracks.size());
      s.preset = active->runner->current_preset();
      s.media_seconds = media_seconds;
      s.error_count = active->errors_before + active->runner->errors().size();
      s.last_error = active->runner->errors().empty() ? active->last_error : active->runner->errors().back();
    }
    std::lock_guard lock(mutex);
    status = std::move(s);
  }

  ProjectMHost& host;
  const IPresetCatalog& catalog;
  EngineConfig config;
  std::vector<float> silence;

  mutable std::mutex mutex;  // guards commands, finished_callback and status
  std::vector<Command> commands;
  std::function<void()> finished_callback;
  EngineStatus status;

  std::unique_ptr<Active> active;  // render thread only
  std::int64_t tick_count = 0;
};

Engine::Engine(ProjectMHost& host, const IPresetCatalog& catalog, EngineConfig config)
    : impl_(std::make_unique<Impl>(host, catalog, std::move(config))) {}

Engine::~Engine() = default;

void Engine::preview(std::string id, Collection collection) {
  std::lock_guard lock(impl_->mutex);
  impl_->commands.push_back({Command::Kind::Preview, std::move(id), std::move(collection)});
}

void Engine::stop_preview() {
  std::lock_guard lock(impl_->mutex);
  impl_->commands.push_back({Command::Kind::StopPreview, {}, {}});
}

void Engine::play(std::string id, Collection collection) {
  std::lock_guard lock(impl_->mutex);
  impl_->commands.push_back({Command::Kind::Play, std::move(id), std::move(collection)});
}

void Engine::stop() {
  std::lock_guard lock(impl_->mutex);
  impl_->commands.push_back({Command::Kind::Stop, {}, {}});
}

void Engine::refresh_schedule() {
  std::lock_guard lock(impl_->mutex);
  impl_->commands.push_back({Command::Kind::Refresh, {}, {}});
}

void Engine::set_finished_callback(std::function<void()> callback) {
  std::lock_guard lock(impl_->mutex);
  impl_->finished_callback = std::move(callback);
}

EngineStatus Engine::status() const {
  std::lock_guard lock(impl_->mutex);
  return impl_->status;
}

void Engine::tick() {
  Impl& s = *impl_;
  ++s.tick_count;

  std::vector<Command> commands;
  {
    std::lock_guard lock(s.mutex);
    commands.swap(s.commands);
  }
  for (Command& command : commands) s.apply(command);

  if (!s.active) {
    s.host.add_pcm_stereo(s.silence.data(), 512);
    s.host.render();
    s.publish_status(0);
    return;
  }

  Impl::Active& a = *s.active;
  const auto step = a.runner->step();
  a.media_pos = step.media_pos;

  if (a.preview) {
    if (a.loop_end > 0 && step.media_pos >= a.loop_end) a.runner->seek(a.loop_start);
  } else if (!a.finished && step.media_pos >= a.length) {
    if (a.track + 1 < a.show.tracks.size()) {
      s.start_track(a.track + 1);
    } else {
      a.finished = true;
      a.finished_tick = s.tick_count;
      std::function<void()> callback;
      {
        std::lock_guard lock(s.mutex);
        callback = s.finished_callback;
      }
      if (callback) callback();
    }
  } else if (a.finished && s.tick_count - a.finished_tick >= s.config.finish_grace_frames) {
    s.go_idle();
    s.publish_status(0);
    return;
  }
  s.publish_status(static_cast<double>(step.media_pos) / static_cast<double>(kSampleRate));
}

}  // namespace rm
