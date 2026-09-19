#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "core/manifest.hpp"

namespace rm {

// Source of presets for pool cues (the preset library plus rm-bench tags).
class IPresetCatalog {
 public:
  virtual ~IPresetCatalog() = default;
  virtual std::vector<std::string> resolve(const PoolExpr& pool) const = 0;
};

struct LoadPreset {
  SamplePos at = 0;
  std::string preset;
  Transition transition;
};

// Expands the cues of one track into a fixed, sorted event list at construction time (so pool
// choices are deterministic for a given collection seed), then hands events out as the audio clock
// advances. Not thread-safe; owned by the sequencer thread.
class Sequencer {
 public:
  // `track_index` is 0-based. `track_length` bounds `every` expansion and must be > 0.
  // Throws std::out_of_range if `track_index` is invalid.
  Sequencer(const Collection& collection, std::size_t track_index, SamplePos track_length,
            const IPresetCatalog& catalog);

  const std::vector<LoadPreset>& events() const { return events_; }
  const std::vector<std::string>& warnings() const { return warnings_; }

  // Returns the events with at <= now that have not been returned yet, in order.
  std::vector<LoadPreset> advance(SamplePos now);

  // Repositions playback. Returns the event that is active at `pos` (the last one at or before it),
  // if any; later calls to advance() only return events after `pos`.
  std::optional<LoadPreset> seek(SamplePos pos);

 private:
  std::vector<LoadPreset> events_;
  std::vector<std::string> warnings_;
  std::size_t cursor_ = 0;
};

}  // namespace rm
