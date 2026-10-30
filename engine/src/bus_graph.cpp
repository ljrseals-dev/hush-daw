#include "hush/bus_graph.hpp"
#include <cmath>
#include <functional>
#include <unordered_map>

namespace hush {

bool BusGraph::configure(std::vector<Bus> buses, std::vector<BusRoute> routes, RoutingError& error) {
  valid_ = false;
  buses_.clear();
  routes_.clear();
  order_.clear();

  std::unordered_map<std::string, std::size_t> index;
  index.reserve(buses.size());
  for (std::size_t i = 0; i < buses.size(); ++i) {
    if (buses[i].id.empty()) {
      error = {"bus_id_empty", "", "Bus ID must not be empty"};
      return false;
    }
    if (!index.emplace(buses[i].id, i).second) {
      error = {"bus_id_duplicate", buses[i].id, "Bus IDs must be unique"};
      return false;
    }
  }

  std::vector<std::vector<std::size_t>> edges(buses.size());
  for (const auto& route : routes) {
    const auto source = index.find(route.source_bus_id);
    const auto destination = index.find(route.destination_bus_id);
    if (source == index.end() || destination == index.end()) {
      const std::string id =
          source == index.end() ? route.source_bus_id : route.destination_bus_id;
      error = {"bus_route_unknown_bus", id, "Route references an unknown bus"};
      return false;
    }
    if (source->second == destination->second) {
      error = {"bus_route_self_cycle", route.source_bus_id,
               "A bus cannot route directly to itself"};
      return false;
    }
    if (!std::isfinite(route.gain) || route.gain < 0.0F || route.gain > 16.0F) {
      error = {"bus_route_gain_invalid", route.source_bus_id,
               "Route gain must be finite and in [0, 16]"};
      return false;
    }
    edges[source->second].push_back(destination->second);
  }

  // 0 = unseen, 1 = active DFS stack, 2 = complete
  std::vector<unsigned char> state(buses.size(), 0);
  std::vector<std::size_t> postorder;
  postorder.reserve(buses.size());

  std::function<bool(std::size_t)> visit = [&](std::size_t node) {
    state[node] = 1;
    for (const auto next : edges[node]) {
      if (state[next] == 1) return false;
      if (state[next] == 0 && !visit(next)) return false;
    }
    state[node] = 2;
    postorder.push_back(node);
    return true;
  };

  for (std::size_t i = 0; i < buses.size(); ++i) {
    if (state[i] == 0 && !visit(i)) {
      error = {"bus_route_cycle", buses[i].id, "Bus routing contains a directed cycle"};
      return false;
    }
  }

  for (auto it = postorder.rbegin(); it != postorder.rend(); ++it)
    order_.push_back(buses[*it].id);

  buses_ = std::move(buses);
  routes_ = std::move(routes);
  valid_ = true;
  error = {};
  return true;
}

}  // namespace hush
