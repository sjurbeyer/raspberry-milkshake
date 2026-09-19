// rm-devserver: runs a collection through the real sequencer and projectM host on a software GL
// context and shows the result in a browser: the main output as MJPEG, the LED chain as live colours.
// Dev tool for the container; the Pi build drives real outputs instead.

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/demo_audio.hpp"
#include "core/dir_catalog.hpp"
#include "core/led_layout.hpp"
#include "core/manifest.hpp"
#include "core/sequencer.hpp"
#include "render/downscaler.hpp"
#include "render/egl_context.hpp"
#include "render/jpeg.hpp"
#include "render/projectm_host.hpp"
#include "render/show_runner.hpp"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

struct Options {
  fs::path collection = "examples/dev/demo-loop";
  fs::path presets = "/presets";
  fs::path fallback_presets = "tests/fixtures/presets";
  fs::path web = "web";
  fs::path brand = "docs/raspberry-milkshake-visual-profile/raspberry-milkshake-brand";
  int port = 8080;
  int fps = 30;
  int width = 640;
  int height = 360;
};

bool parse_options(int argc, char** argv, Options& o) {
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    if (i + 1 >= argc) {
      std::cerr << "missing value for " << key << "\n";
      return false;
    }
    const std::string value = argv[++i];
    if (key == "--collection") o.collection = value;
    else if (key == "--presets") o.presets = value;
    else if (key == "--web") o.web = value;
    else if (key == "--brand") o.brand = value;
    else if (key == "--port") o.port = std::stoi(value);
    else if (key == "--fps") o.fps = std::stoi(value);
    else if (key == "--width") o.width = std::stoi(value);
    else if (key == "--height") o.height = std::stoi(value);
    else {
      std::cerr << "unknown option " << key << "\n";
      return false;
    }
  }
  return true;
}

std::string json_escape(const std::string& s) {
  std::string out;
  for (const char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) out += ' ';
        else out += c;
    }
  }
  return out;
}

class MusicSource final : public rm::IPcmSource {
 public:
  void read_stereo(rm::SamplePos start, unsigned frames, float* out) const override {
    music_.read_stereo(start, frames, out);
  }

 private:
  rm::SyntheticMusic music_{120.0};
};

// What the render loop publishes and the HTTP threads read.
struct Shared {
  std::mutex mutex;
  std::condition_variable cv;
  std::uint64_t seq = 0;
  std::vector<std::uint8_t> jpeg;
  std::string leds_json = "{\"hex\":\"\"}";
  std::string state_json = "{}";
  std::string layout_json = "{\"points\":[]}";
};

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  if (!parse_options(argc, argv, opt)) return 2;

  const auto manifest = rm::parse_manifest(read_file(opt.collection / "collection.toml"));
  for (const auto& d : manifest.diagnostics) {
    std::cerr << (d.severity == rm::Severity::Error ? "error: " : "warning: ") << d.path << " " << d.message << "\n";
  }
  if (!manifest.ok()) return 1;
  const rm::Collection& collection = *manifest.collection;
  if (collection.tracks.empty()) {
    std::cerr << "the collection has no tracks to play\n";
    return 1;
  }

  fs::path preset_root = opt.presets;
  rm::DirCatalog catalog(preset_root);
  if (catalog.size() == 0) {
    std::cerr << "no .milk presets under " << preset_root.string()
              << "; using the small built-in test presets.\n"
              << "run `docker compose run --rm fetch-presets` to download a real preset pack.\n";
    preset_root = opt.fallback_presets;
    catalog = rm::DirCatalog(preset_root);
  }
  std::cerr << catalog.size() << " presets from " << preset_root.string() << "\n";

  std::string error;
  auto egl = rm::EglContext::create_headless(opt.width, opt.height, error);
  if (!egl) {
    std::cerr << "cannot create an OpenGL ES context: " << error << "\n";
    return 1;
  }
  auto host = rm::ProjectMHost::create(rm::HostConfig{opt.width, opt.height, 32, 24, opt.fps}, error);
  if (!host) {
    std::cerr << "cannot start projectM: " << error << "\n";
    return 1;
  }
  rm::Downscaler scaler(64, 36);
  const rm::LedLayout layout = rm::perimeter_layout(24, 14, 24, 14, 0.08f);

  constexpr rm::SamplePos kTrackLength = 120 * rm::kSampleRate;
  rm::Sequencer sequencer(collection, 0, kTrackLength, catalog);
  for (const auto& w : sequencer.warnings()) std::cerr << "warning: " << w << "\n";
  const MusicSource music;
  rm::ShowRunner runner(*host, sequencer, music, rm::RunnerConfig{opt.fps, 0, 512, preset_root});

  Shared shared;
  {
    std::ostringstream layout_json;
    layout_json << "{\"points\":[";
    for (std::size_t i = 0; i < layout.points.size(); ++i) {
      char buf[64];
      std::snprintf(buf, sizeof buf, "%s[%.4f,%.4f]", i ? "," : "", static_cast<double>(layout.points[i].u),
                    static_cast<double>(layout.points[i].v));
      layout_json << buf;
    }
    layout_json << "]}";
    shared.layout_json = layout_json.str();
  }

  httplib::Server server;
  server.set_mount_point("/", opt.web.string());
  server.set_mount_point("/brand", opt.brand.string());
  auto json_route = [&](const char* path, std::string Shared::*field) {
    server.Get(path, [&shared, field](const httplib::Request&, httplib::Response& res) {
      std::lock_guard lock(shared.mutex);
      res.set_header("Cache-Control", "no-store");
      res.set_content(shared.*field, "application/json");
    });
  };
  json_route("/layout.json", &Shared::layout_json);
  json_route("/leds.json", &Shared::leds_json);
  json_route("/state.json", &Shared::state_json);

  server.Get("/frame.jpg", [&shared](const httplib::Request&, httplib::Response& res) {
    std::lock_guard lock(shared.mutex);
    res.set_header("Cache-Control", "no-store");
    res.set_content(reinterpret_cast<const char*>(shared.jpeg.data()), shared.jpeg.size(), "image/jpeg");
  });

  server.Get("/main.mjpg", [&shared](const httplib::Request&, httplib::Response& res) {
    res.set_header("Cache-Control", "no-store");
    res.set_chunked_content_provider(
        "multipart/x-mixed-replace; boundary=frame",
        [&shared, last = std::uint64_t{0}](std::size_t, httplib::DataSink& sink) mutable {
          std::vector<std::uint8_t> jpeg;
          {
            std::unique_lock lock(shared.mutex);
            if (!shared.cv.wait_for(lock, 1s, [&] { return shared.seq != last || g_stop.load(); })) return true;
            if (g_stop) return false;
            jpeg = shared.jpeg;
            last = shared.seq;
          }
          const std::string head = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                   std::to_string(jpeg.size()) + "\r\n\r\n";
          if (!sink.write(head.data(), head.size())) return false;
          if (!sink.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size())) return false;
          return sink.write("\r\n", 2);
        });
  });

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);
  std::thread http([&] { server.listen("0.0.0.0", opt.port); });
  std::cerr << "dashboard on http://localhost:" << opt.port << " (renderer: " << egl->renderer() << ")\n";

  const auto period = std::chrono::microseconds(1'000'000 / opt.fps);
  auto next = std::chrono::steady_clock::now();
  auto previous_start = next;
  bool have_previous = false;
  double fps_smoothed = opt.fps;
  double render_ms = 0;

  while (!g_stop) {
    const auto start = std::chrono::steady_clock::now();
    const auto step = runner.step();
    const rm::Frame full = host->read_frame();
    const rm::Frame small = scaler.capture(opt.width, opt.height);
    const auto colors = rm::sample_leds(small, layout);
    std::vector<std::uint8_t> jpeg = rm::encode_jpeg(full, 75);

    if (step.media_pos >= kTrackLength) runner.seek(0);

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
    std::ostringstream state;
    char numbers[160];
    std::snprintf(numbers, sizeof numbers, "\"frame\":%lld,\"media_seconds\":%.2f,\"fps\":%.1f,\"render_ms\":%.1f",
                  static_cast<long long>(step.frame),
                  static_cast<double>(step.media_pos) / static_cast<double>(rm::kSampleRate), fps_smoothed, render_ms);
    state << "{\"preset\":\"" << json_escape(runner.current_preset()) << "\"," << numbers << ",\"renderer\":\""
          << json_escape(egl->renderer()) << "\",\"preset_count\":" << catalog.size()
          << ",\"errors\":" << runner.errors().size() << ",\"last_error\":\""
          << json_escape(runner.errors().empty() ? "" : runner.errors().back()) << "\",\"width\":" << opt.width
          << ",\"height\":" << opt.height << ",\"target_fps\":" << opt.fps << ",\"leds\":" << colors.size() << "}";
    {
      std::lock_guard lock(shared.mutex);
      shared.jpeg = std::move(jpeg);
      shared.leds_json = "{\"hex\":\"" + hex + "\"}";
      shared.state_json = state.str();
      ++shared.seq;
    }
    shared.cv.notify_all();

    next += period;
    if (done < next) std::this_thread::sleep_until(next);
    else next = done;  // running behind: do not try to catch up
  }

  shared.cv.notify_all();
  server.stop();
  http.join();
  return 0;
}
