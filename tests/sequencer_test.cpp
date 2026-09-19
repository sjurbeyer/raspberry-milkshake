#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "core/manifest.hpp"
#include "core/sequencer.hpp"

using namespace rm;

namespace {

constexpr SamplePos kSec = kSampleRate;

std::string key(const PoolExpr& p) {
  switch (p.kind) {
    case PoolExpr::Kind::All: return "all";
    case PoolExpr::Kind::Tag: return "tag:" + p.arg;
    case PoolExpr::Kind::Dir: return "dir:" + p.arg;
  }
  return "";
}

class FakeCatalog final : public IPresetCatalog {
 public:
  std::map<std::string, std::vector<std::string>> pools;
  std::vector<std::string> resolve(const PoolExpr& pool) const override {
    const auto it = pools.find(key(pool));
    return it == pools.end() ? std::vector<std::string>{} : it->second;
  }
};

Cue explicit_cue(SamplePos at, std::string preset, std::optional<Transition> tr = std::nullopt) {
  Cue c;
  c.at = at;
  c.preset = std::move(preset);
  c.transition = tr;
  return c;
}

Cue pool_cue(SamplePos at, PoolExpr pool, std::optional<SamplePos> every = std::nullopt) {
  Cue c;
  c.at = at;
  c.pool = std::move(pool);
  c.every = every;
  return c;
}

Collection make_collection(std::vector<Cue> cues, std::uint64_t seed = 1) {
  Collection c;
  c.seed = seed;
  Track t;
  t.file = "a.flac";
  t.cues = std::move(cues);
  c.tracks.push_back(std::move(t));
  return c;
}

const PoolExpr kAll{PoolExpr::Kind::All, {}};

std::vector<std::string> presets(const std::vector<LoadPreset>& events) {
  std::vector<std::string> out;
  for (const auto& e : events) out.push_back(e.preset);
  return out;
}

std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

TEST_CASE("explicit cues use the default transition unless overridden", "[sequencer]") {
  const Collection c = make_collection({
      explicit_cue(0, "a.milk", Transition{Transition::Kind::Cut, 0}),
      explicit_cue(10 * kSec, "b.milk"),
  });
  const Sequencer seq(c, 0, 60 * kSec, FakeCatalog{});
  REQUIRE(seq.events().size() == 2);
  REQUIRE(seq.events()[0].at == 0);
  REQUIRE(seq.events()[0].transition.kind == Transition::Kind::Cut);
  REQUIRE(seq.events()[1].at == 10 * kSec);
  REQUIRE(seq.events()[1].transition.kind == Transition::Kind::Blend);
  REQUIRE(seq.events()[1].transition.seconds == 2.0);
}

TEST_CASE("a pool cue with every repeats until the next cue", "[sequencer]") {
  FakeCatalog catalog;
  catalog.pools["all"] = {"p1.milk", "p2.milk", "p3.milk"};
  const Collection c = make_collection({
      pool_cue(0, kAll, kSec),
      explicit_cue(3 * kSec + kSec / 2, "x.milk"),
  });
  const Sequencer seq(c, 0, 10 * kSec, catalog);
  REQUIRE(seq.events().size() == 5);
  for (int k = 0; k < 4; ++k) REQUIRE(seq.events()[k].at == k * kSec);
  REQUIRE(seq.events()[4].at == 3 * kSec + kSec / 2);
  REQUIRE(seq.events()[4].preset == "x.milk");
}

TEST_CASE("a pool cue with every runs to the end of the track when nothing follows", "[sequencer]") {
  FakeCatalog catalog;
  catalog.pools["all"] = {"p1.milk", "p2.milk"};
  const Collection c = make_collection({pool_cue(0, kAll, 4 * kSec)});
  const Sequencer seq(c, 0, 10 * kSec, catalog);
  REQUIRE(seq.events().size() == 3);
  REQUIRE(seq.events()[2].at == 8 * kSec);
}

TEST_CASE("a pool cue without every fires once", "[sequencer]") {
  FakeCatalog catalog;
  catalog.pools["all"] = {"p1.milk", "p2.milk"};
  const Sequencer seq(make_collection({pool_cue(0, kAll)}), 0, 10 * kSec, catalog);
  REQUIRE(seq.events().size() == 1);
}

TEST_CASE("pools never repeat the previous preset when there is an alternative", "[sequencer]") {
  FakeCatalog catalog;
  catalog.pools["all"] = {"a.milk", "b.milk"};

  SECTION("consecutive picks differ") {
    const Sequencer seq(make_collection({pool_cue(0, kAll, kSec)}), 0, 50 * kSec, catalog);
    REQUIRE(seq.events().size() == 50);
    for (std::size_t i = 1; i < seq.events().size(); ++i) {
      REQUIRE(seq.events()[i].preset != seq.events()[i - 1].preset);
    }
  }
  SECTION("the previous explicit cue counts as the previous preset") {
    const Sequencer seq(make_collection({explicit_cue(0, "a.milk"), pool_cue(kSec, kAll)}), 0,
                        10 * kSec, catalog);
    REQUIRE(seq.events()[1].preset == "b.milk");
  }
  SECTION("a single-preset pool may repeat") {
    catalog.pools["all"] = {"a.milk"};
    const Sequencer seq(make_collection({pool_cue(0, kAll, kSec)}), 0, 5 * kSec, catalog);
    REQUIRE(presets(seq.events()) == std::vector<std::string>(5, "a.milk"));
  }
}

TEST_CASE("pool choices are deterministic per seed", "[sequencer]") {
  FakeCatalog catalog;
  catalog.pools["all"] = {"a.milk", "b.milk", "c.milk", "d.milk", "e.milk"};
  auto run = [&](std::uint64_t seed) {
    return presets(Sequencer(make_collection({pool_cue(0, kAll, kSec)}, seed), 0, 30 * kSec, catalog).events());
  };
  REQUIRE(run(1) == run(1));
  bool any_differs = false;
  for (std::uint64_t seed = 2; seed <= 9; ++seed) any_differs |= run(seed) != run(1);
  REQUIRE(any_differs);
}

TEST_CASE("catalog order does not affect the result", "[sequencer]") {
  FakeCatalog forward;
  forward.pools["all"] = {"a.milk", "b.milk", "c.milk"};
  FakeCatalog backward;
  backward.pools["all"] = {"c.milk", "b.milk", "a.milk"};
  const Collection c = make_collection({pool_cue(0, kAll, kSec)});
  REQUIRE(presets(Sequencer(c, 0, 20 * kSec, forward).events()) ==
          presets(Sequencer(c, 0, 20 * kSec, backward).events()));
}

TEST_CASE("an empty pool warns and keeps the previous preset", "[sequencer]") {
  const Collection c = make_collection({explicit_cue(0, "a.milk"), pool_cue(5 * kSec, kAll, kSec)});
  const Sequencer seq(c, 0, 20 * kSec, FakeCatalog{});
  REQUIRE(seq.events().size() == 1);
  REQUIRE(seq.warnings().size() == 1);
  REQUIRE(seq.warnings()[0].find("empty") != std::string::npos);
}

TEST_CASE("a cue at or after the end of the track is skipped with a warning", "[sequencer]") {
  const Collection c = make_collection({explicit_cue(0, "a.milk"), explicit_cue(20 * kSec, "b.milk")});
  const Sequencer seq(c, 0, 10 * kSec, FakeCatalog{});
  REQUIRE(seq.events().size() == 1);
  REQUIRE(seq.warnings().size() == 1);
}

TEST_CASE("an invalid track index throws", "[sequencer]") {
  const Collection c = make_collection({explicit_cue(0, "a.milk")});
  REQUIRE_THROWS_AS(Sequencer(c, 5, 10 * kSec, FakeCatalog{}), std::out_of_range);
}

TEST_CASE("advance delivers each event once, in the block that contains it", "[sequencer]") {
  FakeCatalog catalog;
  catalog.pools["all"] = {"a.milk", "b.milk", "c.milk"};
  const SamplePos length = 10 * kSec;
  const Collection c = make_collection({pool_cue(0, kAll, kSec)});
  Sequencer seq(c, 0, length, catalog);

  constexpr SamplePos kBlock = 1024;
  VirtualClock clock;
  std::size_t delivered = 0;

  for (const auto& e : seq.advance(clock.now())) {  // events at exactly t = 0
    REQUIRE(e.at == 0);
    ++delivered;
  }
  while (clock.now() < length) {
    const SamplePos block_start = clock.now();
    clock.advance(kBlock);
    for (const auto& e : seq.advance(clock.now())) {
      REQUIRE(e.at > block_start);
      REQUIRE(e.at <= clock.now());
      ++delivered;
    }
  }
  REQUIRE(delivered == seq.events().size());
  REQUIRE(seq.advance(clock.now() + 100 * kSec).empty());
}

TEST_CASE("seek repositions playback", "[sequencer]") {
  const Collection c = make_collection({
      explicit_cue(kSec, "b.milk"),
      explicit_cue(2 * kSec, "c.milk"),
      explicit_cue(3 * kSec, "d.milk"),
  });
  Sequencer seq(c, 0, 10 * kSec, FakeCatalog{});
  REQUIRE(seq.advance(10 * kSec).size() == 3);

  const auto active = seq.seek(2 * kSec + kSec / 2);
  REQUIRE(active.has_value());
  REQUIRE(active->preset == "c.milk");
  const auto rest = seq.advance(10 * kSec);
  REQUIRE(rest.size() == 1);
  REQUIRE(rest[0].preset == "d.milk");

  REQUIRE_FALSE(seq.seek(kSec / 2).has_value());  // nothing set yet before the first cue
  REQUIRE(seq.advance(10 * kSec).size() == 3);

  REQUIRE(seq.seek(kSec)->preset == "b.milk");  // an event exactly at pos counts as active
  REQUIRE(seq.advance(10 * kSec).size() == 2);
}

TEST_CASE("the example album sequences as authored", "[sequencer]") {
  const auto parsed = parse_manifest(
      read_file(std::filesystem::path(RM_SOURCE_DIR) / "examples/collections/night-drive/collection.toml"));
  REQUIRE(parsed.ok());
  FakeCatalog catalog;
  catalog.pools["tag:ambient"] = {"amb1.milk", "amb2.milk", "amb3.milk"};

  SECTION("track 1: an explicit cue, then a pool re-picked every 8 bars") {
    const SamplePos length = 4 * 755'410 + 1000;
    const Sequencer seq(*parsed.collection, 0, length, catalog);
    REQUIRE(seq.warnings().empty());
    REQUIRE(seq.events().size() == 5);
    REQUIRE(seq.events()[0].preset == "presets/tunnel.milk");
    REQUIRE(seq.events()[0].transition.kind == Transition::Kind::Cut);
    for (int k = 1; k <= 4; ++k) {
      REQUIRE(seq.events()[k].at == k * 755'410);
      REQUIRE(seq.events()[k].transition.seconds == 2.0);
    }
  }
  SECTION("track 2: musical cue with downbeat offset and per-cue blend") {
    const Sequencer seq(*parsed.collection, 1, 60 * kSec, catalog);
    REQUIRE(seq.events().size() == 2);
    REQUIRE(seq.events()[0].preset == "presets/flow.milk");
    REQUIRE(seq.events()[1].at == 1'508'250);
    REQUIRE(seq.events()[1].preset == "presets/pulse.milk");
    REQUIRE(seq.events()[1].transition.seconds == 0.5);
  }
}
