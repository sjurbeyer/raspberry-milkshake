#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "core/demo_audio.hpp"

using namespace rm;

namespace {

double rms(const std::vector<float>& stereo) {
  double sum = 0;
  for (const float s : stereo) sum += static_cast<double>(s) * static_cast<double>(s);
  return std::sqrt(sum / static_cast<double>(stereo.size()));
}

std::vector<float> read(const SyntheticMusic& m, SamplePos start, unsigned frames) {
  std::vector<float> out(frames * 2);
  m.read_stereo(start, frames, out.data());
  return out;
}

}  // namespace

TEST_CASE("synthetic music is silent before time zero", "[demo_audio]") {
  const SyntheticMusic music(120.0);
  REQUIRE(rms(read(music, -1000, 1000)) == 0.0);
}

TEST_CASE("the result does not depend on how the range is chunked", "[demo_audio]") {
  const SyntheticMusic music(120.0);
  const auto whole = read(music, 10'000, 3000);
  auto joined = read(music, 10'000, 1234);
  const auto rest = read(music, 10'000 + 1234, 3000 - 1234);
  joined.insert(joined.end(), rest.begin(), rest.end());
  REQUIRE(whole == joined);
}

TEST_CASE("kicks land on the beat and are much louder than the gaps", "[demo_audio]") {
  const SyntheticMusic music(120.0);  // a beat every 24000 samples
  const double on_beat = rms(read(music, 24'000, 1200));           // just after the beat at 0.5 s
  const double between = rms(read(music, 24'000 + 12'000 + 3000, 1200));  // late in the gap
  REQUIRE(on_beat > 0.2);
  REQUIRE(on_beat > between * 4.0);
}

TEST_CASE("samples stay within range", "[demo_audio]") {
  const SyntheticMusic music(174.0);
  for (const float s : read(music, 0, 96'000)) {
    REQUIRE(s >= -1.0f);
    REQUIRE(s <= 1.0f);
  }
}
