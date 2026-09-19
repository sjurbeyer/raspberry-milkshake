#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rm {

std::string json_escape(std::string_view text);

struct DashboardOptions {
  int port = 8080;
  std::filesystem::path web = "web";
  std::filesystem::path brand = "docs/raspberry-milkshake-visual-profile/raspberry-milkshake-brand";
};

// Serves the dev dashboard: the page, the brand assets, an MJPEG stream of the main output, and
// small JSON documents for the LED chain and the status pane. The render loop calls publish() each
// frame; HTTP handlers run on their own threads and only read the latest published values.
class DashboardServer {
 public:
  DashboardServer(DashboardOptions options, std::string layout_json);
  ~DashboardServer();
  DashboardServer(const DashboardServer&) = delete;
  DashboardServer& operator=(const DashboardServer&) = delete;

  void start();  // listens on a background thread
  void stop();

  void publish(std::vector<std::uint8_t> jpeg, std::string leds_json, std::string state_json);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rm
