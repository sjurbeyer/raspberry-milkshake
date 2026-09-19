#include "render/dashboard_server.hpp"

#include <httplib.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace rm {

using namespace std::chrono_literals;

std::string json_escape(std::string_view text) {
  std::string out;
  for (const char c : text) {
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

struct DashboardServer::Impl {
  DashboardOptions options;
  httplib::Server server;
  std::thread thread;
  std::atomic<bool> stopping{false};

  std::mutex mutex;
  std::condition_variable cv;
  std::uint64_t seq = 0;
  std::vector<std::uint8_t> jpeg;
  std::string leds_json = "{\"hex\":\"\"}";
  std::string state_json = "{}";
  std::string layout_json;
  RatingHandler rating_handler;  // guarded by mutex
};

DashboardServer::DashboardServer(DashboardOptions options, std::string layout_json) : impl_(std::make_unique<Impl>()) {
  Impl& s = *impl_;
  s.options = std::move(options);
  s.layout_json = std::move(layout_json);

  s.server.set_mount_point("/", s.options.web.string());
  s.server.set_mount_point("/brand", s.options.brand.string());

  auto json_route = [&s](const char* path, std::string Impl::*field) {
    s.server.Get(path, [&s, field](const httplib::Request&, httplib::Response& res) {
      std::lock_guard lock(s.mutex);
      res.set_header("Cache-Control", "no-store");
      res.set_content(s.*field, "application/json");
    });
  };
  json_route("/layout.json", &Impl::layout_json);
  json_route("/leds.json", &Impl::leds_json);
  json_route("/state.json", &Impl::state_json);

  s.server.Post("/rate", [&s](const httplib::Request& req, httplib::Response& res) {
    res.set_header("Cache-Control", "no-store");
    const std::string preset = req.get_param_value("preset");
    const std::string text = req.get_param_value("rating");
    int rating = -1;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), rating);
    RatingHandler handler;
    {
      std::lock_guard lock(s.mutex);
      handler = s.rating_handler;
    }
    const bool valid = !preset.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
                       rating >= 0 && rating <= 5;
    const bool ok = valid && handler && handler(preset, rating);
    res.status = ok ? 200 : 400;
    res.set_content(ok ? "{\"ok\":true}" : "{\"ok\":false}", "application/json");
  });

  s.server.Get("/frame.jpg", [&s](const httplib::Request&, httplib::Response& res) {
    std::lock_guard lock(s.mutex);
    res.set_header("Cache-Control", "no-store");
    res.set_content(reinterpret_cast<const char*>(s.jpeg.data()), s.jpeg.size(), "image/jpeg");
  });

  s.server.Get("/main.mjpg", [&s](const httplib::Request&, httplib::Response& res) {
    res.set_header("Cache-Control", "no-store");
    res.set_chunked_content_provider(
        "multipart/x-mixed-replace; boundary=frame",
        [&s, last = std::uint64_t{0}](std::size_t, httplib::DataSink& sink) mutable {
          std::vector<std::uint8_t> jpeg;
          {
            std::unique_lock lock(s.mutex);
            if (!s.cv.wait_for(lock, 1s, [&] { return s.seq != last || s.stopping.load(); })) return true;
            if (s.stopping) return false;
            jpeg = s.jpeg;
            last = s.seq;
          }
          const std::string head = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                   std::to_string(jpeg.size()) + "\r\n\r\n";
          if (!sink.write(head.data(), head.size())) return false;
          if (!sink.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size())) return false;
          return sink.write("\r\n", 2);
        });
  });
}

DashboardServer::~DashboardServer() { stop(); }

void DashboardServer::start() {
  Impl& s = *impl_;
  s.thread = std::thread([&s] { s.server.listen("0.0.0.0", s.options.port); });
}

void DashboardServer::stop() {
  Impl& s = *impl_;
  s.stopping = true;
  s.cv.notify_all();
  s.server.stop();
  if (s.thread.joinable()) s.thread.join();
}

void DashboardServer::set_rating_handler(RatingHandler handler) {
  std::lock_guard lock(impl_->mutex);
  impl_->rating_handler = std::move(handler);
}

void DashboardServer::publish(std::vector<std::uint8_t> jpeg, std::string leds_json, std::string state_json) {
  Impl& s = *impl_;
  {
    std::lock_guard lock(s.mutex);
    s.jpeg = std::move(jpeg);
    s.leds_json = std::move(leds_json);
    s.state_json = std::move(state_json);
    ++s.seq;
  }
  s.cv.notify_all();
}

}  // namespace rm
