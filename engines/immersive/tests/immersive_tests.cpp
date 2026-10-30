// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/adm.hpp"
#include "hush/immersive/ambisonics.hpp"
#include "hush/immersive/binaural.hpp"
#include "hush/immersive/channel_format.hpp"
#include "hush/immersive/json_util.hpp"
#include "hush/immersive/panner.hpp"
#include "hush/immersive/renderer.hpp"
#include "hush/immersive/resample.hpp"
#include "hush/immersive/speakers.hpp"
#include "hush/immersive/transport.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace hush::immersive;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "FAIL: " << #cond << " at " << __FILE__ << ":" << __LINE__  \
                << "\n";                                                       \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                                  \
  do {                                                                         \
    if (std::abs((a) - (b)) > (eps)) {                                         \
      std::cerr << "FAIL: " << #a << " (" << (a) << ") near " << #b << " ("    \
                << (b) << ") eps=" << (eps) << " at " << __FILE__ << ":"       \
                << __LINE__ << "\n";                                           \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main() {
  // 1. Channel counts
  CHECK(channelCount(ChannelFormat::Mono) == 1);
  CHECK(channelCount(ChannelFormat::Stereo) == 2);
  CHECK(channelCount(ChannelFormat::LCR) == 3);
  CHECK(channelCount(ChannelFormat::Quad) == 4);
  CHECK(channelCount(ChannelFormat::FivePointOne) == 6);
  CHECK(channelCount(ChannelFormat::SevenPointOne) == 8);
  CHECK(channelCount(ChannelFormat::FivePointOneFour) == 10);
  CHECK(channelCount(ChannelFormat::SevenPointOneFour) == 12);
  CHECK(channelCount(ChannelFormat::NinePointOneSix) == 16);
  CHECK(channelCount(ChannelFormat::AmbisonicsFOA) == 4);
  CHECK(channelCount(ChannelFormat::AmbisonicsSOA) == 9);
  CHECK(channelCount(ChannelFormat::AmbisonicsHOA3) == 16);

  // 2. speakerFor throws
  try {
    speakerFor(static_cast<ChannelFormat>(255), 0);
    CHECK(false);
  } catch (const std::invalid_argument&) {
  }

  // 3. FOA encoding
  {
    auto v = encodeAmbisonics(0.f, 0.f, ChannelFormat::AmbisonicsFOA);
    CHECK(v.size() == 4);
    CHECK_NEAR(v[0], 1.f, 1e-5f);
    CHECK_NEAR(v[1], 0.f, 1e-5f);
    CHECK_NEAR(v[2], 0.f, 1e-5f);
    CHECK_NEAR(v[3], 1.f, 1e-5f);
  }
  {
    auto v = encodeAmbisonics(90.f, 0.f, ChannelFormat::AmbisonicsFOA);
    CHECK_NEAR(v[0], 1.f, 1e-5f);
    CHECK_NEAR(v[1], 1.f, 1e-5f);
    CHECK_NEAR(v[2], 0.f, 1e-5f);
    CHECK_NEAR(v[3], 0.f, 1e-5f);
  }
  {
    auto v = encodeAmbisonics(0.f, 90.f, ChannelFormat::AmbisonicsFOA);
    CHECK_NEAR(v[0], 1.f, 1e-5f);
    CHECK_NEAR(v[1], 0.f, 1e-5f);
    CHECK_NEAR(v[2], 1.f, 1e-5f);
    CHECK_NEAR(v[3], 0.f, 1e-5f);
  }

  // 4. SOA / HOA3 energy (unit-gain SN3D: order-n block energy ≈ 1 for plane wave)
  {
    auto v = encodeAmbisonics(30.f, 20.f, ChannelFormat::AmbisonicsSOA);
    CHECK_NEAR(v[0] * v[0], 1.f, 0.1f);
    float e1 = v[1]*v[1] + v[2]*v[2] + v[3]*v[3];
    CHECK_NEAR(e1, 1.f, 0.5f);
    float e2 = 0.f;
    for (int i = 4; i <= 8; ++i) e2 += v[i]*v[i];
    CHECK(e2 > 0.1f);
  }
  {
    auto v = encodeAmbisonics(45.f, 30.f, ChannelFormat::AmbisonicsHOA3);
    float e3 = 0.f;
    for (int i = 9; i <= 15; ++i) e3 += v[i]*v[i];
    CHECK(e3 > 0.05f);
  }

  // 5. Distance attenuation
  {
    PannerState s0, s2;
    s0.isObject = true;
    s0.azimuthDeg = 0.f;
    s0.distance = 0.f;
    s0.gain = 1.f;
    s2 = s0;
    s2.distance = 2.f;
    auto g0 = pan(s0, ChannelFormat::Stereo);
    auto g2 = pan(s2, ChannelFormat::Stereo);
    float e0 = 0.f, e2 = 0.f;
    for (float g : g0) e0 += g * g;
    for (float g : g2) e2 += g * g;
    float ratio = (e0 > 1e-12f) ? e2 / e0 : 0.f;
    CHECK_NEAR(ratio, 1.f / 16.f, 0.05f);
  }

  // 6. screenAnchor
  {
    PannerState s;
    s.isObject = true;
    s.azimuthDeg = 90.f;
    s.screenAnchor = 1.f;
    s.gain = 1.f;
    auto g = pan(s, ChannelFormat::Stereo);
    CHECK(std::abs(g[0] - g[1]) < 0.3f);
  }

  // 7. Binaural fallback
  {
    BinauralRenderer br;
    CHECK(!br.usingSofa());
    const int nSp = 2, nFr = 128;
    std::vector<float> in(static_cast<std::size_t>(nSp * nFr), 0.5f);
    std::vector<float> out(static_cast<std::size_t>(2 * nFr), 0.f);
    br.process(in.data(), nSp, nFr, out.data());
    float energy = 0.f;
    for (float v : out) energy += v * v;
    CHECK(energy > 1e-6f);
  }

  // 8. ADM writer
  {
    const int nCh = 2, nFr = 64, sr = 48000;
    std::vector<float> samples(static_cast<std::size_t>(nCh * nFr), 0.1f);
    std::vector<AdmObject> objs;
    AdmObject o;
    o.name = "testObj";
    o.panner.azimuthDeg = 30.f;
    o.firstChannel = 0;
    o.numChannels = 1;
    objs.push_back(o);
    const std::string path = "test_adm.w64";
    exportAdm(path, samples, nCh, nFr, sr, ChannelFormat::Stereo, objs);
    std::ifstream ifs(path, std::ios::binary);
    CHECK(ifs.good());
    char magic[4];
    ifs.read(magic, 4);
    CHECK(std::string(magic, 4) == "BW64");
    std::uint32_t riffSz = 0;
    ifs.read(reinterpret_cast<char*>(&riffSz), 4);
    char form[4];
    ifs.read(form, 4);
    CHECK(std::string(form, 4) == "WAVE");
    bool foundChna = false, foundAxml = false;
    while (ifs) {
      char id[4];
      ifs.read(id, 4);
      if (!ifs) break;
      std::uint32_t sz = 0;
      ifs.read(reinterpret_cast<char*>(&sz), 4);
      if (!ifs) break;
      if (std::string(id, 4) == "chna") foundChna = true;
      if (std::string(id, 4) == "axml") {
        foundAxml = true;
        CHECK(sz > 100);
      }
      ifs.seekg(sz, std::ios::cur);
    }
    CHECK(foundChna);
    CHECK(foundAxml);
    std::remove(path.c_str());
  }

  // 9. Determinism
  {
    Renderer r(ChannelFormat::Stereo, 48000);
    std::vector<float> mono(256, 0.25f);
    PannerState st;
    st.azimuthDeg = 15.f;
    st.gain = 0.8f;
    std::vector<float> out1(512, 0.f), out2(512, 0.f);
    r.process(mono.data(), 256, st, out1.data());
    r.resetLevels();
    r.process(mono.data(), 256, st, out2.data());
    CHECK(std::memcmp(out1.data(), out2.data(),
                      out1.size() * sizeof(float)) == 0);
  }

  // 10. Resampler identity
  {
    std::vector<float> src(128, 0.5f);
    std::vector<float> dst(128, 0.f);
    int n = resample(src.data(), 128, 48000, dst.data(), 128, 48000, 1);
    CHECK(n == 128);
    CHECK_NEAR(dst[0], 0.5f, 1e-6f);
  }

  // 11. Resampler up/down
  {
    std::vector<float> src(100);
    for (int i = 0; i < 100; ++i)
      src[static_cast<std::size_t>(i)] = std::sin(0.1f * i);
    auto dst = resampleTo(src, 100, 44100, 48000, 1);
    CHECK(dst.size() > 100);
    // energy roughly preserved
    float e0 = 0.f, e1 = 0.f;
    for (float v : src) e0 += v * v;
    for (float v : dst) e1 += v * v;
    CHECK(e1 > 0.5f * e0);
  }

  // 12. HOA rotation does not crash and preserves W
  {
    auto enc = encodeAmbisonics(45.f, 10.f, ChannelFormat::AmbisonicsFOA);
    std::vector<float> buf = enc;
    HoaRotation rot;
    rot.yaw = 0.5f;
    rotateHoa(buf.data(), 4, 1, rot);
    CHECK_NEAR(buf[0], 1.f, 1e-5f);
  }

  // 13. HOA decode produces non-zero output
  {
    auto enc = encodeAmbisonics(30.f, 0.f, ChannelFormat::AmbisonicsFOA);
    auto decoded =
        decodeAmbisonics(enc.data(), 1, ChannelFormat::AmbisonicsFOA,
                         ChannelFormat::Stereo);
    CHECK(decoded.size() == 2);
    float energy = decoded[0]*decoded[0] + decoded[1]*decoded[1];
    CHECK(energy > 1e-6f);
  }

  // 14. JSON levels writer
  {
    std::vector<float> peaks = {0.5f, 0.3f};
    std::vector<float> rms = {0.2f, 0.1f};
    writeLevelsJson("test_levels.json", peaks, rms);
    std::ifstream ifs("test_levels.json");
    CHECK(ifs.good());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    CHECK(content.find("\"peak\"") != std::string::npos);
    std::remove("test_levels.json");
  }

  std::cout << "All immersive tests passed.\n";
  return 0;
}
