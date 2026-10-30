// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/json_util.hpp"
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace hush {
namespace immersive {

namespace {

std::string readFile(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs) throw std::runtime_error("cannot open JSON: " + path);
  std::ostringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

void skipWs(const std::string& s, std::size_t& i) {
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

std::string parseString(const std::string& s, std::size_t& i) {
  skipWs(s, i);
  if (i >= s.size() || s[i] != '"')
    throw std::runtime_error("JSON: expected string");
  ++i;
  std::string out;
  while (i < s.size() && s[i] != '"') {
    if (s[i] == '\\' && i + 1 < s.size()) {
      ++i;
      out.push_back(s[i]);
    } else {
      out.push_back(s[i]);
    }
    ++i;
  }
  if (i >= s.size()) throw std::runtime_error("JSON: unterminated string");
  ++i;
  return out;
}

double parseNumber(const std::string& s, std::size_t& i) {
  skipWs(s, i);
  std::size_t start = i;
  if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
  while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) ||
                          s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
                          s[i] == '+' || s[i] == '-'))
    ++i;
  return std::stod(s.substr(start, i - start));
}

void expect(const std::string& s, std::size_t& i, char c) {
  skipWs(s, i);
  if (i >= s.size() || s[i] != c)
    throw std::runtime_error(std::string("JSON: expected '") + c + "'");
  ++i;
}

}  // namespace

JsonScene parseSceneJson(const std::string& path) {
  const std::string s = readFile(path);
  std::size_t i = 0;
  JsonScene scene;
  expect(s, i, '{');

  while (true) {
    skipWs(s, i);
    if (i < s.size() && s[i] == '}') {
      ++i;
      break;
    }
    std::string key = parseString(s, i);
    expect(s, i, ':');
    if (key == "format") {
      scene.format = channelFormatFromName(parseString(s, i));
    } else if (key == "bed") {
      expect(s, i, '{');
      while (true) {
        skipWs(s, i);
        if (i < s.size() && s[i] == '}') {
          ++i;
          break;
        }
        std::string k2 = parseString(s, i);
        expect(s, i, ':');
        if (k2 == "path")
          scene.bedPath = parseString(s, i);
        else if (k2 == "gainDb")
          scene.bedGainDb = static_cast<float>(parseNumber(s, i));
        else
          throw std::runtime_error("JSON: unknown bed key");
        skipWs(s, i);
        if (i < s.size() && s[i] == ',') ++i;
      }
    } else if (key == "objects") {
      expect(s, i, '[');
      while (true) {
        skipWs(s, i);
        if (i < s.size() && s[i] == ']') {
          ++i;
          break;
        }
        expect(s, i, '{');
        JsonSceneObject obj;
        obj.panner.isObject = true;
        while (true) {
          skipWs(s, i);
          if (i < s.size() && s[i] == '}') {
            ++i;
            break;
          }
          std::string k2 = parseString(s, i);
          expect(s, i, ':');
          if (k2 == "name")
            obj.name = parseString(s, i);
          else if (k2 == "path")
            obj.path = parseString(s, i);
          else if (k2 == "az")
            obj.panner.azimuthDeg = static_cast<float>(parseNumber(s, i));
          else if (k2 == "el")
            obj.panner.elevationDeg = static_cast<float>(parseNumber(s, i));
          else if (k2 == "dist")
            obj.panner.distance = static_cast<float>(parseNumber(s, i));
          else if (k2 == "div")
            obj.panner.divergence = static_cast<float>(parseNumber(s, i));
          else if (k2 == "size")
            obj.panner.size = static_cast<float>(parseNumber(s, i));
          else if (k2 == "anchor")
            obj.panner.screenAnchor = static_cast<float>(parseNumber(s, i));
          else if (k2 == "gainDb") {
            float db = static_cast<float>(parseNumber(s, i));
            obj.panner.gain = std::pow(10.f, db / 20.f);
          } else
            throw std::runtime_error("JSON: unknown object key");
          skipWs(s, i);
          if (i < s.size() && s[i] == ',') ++i;
        }
        scene.objects.push_back(obj);
        skipWs(s, i);
        if (i < s.size() && s[i] == ',') ++i;
      }
    } else {
      throw std::runtime_error("JSON: unknown top-level key: " + key);
    }
    skipWs(s, i);
    if (i < s.size() && s[i] == ',') ++i;
  }
  return scene;
}

void writeLevelsJson(const std::string& path, const std::vector<float>& peaks,
                     const std::vector<float>& rms) {
  std::ofstream ofs(path);
  if (!ofs) throw std::runtime_error("cannot write levels: " + path);
  ofs << "{\n  \"channels\": [\n";
  const std::size_t n = peaks.size();
  for (std::size_t i = 0; i < n; ++i) {
    ofs << "    {\"peak\": " << peaks[i] << ", \"rms\": "
        << (i < rms.size() ? rms[i] : 0.f) << "}";
    if (i + 1 < n) ofs << ",";
    ofs << "\n";
  }
  ofs << "  ]\n}\n";
}

std::string pannerToJson(const PannerState& p) {
  std::ostringstream ss;
  ss << "{\"az\":" << p.azimuthDeg << ",\"el\":" << p.elevationDeg
     << ",\"dist\":" << p.distance << ",\"div\":" << p.divergence
     << ",\"size\":" << p.size << ",\"anchor\":" << p.screenAnchor
     << ",\"gain\":" << p.gain << ",\"isObject\":"
     << (p.isObject ? "true" : "false") << "}";
  return ss.str();
}

}  // namespace immersive
}  // namespace hush
