#pragma once

#include "core/clock.hpp"

namespace rm {

// A stateless, deterministic stand-in for music (kick on the beat, hats on the off-beat, a slow
// pad), so the dev dashboard has something to react to before real audio playback exists. Any
// range can be read in any chunking with identical results.
class SyntheticMusic {
 public:
  explicit SyntheticMusic(double bpm = 120.0) : bpm_(bpm) {}

  // Fills `frames` interleaved stereo samples for positions start .. start+frames-1. Positions
  // before 0 are silence.
  void read_stereo(SamplePos start, unsigned frames, float* interleaved_out) const;

 private:
  double bpm_;
};

}  // namespace rm
