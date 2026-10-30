#include "hush/bus_graph.hpp"
#include "hush/realtime_bus_engine.hpp"
#include <cstdio>
#include <vector>
static int fails=0;
#define CHECK(c,m) do{if(!(c)){std::printf("FAIL %s\n",m);++fails;}else std::printf("PASS %s\n",m);}while(0)
int main() {
  using namespace hush;
  BusGraph g; RoutingError e;
  CHECK(g.configure({{"in","In"},{"m","Master"}},
                    {{"in","m",SendTap::PostFader,1.f}}, e), "graph");
  RealtimeBusEngine eng;
  ProcessorSetupError se;
  CHECK(eng.configure(g, 32, se), "configure");
  CHECK(eng.push_param({0, 1.0f, 0}), "push gain");
  std::vector<float> in(64, 0.5f), out(64, 0.f);
  CHECK(eng.process_interleaved(in.data(), out.data(), 32, 0, 1), "interleaved");
  // queue full does not block
  for (int i=0;i<300;i++) eng.push_param({0, 0.5f, 0});
  CHECK(eng.process_interleaved(in.data(), out.data(), 32, 0, 1), "after flood");
  CHECK(eng.process_interleaved(in.data(), out.data(), 64, 0, 1)==false, "oversize false");
  std::printf("\nrealtime failures: %d\n", fails);
  return fails?1:0;
}
