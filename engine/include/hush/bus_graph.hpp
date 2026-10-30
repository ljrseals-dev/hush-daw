#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hush {

enum class SendTap { PreFader, PostFader };

struct Bus {
  std::string id;
  std::string name;
  bool operator==(const Bus&) const = default;
};

struct BusRoute {
  std::string source_bus_id;
  std::string destination_bus_id;
  SendTap tap{SendTap::PostFader};
  float gain{1.0F};
  bool operator==(const BusRoute&) const = default;
};

struct RoutingError {
  std::string code;
  std::string entity_id;
  std::string message;
};

// Setup/control-side validator. Never call configure() from an audio callback.
class BusGraph {
public:
  bool configure(std::vector<Bus> buses, std::vector<BusRoute> routes, RoutingError& error);
  [[nodiscard]] bool valid() const noexcept { return valid_; }
  [[nodiscard]] const std::vector<Bus>& buses() const noexcept { return buses_; }
  [[nodiscard]] const std::vector<BusRoute>& routes() const noexcept { return routes_; }
  [[nodiscard]] std::vector<std::string> render_order() const { return order_; }

private:
  std::vector<Bus> buses_;
  std::vector<BusRoute> routes_;
  std::vector<std::string> order_;
  bool valid_{false};
};

}  // namespace hush
