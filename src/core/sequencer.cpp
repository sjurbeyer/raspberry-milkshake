#include "core/sequencer.hpp"

#include <algorithm>
#include <cstdint>

namespace rm {
namespace {

// Own generator: std::uniform_int_distribution is implementation-defined, and a show must replay
// identically on the Pi, in the dev container and in tests.
struct SplitMix64 {
  std::uint64_t state;

  std::uint64_t next() {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
};

std::uint64_t mix(std::uint64_t a, std::uint64_t b) {
  return SplitMix64{a ^ ((b + 1) * 0xD6E8FEB86659FD93ULL)}.next();
}

std::string describe(const PoolExpr& pool) {
  switch (pool.kind) {
    case PoolExpr::Kind::All: return "all";
    case PoolExpr::Kind::Tag: return "tag:" + pool.arg;
    case PoolExpr::Kind::Dir: return "dir:" + pool.arg;
  }
  return "?";
}

// Never returns `previous` when there is any alternative.
const std::string& pick(const std::vector<std::string>& candidates, const std::string& previous,
                        SplitMix64& rng) {
  std::vector<const std::string*> allowed;
  for (const std::string& c : candidates) {
    if (c != previous) allowed.push_back(&c);
  }
  if (allowed.empty()) return candidates.front();
  return *allowed[rng.next() % allowed.size()];
}

}  // namespace

Sequencer::Sequencer(const Collection& collection, std::size_t track_index, SamplePos track_length,
                     const IPresetCatalog& catalog) {
  const Track& track = collection.tracks.at(track_index);
  const auto& cues = track.cues;
  std::string previous;

  auto emit = [&](SamplePos at, const std::string& preset, const Transition& transition) {
    events_.push_back({at, preset, transition});
    previous = preset;
  };

  for (std::size_t i = 0; i < cues.size(); ++i) {
    const Cue& cue = cues[i];
    const std::string where = "cue[" + std::to_string(i + 1) + "]";
    if (cue.at >= track_length) {
      warnings_.push_back(where + ": starts at or after the end of the track; skipped");
      continue;
    }
    const SamplePos end = (i + 1 < cues.size()) ? std::min(cues[i + 1].at, track_length) : track_length;
    const Transition transition = cue.transition.value_or(collection.default_transition);

    if (!cue.pool) {
      emit(cue.at, cue.preset, transition);
      continue;
    }

    std::vector<std::string> candidates = catalog.resolve(*cue.pool);
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    if (candidates.empty()) {
      warnings_.push_back(where + ": pool \"" + describe(*cue.pool) +
                          "\" is empty; keeping the previous preset");
      continue;
    }

    SplitMix64 rng{mix(mix(collection.seed, track_index), i)};
    emit(cue.at, pick(candidates, previous, rng), transition);
    if (cue.every) {
      for (SamplePos t = cue.at + *cue.every; t < end; t += *cue.every) {
        emit(t, pick(candidates, previous, rng), transition);
      }
    }
  }
}

std::vector<LoadPreset> Sequencer::advance(SamplePos now) {
  std::vector<LoadPreset> due;
  while (cursor_ < events_.size() && events_[cursor_].at <= now) {
    due.push_back(events_[cursor_++]);
  }
  return due;
}

std::optional<LoadPreset> Sequencer::seek(SamplePos pos) {
  const auto after = std::find_if(events_.begin(), events_.end(),
                                  [pos](const LoadPreset& e) { return e.at > pos; });
  cursor_ = static_cast<std::size_t>(after - events_.begin());
  if (cursor_ == 0) return std::nullopt;
  return events_[cursor_ - 1];
}

}  // namespace rm
