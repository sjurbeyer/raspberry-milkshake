#include "render_harness.hpp"

#include "app/engine_bridge.hpp"
#include "app/headless.hpp"
#include "core/dir_catalog.hpp"
#include "render/engine.hpp"
#include "ui/app_controller.hpp"
#include "ui/library_model.hpp"

using namespace rm;
using namespace rm::test;

namespace {

const std::filesystem::path kPresetRoot = std::filesystem::path(RM_SOURCE_DIR) / "tests/fixtures/presets";

}  // namespace

// Proves the claim design-spec.md section 15.1 item 3 relies on: the engine, controller and bridge
// work without any QML view ever having been created, which is what a headless start needs.
TEST_CASE("starting headless plays the interactive collection with no view involved", "[app][headless]") {
  Harness harness;
  DirCatalog catalog(kPresetRoot);
  EngineConfig config;
  config.fps = 60;
  config.preset_root = kPresetRoot;
  Engine engine(*harness.host, catalog, config);

  LibraryModel model;
  model.load(QString(RM_SOURCE_DIR) + "/examples/collections");
  AppController controller(&model);
  EngineBridge bridge(&controller, &model, &engine);

  REQUIRE(should_run_headless(0, false, false));
  const QString id = find_default_interactive_id(model);
  REQUIRE(id == "interactive");
  controller.playNow(id);

  for (int i = 0; i < 3; ++i) engine.tick();
  const EngineStatus status = engine.status();
  REQUIRE(status.mode == EngineMode::Playing);
  REQUIRE(status.id == "interactive");
  REQUIRE(status.title == "Interactive visualization");
}
