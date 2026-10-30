#include "hush/bus_graph.hpp"
#include <cmath>
#include <cstdio>
static int fails=0;
#define CHECK(c,m) do{if(!(c)){std::printf("FAIL %s\n",m);++fails;}else std::printf("PASS %s\n",m);}while(0)
int main() {
  using namespace hush;
  RoutingError err;
  {
    BusGraph g;
    CHECK(g.configure({{"a","A"},{"b","B"},{"m","Master"}},
                      {{"a","m",SendTap::PostFader,1.f},{"b","m",SendTap::PostFader,0.5f}},
                      err) && g.valid(), "acyclic");
    CHECK(g.render_order().size()==3, "order");
  }
  {
    BusGraph g;
    CHECK(!g.configure({{"a","A"},{"b","B"}},
                       {{"a","b",SendTap::PostFader,1.f},{"b","a",SendTap::PostFader,1.f}},
                       err) && err.code=="bus_route_cycle", "cycle");
  }
  {
    BusGraph g;
    CHECK(!g.configure({{"a","A"}}, {{"a","a",SendTap::PostFader,1.f}}, err) &&
          err.code=="bus_route_self_cycle", "self");
  }
  {
    BusGraph g;
    CHECK(!g.configure({{"a","A"},{"m","M"}},
                       {{"a","m",SendTap::PostFader,99.f}}, err) &&
          err.code=="bus_route_gain_invalid", "gain high");
  }
  {
    BusGraph g;
    CHECK(!g.configure({{"a","A"},{"m","M"}},
                       {{"a","m",SendTap::PostFader,std::nanf("")}}, err) &&
          err.code=="bus_route_gain_invalid", "gain nan");
  }
  {
    BusGraph g;
    CHECK(!g.configure({{"a","A"},{"a","dup"}}, {}, err) &&
          err.code=="bus_id_duplicate", "dup id");
  }
  std::printf("\nbus_graph failures: %d\n", fails);
  return fails?1:0;
}
