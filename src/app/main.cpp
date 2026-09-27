// rm-app: the whole appliance in one process, as it will run on the Pi: the Qt touch UI on the main
// thread, the projectM engine on its own render thread, and (for the dev container) an HTTP server that
// shows the main output and LED chain in a browser. In the container the touch UI runs on Qt's VNC
// platform plugin (-platform vnc:size=1280x720:port=5900).

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQuickView>
#include <QtDebug>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <future>
#include <memory>
#include <sstream>
#include <thread>

#include "app/engine_bridge.hpp"
#include "app/headless.hpp"
#include "core/dir_catalog.hpp"
#include "core/led_layout.hpp"
#include "render/dashboard_server.hpp"
#include "render/downscaler.hpp"
#include "render/egl_context.hpp"
#include "render/engine.hpp"
#include "render/jpeg.hpp"
#include "render/projectm_host.hpp"
#include "ui/app_controller.hpp"
#include "ui/library_filter.hpp"
#include "ui/library_model.hpp"
#include "ui/svg_image_provider.hpp"

namespace fs = std::filesystem;

namespace {

struct RenderSettings {
  int width = 640;
  int height = 360;
  int fps = 30;
  double track_seconds = 60;
  rm::PauseDisplay pause_display = rm::PauseDisplay::Hold;
  bool headless = false;  // no touch display: no window, dashboard/status say so
  fs::path presets;
  fs::path fallback_presets;
};

const char* mode_name(rm::EngineMode mode) {
  switch (mode) {
    case rm::EngineMode::Idle: return "idle";
    case rm::EngineMode::Previewing: return "preview";
    case rm::EngineMode::Playing: return "playing";
    case rm::EngineMode::Paused: return "paused";
  }
  return "idle";
}

std::string layout_json(const rm::LedLayout& layout) {
  std::ostringstream out;
  out << "{\"points\":[";
  for (std::size_t i = 0; i < layout.points.size(); ++i) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s[%.4f,%.4f]", i ? "," : "", static_cast<double>(layout.points[i].u),
                  static_cast<double>(layout.points[i].v));
    out << buf;
  }
  out << "]}";
  return out.str();
}

std::string status_json(const rm::EngineStatus& s, const RenderSettings& r, const std::string& renderer,
                        const rm::DirCatalog& catalog, std::size_t led_count, double fps, double render_ms) {
  // -1 when the preset is not in the catalog (for example an explicit cue outside the preset folder).
  const double rating = catalog.rating(s.preset).value_or(-1.0);
  const std::size_t preset_count = catalog.size();
  char numbers[240];
  std::snprintf(numbers, sizeof numbers,
                "\"media_seconds\":%.2f,\"fps\":%.1f,\"render_ms\":%.1f,\"rating\":%.0f,\"hidden_presets\":%zu",
                s.media_seconds, fps, render_ms, rating, catalog.hidden_count());
  std::ostringstream out;
  out << "{\"mode\":\"" << mode_name(s.mode) << "\",\"id\":\"" << rm::json_escape(s.id) << "\",\"title\":\""
      << rm::json_escape(s.title) << "\",\"track\":" << s.track << ",\"track_count\":" << s.track_count
      << ",\"preset\":\"" << rm::json_escape(s.preset) << "\"," << numbers << ",\"renderer\":\""
      << rm::json_escape(renderer) << "\",\"preset_count\":" << preset_count << ",\"errors\":" << s.error_count
      << ",\"last_error\":\"" << rm::json_escape(s.last_error) << "\",\"width\":" << r.width
      << ",\"height\":" << r.height << ",\"target_fps\":" << r.fps << ",\"leds\":" << led_count
      << ",\"headless\":" << (r.headless ? "true" : "false") << "}";
  return out.str();
}

// Owns the projectM context and the engine; runs until `stop` is set.
void render_thread(const RenderSettings& settings, const rm::DirCatalog& catalog, const fs::path& preset_root,
                   rm::DashboardServer& dashboard, const rm::LedLayout& layout, std::promise<rm::Engine*> ready,
                   const std::atomic<bool>& stop) {
  std::string error;
  auto egl = rm::EglContext::create_headless(settings.width, settings.height, error);
  if (!egl) {
    qCritical().noquote() << "cannot create an OpenGL ES context:" << QString::fromStdString(error);
    ready.set_value(nullptr);
    return;
  }
  auto host = rm::ProjectMHost::create(rm::HostConfig{settings.width, settings.height, 32, 24, settings.fps}, error);
  if (!host) {
    qCritical().noquote() << "cannot start projectM:" << QString::fromStdString(error);
    ready.set_value(nullptr);
    return;
  }

  rm::Downscaler scaler(64, 36);
  rm::EngineConfig config;
  config.fps = settings.fps;
  config.preset_root = preset_root;
  config.pause_display = settings.pause_display;
  config.dev_track_length = static_cast<rm::SamplePos>(settings.track_seconds * static_cast<double>(rm::kSampleRate));
  rm::Engine engine(*host, catalog, config);
  ready.set_value(&engine);

  const auto period = std::chrono::microseconds(1'000'000 / settings.fps);
  auto next = std::chrono::steady_clock::now();
  auto previous_start = next;
  bool have_previous = false;
  double fps_smoothed = settings.fps;
  double render_ms = 0;

  std::string last_logged_error;
  while (!stop) {
    const auto start = std::chrono::steady_clock::now();
    engine.tick();
    const rm::EngineStatus status = engine.status();
    if (!status.last_error.empty() && status.last_error != last_logged_error) {
      qWarning().noquote() << "preset failed to load:" << QString::fromStdString(status.last_error);
      last_logged_error = status.last_error;
    }
    // The LEDs are sampled before the pause icon is drawn, so they never show it.
    const auto colors = rm::sample_leds(scaler.capture(settings.width, settings.height), layout);
    engine.draw_overlays();
    const rm::Frame full = host->read_frame();
    std::vector<std::uint8_t> jpeg = rm::encode_jpeg(full, 75);
    const auto done = std::chrono::steady_clock::now();

    render_ms = 0.9 * render_ms + 0.1 * std::chrono::duration<double, std::milli>(done - start).count();
    const double interval = std::chrono::duration<double>(start - previous_start).count();
    if (have_previous && interval > 0) fps_smoothed = 0.9 * fps_smoothed + 0.1 / interval;
    previous_start = start;
    have_previous = true;

    std::string hex;
    hex.reserve(colors.size() * 6);
    for (const rm::LedColor& c : colors) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "%02x%02x%02x", c.r, c.g, c.b);
      hex += buf;
    }
    dashboard.publish(std::move(jpeg), "{\"hex\":\"" + hex + "\"}",
                      status_json(status, settings, egl->renderer(), catalog, colors.size(), fps_smoothed,
                                  render_ms));

    next += period;
    if (done < next) std::this_thread::sleep_until(next);
    else next = done;  // running behind: do not try to catch up
  }
}

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName("rm-app");

  QCommandLineParser parser;
  parser.addHelpOption();
  const QCommandLineOption library_option("library", "Folder holding the collections.", "dir", "examples/collections");
  const QCommandLineOption presets_option("presets", "Folder holding .milk presets.", "dir", "/presets");
  const QCommandLineOption fallback_option("fallback-presets", "Used when --presets holds none.", "dir",
                                           "tests/fixtures/presets");
  const QCommandLineOption port_option("port", "Dashboard HTTP port.", "port", "8080");
  const QCommandLineOption fps_option("fps", "Main output frame rate.", "fps", "30");
  const QCommandLineOption track_option("track-seconds", "Length of every track until audio decoding exists.", "s", "60");
  const QCommandLineOption pause_option("pause-display", "What the main output shows while paused: hold, fade or home.", "mode", "hold");
  const QCommandLineOption size_option("size", "Main output size.", "WxH", "640x360");
  const QCommandLineOption web_option("web", "Dashboard page folder.", "dir", "web");
  const QCommandLineOption headless_option("headless", "Force headless mode (no window), whatever displays are detected.");
  const QCommandLineOption ui_option("ui", "Force the touch UI window, even with no display detected.");
  parser.addOptions({library_option, presets_option, fallback_option, port_option, fps_option, track_option,
                     pause_option, size_option, web_option, headless_option, ui_option});
  parser.process(app);
  if (parser.isSet(headless_option) && parser.isSet(ui_option)) {
    qCritical() << "--headless and --ui are mutually exclusive";
    return 2;
  }

  RenderSettings settings;
  settings.fps = parser.value(fps_option).toInt();
  settings.headless = rm::should_run_headless(QGuiApplication::screens().size(), parser.isSet(headless_option),
                                              parser.isSet(ui_option));
  settings.track_seconds = parser.value(track_option).toDouble();
  const QString pause_display = parser.value(pause_option);
  if (pause_display == "fade") settings.pause_display = rm::PauseDisplay::Fade;
  else if (pause_display == "home") settings.pause_display = rm::PauseDisplay::Home;
  else if (pause_display != "hold") qWarning().noquote() << "unknown --pause-display" << pause_display << "- using hold";
  const QStringList size = parser.value(size_option).split('x');
  if (size.size() == 2) {
    settings.width = size[0].toInt();
    settings.height = size[1].toInt();
  }
  settings.presets = parser.value(presets_option).toStdString();
  settings.fallback_presets = parser.value(fallback_option).toStdString();

  // The touch UI's data.
  rm::LibraryModel model;
  model.load(parser.value(library_option));
  for (const QString& problem : model.problems()) qWarning().noquote() << "skipped collection:" << problem;
  qInfo().noquote() << model.rowCount() << "collections from" << parser.value(library_option);
  if (settings.headless) qInfo() << "no touch display detected (or --headless): running headless";
  rm::AppController controller(&model);

  // The presets. The catalog is shared with the dashboard's rating endpoint, so it lives here.
  fs::path preset_root = settings.presets;
  rm::DirCatalog catalog(preset_root);
  if (catalog.size() == 0) {
    qWarning().noquote() << "no .milk presets under" << QString::fromStdString(preset_root.string())
                         << "- using the small built-in test presets. Run `just presets`.";
    preset_root = settings.fallback_presets;
    catalog = rm::DirCatalog(preset_root);
  }
  qInfo().noquote() << catalog.size() << "presets from" << QString::fromStdString(preset_root.string()) << "("
                    << catalog.hidden_count() << "rated 0, hidden from random picks)";

  // The main output: dashboard server plus the render thread with the engine.
  const rm::LedLayout layout = rm::perimeter_layout(24, 14, 24, 14, 0.08f);
  rm::DashboardOptions dashboard_options;
  dashboard_options.port = parser.value(port_option).toInt();
  dashboard_options.web = parser.value(web_option).toStdString();
  rm::DashboardServer dashboard(dashboard_options, layout_json(layout));
  dashboard.start();

  std::atomic<bool> stop{false};
  std::promise<rm::Engine*> ready;
  std::future<rm::Engine*> engine_future = ready.get_future();
  std::thread renderer(render_thread, std::cref(settings), std::cref(catalog), std::cref(preset_root),
                       std::ref(dashboard), std::cref(layout), std::move(ready), std::cref(stop));
  rm::Engine* engine = engine_future.get();
  if (!engine) {
    stop = true;
    renderer.join();
    return 1;
  }
  auto bridge = std::make_unique<rm::EngineBridge>(&controller, &model, engine);

  // Rating a preset from the dashboard: write it into the preset file, then re-plan the current track
  // so a preset rated 0 goes away now. Runs on an HTTP thread; both calls are thread-safe.
  dashboard.set_rating_handler([&catalog, engine](const std::string& preset, int rating) {
    std::string error;
    if (!catalog.set_rating(preset, rating, &error)) {
      qWarning().noquote() << "could not rate" << QString::fromStdString(preset) << ":" << QString::fromStdString(error);
      return false;
    }
    qInfo().noquote() << "rated" << QString::fromStdString(preset) << rating;
    engine->refresh_schedule();
    return true;
  });

  QObject::connect(&controller, &rm::AppController::previewRequested,
                   [](const QString& id) { qInfo().noquote() << "preview:" << id; });
  QObject::connect(&controller, &rm::AppController::playRequested,
                   [](const QString& id) { qInfo().noquote() << "play:" << id; });
  QObject::connect(&controller, &rm::AppController::stopped, [] { qInfo() << "stopped"; });

  int exit_code = 1;
  if (settings.headless) {
    // No touch UI: play the interactive collection by itself. design-spec.md section 15.1 item 3.
    const QString id = rm::find_default_interactive_id(model);
    if (!id.isEmpty()) {
      qInfo().noquote() << "starting interactive collection automatically:" << id;
      controller.playNow(id);
    } else {
      qWarning() << "no interactive collection found in the library; staying idle";
    }
    exit_code = app.exec();
  } else {
    rm::LibraryFilter filter;
    filter.setSourceModel(&model);

    QQuickView view;
    view.setTitle("raspberry milkshake");
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.engine()->addImageProvider("svg", new rm::SvgImageProvider);
    view.setInitialProperties({{"libraryFilter", QVariant::fromValue<QObject*>(&filter)},
                               {"controller", QVariant::fromValue<QObject*>(&controller)},
                               {"tags", model.allTags()}});
    view.loadFromModule("Milkshake", "HomeScreen");
    if (view.status() == QQuickView::Ready) {
      view.resize(1280, 720);
      view.show();
      exit_code = app.exec();
    } else {
      qCritical() << "could not load the home screen";
    }
  }

  // Stop everything that points at the engine before the render thread (and the engine) goes away.
  bridge.reset();
  dashboard.set_rating_handler({});
  dashboard.stop();
  stop = true;
  renderer.join();
  return exit_code;
}
