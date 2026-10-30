#include "hush/bus_graph.hpp"
#include "hush/preallocated_bus_processor.hpp"
#include <cstdio>
#include <vector>
static int fails=0;
#define CHECK(c,m) do{if(!(c)){std::printf("FAIL %s\n",m);++fails;}else std::printf("PASS %s\n",m);}while(0)
int main() {
  using namespace hush;
  BusGraph g;
  RoutingError err;
  CHECK(g.configure({{"a","A"},{"m","Master"}},
                    {{"a","m",SendTap::PostFader,1.f}}, err), "graph");
  PreallocatedBusProcessor p;
  ProcessorSetupError se;
  CHECK(p.configure(g, 64, se), "configure");
  std::vector<float> L(64, 0.5f), R(64, 0.25f);
  BusInputView ins[2] = {{L.data(), R.data(), 1.f}, {L.data(), R.data(), 1.f}};
  CHECK(p.process(ins, 2, 64)==RealtimeProcessStatus::Ok, "process ok");
  CHECK(p.process(ins, 2, 128)==RealtimeProcessStatus::FrameCountExceedsCapacity, "frames cap");
  CHECK(p.process(nullptr, 2, 16)==RealtimeProcessStatus::NullChannel, "null");
  BusInputView bad = {L.data(), R.data(), 99.f};
  BusInputView badins[2] = {bad, bad};
  CHECK(p.process(badins, 2, 16)==RealtimeProcessStatus::InvalidFaderGain, "fader");
  auto post = p.post_fader(0, 64);
  CHECK(post.left && post.right && post.frames==64, "post view");
  std::printf("\nprealloc failures: %d\n", fails);
  return fails?1:0;
}
