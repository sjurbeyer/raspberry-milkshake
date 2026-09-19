#include <catch2/catch_test_macros.hpp>

#include <httplib.h>

#include <chrono>
#include <thread>

#include "render/dashboard_server.hpp"

using namespace rm;

namespace {

constexpr int kPort = 38199;

struct Running {
  Running() : server(options(), "{\"points\":[]}") {
    server.start();
    httplib::Client waiter("127.0.0.1", kPort);
    for (int i = 0; i < 100; ++i) {  // wait for the listener
      if (waiter.Get("/layout.json")) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
  ~Running() { server.stop(); }
  static DashboardOptions options() {
    DashboardOptions o;
    o.port = kPort;
    o.web = "/no/such/folder";
    o.brand = "/no/such/folder";
    return o;
  }
  DashboardServer server;
  httplib::Client client{"127.0.0.1", kPort};
};

}  // namespace

TEST_CASE("published state and LED colours are served", "[dashboard]") {
  Running r;
  r.server.publish({0xFF, 0xD8, 0xFF, 0xD9}, "{\"hex\":\"ff0000\"}", "{\"mode\":\"idle\"}");
  const auto state = r.client.Get("/state.json");
  REQUIRE(state);
  REQUIRE(state->body == "{\"mode\":\"idle\"}");
  REQUIRE(r.client.Get("/leds.json")->body == "{\"hex\":\"ff0000\"}");
  REQUIRE(r.client.Get("/layout.json")->body == "{\"points\":[]}");
  const auto frame = r.client.Get("/frame.jpg");
  REQUIRE(frame);
  REQUIRE(frame->body.size() == 4);
}

TEST_CASE("POST /rate calls the handler with the preset and rating", "[dashboard][rating]") {
  Running r;
  std::string got_preset;
  int got_rating = -1;
  int calls = 0;
  r.server.set_rating_handler([&](const std::string& preset, int rating) {
    got_preset = preset;
    got_rating = rating;
    ++calls;
    return true;
  });

  const auto res =
      r.client.Post("/rate", httplib::Params{{"preset", "cream-of-the-crop/Some Dir/a & b.milk"}, {"rating", "0"}});
  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(res->body == "{\"ok\":true}");
  REQUIRE(calls == 1);
  REQUIRE(got_preset == "cream-of-the-crop/Some Dir/a & b.milk");
  REQUIRE(got_rating == 0);
}

TEST_CASE("POST /rate rejects bad requests without calling the handler", "[dashboard][rating]") {
  Running r;
  int calls = 0;
  r.server.set_rating_handler([&](const std::string&, int) {
    ++calls;
    return true;
  });
  for (const char* rating : {"6", "-1", "abc", "", "3x"}) {
    INFO(rating);
    const auto res = r.client.Post("/rate", httplib::Params{{"preset", "a.milk"}, {"rating", rating}});
    REQUIRE(res);
    REQUIRE(res->status == 400);
  }
  const auto no_preset = r.client.Post("/rate", httplib::Params{{"rating", "2"}});
  REQUIRE(no_preset->status == 400);
  REQUIRE(calls == 0);
}

TEST_CASE("POST /rate reports failure when the handler refuses or is missing", "[dashboard][rating]") {
  Running r;
  const auto none = r.client.Post("/rate", httplib::Params{{"preset", "a.milk"}, {"rating", "0"}});
  REQUIRE(none->status == 400);

  r.server.set_rating_handler([](const std::string&, int) { return false; });
  const auto refused = r.client.Post("/rate", httplib::Params{{"preset", "a.milk"}, {"rating", "0"}});
  REQUIRE(refused->status == 400);
  REQUIRE(refused->body == "{\"ok\":false}");
}
