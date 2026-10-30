#include "hush/bus_graph.hpp"
#include "hush/mixer_engine_bridge.hpp"
#include "hush/realtime_bus_audio_adapter.hpp"
#include "hush/realtime_bus_engine.hpp"
#include "hush/realtime_bus_engine_host.hpp"
#include "hush/realtime_bus_engine_host_audio_adapter.hpp"
#include "hush/stereo_adapter.hpp"
#include <chrono>
#include <cstdio>
#include <vector>
static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL %s\n", m); ++fails; } else std::printf("PASS %s\n", m); } while (0)

int main() {
  using namespace hush;
  BusGraph g;
  RoutingError err;
  CHECK(g.configure({{"in", "In"}, {"fx", "FX"}, {"m", "Master"}},
                    {{"in", "m", SendTap::PostFader, 1.f},
                     {"in", "fx", SendTap::PreFader, 0.5f},
                     {"fx", "m", SendTap::PostFader, 1.f}}, err), "graph");

  // Stereo round-trip fidelity
  {
    StereoScratch s;
    s.configure(64);
    std::vector<float> inter(128);
    for (int i = 0; i < 128; ++i) inter[i] = float(i) * 0.01f;
    CHECK(s.deinterleave(inter.data(), 64), "deint");
    std::vector<float> out(128, 0.f);
    CHECK(s.interleave(out.data(), 64), "int");
    bool ok = true;
    for (int i = 0; i < 128; ++i) if (out[i] != inter[i]) ok = false;
    CHECK(ok, "stereo fidelity");
  }

  // Audio adapter
  {
    RealtimeBusEngine eng;
    ProcessorSetupError se;
    CHECK(eng.configure(g, 64, se), "eng");
    RealtimeBusAudioAdapter ad;
    std::string e;
    CHECK(ad.configure(eng, 0, 2, 64, e), "audio ad");
    std::vector<float> in(128, 0.5f), out(128, 0.f);
    CHECK(ad.process(in.data(), out.data(), 64), "process");
    // Silence path
    CHECK(ad.process(nullptr, out.data(), 64), "null input");
    {
    std::vector<float> big(256, 0.f);
    CHECK(!ad.process(in.data(), big.data(), 128), "oversize reject");
  }
  }

  // Mixer
  {
    RealtimeBusEngine eng;
    ProcessorSetupError se;
    eng.configure(g, 32, se);
    MixerEngineBridge br;
    std::string e;
    CHECK(br.configure(eng, {{"a", 0}, {"fx", 1}, {"m", 2}}, e), "mixer");
    CHECK(br.enqueue_gain("a", 0.7f), "gain");
    CHECK(br.enqueue_mute("fx", true), "mute");
    CHECK(!br.enqueue_gain("missing", 1.f), "missing");
    CHECK(!br.enqueue_gain("a", -1.f), "neg");
  }

  // Host + host adapter
  {
    RealtimeBusEngineHost host;
    ProcessorSetupError se;
    CHECK(host.replace_graph(g, 64, "m", se), "replace");
    CHECK(host.bus_count_for_control() == 3, "nbuses");
    RealtimeBusEngineHostAudioAdapter had;
    std::string e;
    CHECK(had.configure(host, 0, 3, 64, e), "host ad");
    std::vector<float> in(128, 0.25f), out(128, -1.f);
    CHECK(had.process(in.data(), out.data(), 64), "host process");
    // Replace graph while adapter configured (same topology size)
    CHECK(host.replace_graph(g, 64, "m", se), "replace2");
    host.reclaim_retired();
    CHECK(had.process(in.data(), out.data(), 64), "process after replace");
  }

  // Microbench: 50k blocks × 64 frames stereo through audio adapter
  {
    RealtimeBusEngine eng;
    ProcessorSetupError se;
    eng.configure(g, 64, se);
    RealtimeBusAudioAdapter ad;
    std::string e;
    ad.configure(eng, 0, 2, 64, e);
    std::vector<float> in(128, 0.1f), out(128, 0.f);
    const int N = 50000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
      if (!ad.process(in.data(), out.data(), 64)) ++fails;
    }
    auto t1 = std::chrono::steady_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double us_per_block = (ms * 1000.0) / N;
    std::printf("BENCH audio_adapter: %.2f ms / %d blocks (%.3f µs/block @ 64 frames)\n",
                ms, N, us_per_block);
    // 64 frames @ 48kHz = 1.333 ms block period; require well under that
    CHECK(us_per_block < 200.0, "perf under 200µs/block");
  }

  std::printf("\ncosmic adapter failures: %d\n", fails);
  return fails ? 1 : 0;
}
