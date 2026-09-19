#pragma once

#include <cstdint>

namespace rm {

inline constexpr std::int64_t kSampleRate = 48000;

using SamplePos = std::int64_t;

class IClock {
 public:
  virtual ~IClock() = default;
  virtual SamplePos now() const = 0;
};

// Deterministic clock for tests and the dev container: time only moves when told to.
class VirtualClock final : public IClock {
 public:
  SamplePos now() const override;
  void advance(SamplePos samples);
  void set(SamplePos pos);

 private:
  SamplePos pos_ = 0;
};

}  // namespace rm
