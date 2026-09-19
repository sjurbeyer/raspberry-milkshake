#include "core/demo_audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rm {
namespace {

constexpr double kTwoPi = 6.283185307179586;

// Deterministic noise in [-1, 1] from the sample index.
double noise(std::uint64_t n) {
  n ^= n >> 33;
  n *= 0xff51afd7ed558ccdULL;
  n ^= n >> 33;
  n *= 0xc4ceb9fe1a85ec53ULL;
  n ^= n >> 33;
  return static_cast<double>(n >> 11) / 4503599627370496.0 - 1.0;  // 2^52
}

}  // namespace

void SyntheticMusic::read_stereo(SamplePos start, unsigned frames, float* out) const {
  const double beat = 60.0 / bpm_;
  for (unsigned i = 0; i < frames; ++i) {
    const SamplePos n = start + static_cast<SamplePos>(i);
    double left = 0;
    double right = 0;
    if (n >= 0) {
      const double t = static_cast<double>(n) / static_cast<double>(kSampleRate);

      // Kick: a sine sweeping 120 -> 45 Hz with a fast decay, at the start of every beat.
      const double since_beat = std::fmod(t, beat);
      const double kick_phase = kTwoPi * (45.0 * since_beat + 75.0 * (1.0 - std::exp(-30.0 * since_beat)) / 30.0);
      const double kick = 0.8 * std::exp(-14.0 * since_beat) * std::sin(kick_phase);

      // Hat: a burst of noise halfway between beats.
      const double since_offbeat = std::fmod(t + beat / 2.0, beat);
      const double hat = 0.25 * std::exp(-70.0 * since_offbeat) * noise(static_cast<std::uint64_t>(n));

      // Pad: a slowly swelling A-minor triad, slightly detuned per side.
      const double swell = 0.6 + 0.4 * std::sin(kTwoPi * t / (beat * 8.0));
      const double pad_l = std::sin(kTwoPi * 220.0 * t) + std::sin(kTwoPi * 261.63 * t) + std::sin(kTwoPi * 329.63 * t);
      const double pad_r = std::sin(kTwoPi * 220.7 * t) + std::sin(kTwoPi * 261.63 * t) + std::sin(kTwoPi * 330.4 * t);

      left = kick + hat + 0.05 * swell * pad_l;
      right = kick + hat + 0.05 * swell * pad_r;
    }
    out[i * 2] = static_cast<float>(std::clamp(left, -1.0, 1.0));
    out[i * 2 + 1] = static_cast<float>(std::clamp(right, -1.0, 1.0));
  }
}

}  // namespace rm
