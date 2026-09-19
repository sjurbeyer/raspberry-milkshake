#include "core/clock.hpp"

namespace rm {

SamplePos VirtualClock::now() const { return pos_; }
void VirtualClock::advance(SamplePos samples) { pos_ += samples; }
void VirtualClock::set(SamplePos pos) { pos_ = pos; }

}  // namespace rm
