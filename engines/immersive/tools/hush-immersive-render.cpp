// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/adm.hpp"
#include "hush/immersive/binaural.hpp"
#include "hush/immersive/channel_format.hpp"
#include "hush/immersive/json_util.hpp"
#include "hush/immersive/panner.hpp"
#include "hush/immersive/renderer.hpp"
#include "hush/immersive/resample.hpp"
#include "hush/immersive/speakers.hpp"
#include "hush/immersive/transport.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace hush::immersive;

namespace {

struct SceneBed {
  std::string path;
  float gainDb{0.f};
};

struct SceneObject {
  std::string name;
  std::string path;
  PannerState panner;
};

struct Scene {
  ChannelFormat format{ChannelFormat::SevenPointOne};
  SceneBed bed;
  std::vector<SceneObject> objects;
};

void printHelp() {
  std::cout
      << "hush-immersive-render <scene.txt|scene.json> --out <out.wav>\n"
      << "    [--binaural <sofa.sofa>] [--adm <out.w64>]\n"
      << "    [--levels <levels.json>] [--stream <host:port>]\n"
      << "\n"
      << "Scene text grammar:\n"
      << "  format <ChannelFormatName>\n"
      << "  bed <source.wav> gain <dB>\n"
      << "  object <name> <source.wav> az <deg> el <deg>\n"
      << "         [dist <m>] [div <0..1>] [size <0..1>]\n"
      << "         [anchor <0..1>] gain <dB>\n"
      << "\n"
      << "JSON scene is also accepted (see README).\n"
      << "Sample-rate mismatches between bed/objects are automatically\n"
      << "resampled with cubic interpolation.\n";
}

bool readWav(const std::string& path, std::vector<float>& samples,
             int& sampleRate, int& numChannels, int& numFrames) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return false;

  char riff[4];
  ifs.read(riff, 4);
  if (std::string(riff, 4) != "RIFF") return false;
  std::uint32_t riffSize = 0;
  ifs.read(reinterpret_cast<char*>(&riffSize), 4);
  char wave[4];
  ifs.read(wave, 4);
  if (std::string(wave, 4) != "WAVE") return false;

  std::uint16_t audioFormat = 0, nCh = 0, bits = 0;
  std::uint32_t sr = 0, dataSize = 0;
  bool foundFmt = false, foundData = false;
  std::streampos dataPos;

  while (ifs && (!foundFmt || !foundData)) {
    char id[4];
    ifs.read(id, 4);
    std::uint32_t sz = 0;
    ifs.read(reinterpret_cast<char*>(&sz), 4);
    if (std::string(id, 4) == "fmt ") {
      ifs.read(reinterpret_cast<char*>(&audioFormat), 2);
      ifs.read(reinterpret_cast<char*>(&nCh), 2);
      ifs.read(reinterpret_cast<char*>(&sr), 4);
      std::uint32_t byteRate = 0;
      ifs.read(reinterpret_cast<char*>(&byteRate), 4);
      std::uint16_t blockAlign = 0;
      ifs.read(reinterpret_cast<char*>(&blockAlign), 2);
      ifs.read(reinterpret_cast<char*>(&bits), 2);
      if (sz > 16) ifs.seekg(sz - 16, std::ios::cur);
      foundFmt = true;
    } else if (std::string(id, 4) == "data") {
      dataSize = sz;
      dataPos = ifs.tellg();
      ifs.seekg(sz, std::ios::cur);
      foundData = true;
    } else {
      ifs.seekg(sz, std::ios::cur);
    }
  }
  if (!foundFmt || !foundData) return false;

  sampleRate = static_cast<int>(sr);
  numChannels = static_cast<int>(nCh);
  const int bytesPerSample = bits / 8;
  numFrames = static_cast<int>(dataSize / (bytesPerSample * nCh));

  ifs.seekg(dataPos);
  samples.resize(static_cast<std::size_t>(numFrames) * nCh);

  if (audioFormat == 3 && bits == 32) {
    ifs.read(reinterpret_cast<char*>(samples.data()),
             static_cast<std::streamsize>(dataSize));
  } else if (audioFormat == 1 && bits == 16) {
    std::vector<std::int16_t> tmp(static_cast<std::size_t>(numFrames) * nCh);
    ifs.read(reinterpret_cast<char*>(tmp.data()),
             static_cast<std::streamsize>(dataSize));
    for (std::size_t i = 0; i < tmp.size(); ++i)
      samples[i] = tmp[i] / 32768.f;
  } else {
    return false;
  }
  return true;
}

void writeWav(const std::string& path, const std::vector<float>& samples,
              int sampleRate, int numChannels, int numFrames) {
  std::ofstream ofs(path, std::ios::binary);
  if (!ofs) throw std::runtime_error("cannot write " + path);

  const std::uint32_t dataSize =
      static_cast<std::uint32_t>(numChannels * numFrames * sizeof(float));
  const std::uint32_t riffSize = 36 + dataSize;

  ofs.write("RIFF", 4);
  ofs.write(reinterpret_cast<const char*>(&riffSize), 4);
  ofs.write("WAVE", 4);
  ofs.write("fmt ", 4);
  std::uint32_t fmtSize = 16;
  ofs.write(reinterpret_cast<const char*>(&fmtSize), 4);
  std::uint16_t fmt = 3;
  ofs.write(reinterpret_cast<const char*>(&fmt), 2);
  std::uint16_t nCh = static_cast<std::uint16_t>(numChannels);
  ofs.write(reinterpret_cast<const char*>(&nCh), 2);
  std::uint32_t sr = static_cast<std::uint32_t>(sampleRate);
  ofs.write(reinterpret_cast<const char*>(&sr), 4);
  std::uint32_t byteRate = sr * nCh * 4;
  ofs.write(reinterpret_cast<const char*>(&byteRate), 4);
  std::uint16_t blockAlign = static_cast<std::uint16_t>(nCh * 4);
  ofs.write(reinterpret_cast<const char*>(&blockAlign), 2);
  std::uint16_t bits = 32;
  ofs.write(reinterpret_cast<const char*>(&bits), 2);
  ofs.write("data", 4);
  ofs.write(reinterpret_cast<const char*>(&dataSize), 4);
  ofs.write(reinterpret_cast<const char*>(samples.data()), dataSize);
}

Scene parseSceneText(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs) throw std::runtime_error("cannot open scene: " + path);

  Scene scene;
  std::string line;
  while (std::getline(ifs, line)) {
    auto hash = line.find('#');
    if (hash != std::string::npos) line.resize(hash);
    std::istringstream iss(line);
    std::string tok;
    if (!(iss >> tok)) continue;

    if (tok == "format") {
      std::string name;
      iss >> name;
      scene.format = channelFormatFromName(name);
    } else if (tok == "bed") {
      iss >> scene.bed.path;
      std::string g;
      iss >> g;
      if (g == "gain") iss >> scene.bed.gainDb;
    } else if (tok == "object") {
      SceneObject obj;
      iss >> obj.name >> obj.path;
      obj.panner.isObject = true;
      std::string key;
      while (iss >> key) {
        if (key == "az")
          iss >> obj.panner.azimuthDeg;
        else if (key == "el")
          iss >> obj.panner.elevationDeg;
        else if (key == "dist")
          iss >> obj.panner.distance;
        else if (key == "div")
          iss >> obj.panner.divergence;
        else if (key == "size")
          iss >> obj.panner.size;
        else if (key == "anchor")
          iss >> obj.panner.screenAnchor;
        else if (key == "gain") {
          float db = 0.f;
          iss >> db;
          obj.panner.gain = std::pow(10.f, db / 20.f);
        }
      }
      scene.objects.push_back(obj);
    }
  }
  return scene;
}

Scene loadScene(const std::string& path) {
  if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") {
    JsonScene js = parseSceneJson(path);
    Scene s;
    s.format = js.format;
    s.bed.path = js.bedPath;
    s.bed.gainDb = js.bedGainDb;
    for (const auto& o : js.objects) {
      SceneObject so;
      so.name = o.name;
      so.path = o.path;
      so.panner = o.panner;
      s.objects.push_back(so);
    }
    return s;
  }
  return parseSceneText(path);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    printHelp();
    return 1;
  }
  std::string arg1 = argv[1];
  if (arg1 == "--help" || arg1 == "-h") {
    printHelp();
    return 0;
  }

  std::string scenePath = arg1;
  std::string outWav, sofaPath, admPath, levelsPath, streamTarget;

  for (int i = 2; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--out" && i + 1 < argc)
      outWav = argv[++i];
    else if (a == "--binaural" && i + 1 < argc)
      sofaPath = argv[++i];
    else if (a == "--adm" && i + 1 < argc)
      admPath = argv[++i];
    else if (a == "--levels" && i + 1 < argc)
      levelsPath = argv[++i];
    else if (a == "--stream" && i + 1 < argc)
      streamTarget = argv[++i];
  }

  if (outWav.empty()) {
    std::cerr << "error: --out is required\n";
    return 1;
  }

  try {
    Scene scene = loadScene(scenePath);
    const int nCh = channelCount(scene.format);
    int targetSr = 48000;

    // Load bed (with possible resampling)
    std::vector<float> bedSamples;
    int bedCh = 1, bedFrames = 0;
    if (!scene.bed.path.empty()) {
      int bedSr = 0;
      if (!readWav(scene.bed.path, bedSamples, bedSr, bedCh, bedFrames)) {
        throw std::runtime_error("failed to read bed: " + scene.bed.path);
      }
      targetSr = bedSr;
      if (bedSr != targetSr) {
        // already equal; keep for clarity
      }
    }

    // Objects – resample to targetSr if needed
    std::vector<std::vector<float>> objSamples(scene.objects.size());
    std::vector<int> objFrames(scene.objects.size(), 0);
    int totalFrames = bedFrames;

    for (std::size_t i = 0; i < scene.objects.size(); ++i) {
      int oSr = 0, oCh = 0, oFr = 0;
      std::vector<float> raw;
      if (!readWav(scene.objects[i].path, raw, oSr, oCh, oFr)) {
        throw std::runtime_error("failed to read object: " +
                                 scene.objects[i].path);
      }
      if (oSr != targetSr && targetSr > 0) {
        raw = resampleTo(raw, oFr, oSr, targetSr, oCh);
        oFr = static_cast<int>(raw.size() / oCh);
      } else if (bedFrames == 0) {
        targetSr = oSr;
      }
      // Extract mono (first channel)
      objSamples[i].resize(static_cast<std::size_t>(oFr));
      for (int f = 0; f < oFr; ++f)
        objSamples[i][static_cast<std::size_t>(f)] =
            raw[static_cast<std::size_t>(f * oCh)];
      objFrames[i] = oFr;
      totalFrames = std::max(totalFrames, oFr);
    }
    if (totalFrames == 0) totalFrames = 1024;

    // Resample bed if it was loaded at a different rate (edge case)
    if (!bedSamples.empty() && bedCh > 0) {
      // already at targetSr by construction above
    }

    std::vector<float> mix(static_cast<std::size_t>(totalFrames) * nCh, 0.f);
    Renderer renderer(scene.format, targetSr);

    if (!bedSamples.empty()) {
      const float bedGain = std::pow(10.f, scene.bed.gainDb / 20.f);
      if (bedCh == nCh) {
        for (int f = 0; f < bedFrames; ++f)
          for (int c = 0; c < nCh; ++c)
            mix[static_cast<std::size_t>(f * nCh + c)] +=
                bedSamples[static_cast<std::size_t>(f * bedCh + c)] * bedGain;
      } else {
        PannerState bedPan;
        bedPan.gain = bedGain;
        renderer.process(bedSamples.data(), bedFrames, bedPan, mix.data());
      }
    }

    for (std::size_t i = 0; i < scene.objects.size(); ++i) {
      renderer.process(objSamples[i].data(), objFrames[i],
                       scene.objects[i].panner, mix.data());
    }

    std::vector<float> finalOut = mix;
    int outCh = nCh;
    bool wantBinaural = false;
    for (int i = 1; i < argc; ++i)
      if (std::string(argv[i]) == "--binaural") wantBinaural = true;

    if (wantBinaural) {
      BinauralRenderer br(sofaPath);
      std::vector<float> stereo(static_cast<std::size_t>(totalFrames) * 2);
      br.process(mix.data(), nCh, totalFrames, stereo.data());
      finalOut = std::move(stereo);
      outCh = 2;
    }

    writeWav(outWav, finalOut, targetSr, outCh, totalFrames);
    std::cout << "Wrote " << outWav << " (" << outCh << " ch, " << totalFrames
              << " frames @ " << targetSr << " Hz)\n";

    if (!admPath.empty()) {
      std::vector<AdmObject> admObjs;
      for (const auto& o : scene.objects) {
        AdmObject ao;
        ao.name = o.name;
        ao.panner = o.panner;
        ao.firstChannel = 0;
        ao.numChannels = 1;
        admObjs.push_back(ao);
      }
      exportAdm(admPath, mix, nCh, totalFrames, targetSr, scene.format,
                admObjs);
      std::cout << "Wrote ADM " << admPath << "\n";
    }

    if (!levelsPath.empty()) {
      const auto& lv = renderer.levels();
      std::vector<float> peaks, rms;
      for (const auto& l : lv) {
        peaks.push_back(l.peak);
        rms.push_back(l.rms);
      }
      writeLevelsJson(levelsPath, peaks, rms);
    }

    // Optional real-time stream
    if (!streamTarget.empty()) {
      auto colon = streamTarget.find(':');
      if (colon == std::string::npos)
        throw std::runtime_error("--stream expects host:port");
      std::string host = streamTarget.substr(0, colon);
      int port = std::stoi(streamTarget.substr(colon + 1));
      UdpSender sender(host, port);
      const int block = 256;
      for (int pos = 0; pos < totalFrames; pos += block) {
        const int n = std::min(block, totalFrames - pos);
        if (!sender.send(finalOut.data() + pos * outCh, outCh, n, targetSr)) {
          std::cerr << "stream send failed at frame " << pos << "\n";
          break;
        }
      }
      std::cout << "Streamed " << totalFrames << " frames to " << streamTarget
                << "\n";
    }
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
